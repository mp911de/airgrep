#include "usb_device.hpp"

#include "sdkconfig.h"

#if CONFIG_TINYUSB_CDC_ENABLED
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED || \
	CONFIG_ESP_CONSOLE_USB_CDC || CONFIG_ESP_CONSOLE_UART
#error \
	"TinyUSB console requires ESP_CONSOLE_NONE and ESP_CONSOLE_SECONDARY_NONE"
#endif
#include <algorithm>
#include <cstdio>

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_pm.h"
#include "esp_private/usb_phy.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include "system/cpu_sampler.hpp"
#include "system/device_info.h"
#include "system/system.h"
#include "tinyusb.h"
#include "tinyusb_cdc_acm.h"
#include "tinyusb_default_config.h"
#include "usb_console.hpp"
#include "usb_protocol.hpp"
#if CONFIG_AIRGREP_USB_SERVICE_COMMANDS
#include "adsbee/adsbee_link.hpp"
#include "loop.hpp"
#include "usb_components.hpp"
#endif

namespace usb {
namespace {
using protocol::Command;
using protocol::Status;
constexpr uint8_t kData = 0;
constexpr uint16_t kDataInterface = 2;
constexpr size_t kFrameCapacity = UINT16_MAX;
constexpr size_t kTransferCapacity =
	std::min(CONFIG_TINYUSB_VENDOR_EPSIZE, CONFIG_TINYUSB_VENDOR_TX_BUFSIZE);
static_assert(CONFIG_TINYUSB_CDC_COUNT == 1);
static_assert(CONFIG_TINYUSB_VENDOR_COUNT == 1);
static_assert(CFG_TUD_VENDOR_TXRX_BUFFERED);
static_assert(kTransferCapacity >= 64);

SemaphoreHandle_t mutex;
TaskHandle_t control_task;
// Held while USB is mounted, also while the host suspends: the full-speed
// PHY of the S3 cannot wake from light sleep.
esp_pm_lock_handle_t sleep_lock;
bool mounted, reset_required, transmitting, shutting_down, work_pending,
	executing;
Command pending_action = Command::status, committed_action = Command::status;
int64_t sent_at;
uint32_t generation;
uint8_t *input, *response;
size_t input_size, response_size, response_offset;
#if CONFIG_AIRGREP_USB_SERVICE_COMMANDS
protocol::Decoder decoder{true};
#else
protocol::Decoder decoder;
#endif

struct Guard {
	Guard() { xSemaphoreTake(mutex, portMAX_DELAY); }
	~Guard() { xSemaphoreGive(mutex); }
	Guard(const Guard&) = delete;
	Guard& operator=(const Guard&) = delete;
	Guard(Guard&&) = delete;
	Guard& operator=(Guard&&) = delete;
};

void clear_input() { tud_vendor_n_read_flush(kData); }

void reset_session() {
	++generation;
	decoder.reset();
	input_size = response_size = response_offset = 0;
	work_pending = false;
	pending_action = Command::status;
	sent_at = 0;
#if CONFIG_AIRGREP_USB_SERVICE_COMMANDS
	adsbee::reset_ingestion();
#endif
	clear_input();
	tud_vendor_n_write_clear(kData);
	// An endpoint already handed to the controller must drain before reuse.
	// RESET_SESSION clears software FIFOs, not an in-flight USB transfer.
}

bool response_timed_out(int64_t now) {
	return sent_at && now - sent_at >= protocol::kTimeoutUs;
}

void expire() {
	reset_required = true;
	reset_session();
}

protocol::Hello hello() {
	protocol::Hello result{};
	const uint64_t now = esp_timer_get_time();
	result.uptime_seconds = now / 1000000;
	(void)std::snprintf(result.firmware_version, sizeof result.firmware_version,
						"%s", esp_app_get_description()->version);
	(void)std::snprintf(result.variant, sizeof result.variant, "%s",
						device_variant());
	device_firmware_format(result.firmware_description,
						   sizeof result.firmware_description);
	(void)std::snprintf(result.board, sizeof result.board, "%s",
						device_board_name());
	device_chip_format(result.chip, sizeof result.chip);
	constexpr uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
	constexpr uint32_t psram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
	result.internal_free = heap_caps_get_free_size(internal);
	result.internal_minimum = heap_caps_get_minimum_free_size(internal);
	result.internal_total = heap_caps_get_total_size(internal);
	result.psram_free = heap_caps_get_free_size(psram);
	result.psram_minimum = heap_caps_get_minimum_free_size(psram);
	result.psram_total = heap_caps_get_total_size(psram);
	result.health = system_health();
	result.mode = system_mode();
#if CONFIG_AIRGREP_USB_SERVICE_COMMANDS
	result.service = protocol::Service::present;
#else
	result.service = protocol::Service::absent;
#endif
	result.cpu_core0 = result.cpu_core1 = result.cpu_overall =
		AIRGREP_CPU_UNAVAILABLE;
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
	static cpu::Sampler window;
	const auto load = cpu::sample(window, now);
	result.cpu_core0 = load.cores[0];
	if constexpr (CONFIG_FREERTOS_NUMBER_OF_CORES > 1) {
		result.cpu_core1 = load.cores[1];
	}
	result.cpu_overall = load.overall;
#endif
	return result;
}

void send_chunk() {
	if (response_timed_out(esp_timer_get_time())) {
		expire();
		return;
	}
	size_t length =
		std::min(response_size - response_offset, kTransferCapacity);
	// Short final packets avoid TinyUSB's automatic ZLP and its extra
	// completion. Bulk transfer boundaries carry no protocol meaning.
	if (length % 64 == 0) {
		--length;
	}
	if (tud_vendor_n_write_available(kData) < length) {
		expire();
		return;
	}
	const size_t queued =
		tud_vendor_n_write(kData, response + response_offset, length);
	transmitting = queued != 0;
	if (queued != length) {
		if (queued) {
			tud_vendor_n_write_flush(kData);
		}
		expire();
		return;
	}
	response_offset += length;
	tud_vendor_n_write_flush(kData);
}

void send_response(size_t length) {
	work_pending = false;
	clear_input();
	if (!length || response_timed_out(esp_timer_get_time())) {
		expire();
		return;
	}
	response_size = length;
	response_offset = 0;
	send_chunk();
	if (response[6] == static_cast<uint8_t>(Status::invalid_length)) {
		reset_required = true;
	}
}

#if CONFIG_AIRGREP_USB_SERVICE_COMMANDS
void execute_request(uint32_t ticket, Command command, size_t payload_size) {
	int64_t received_at = 0;
	{
		const Guard guard;
		if (ticket != generation || !work_pending || shutting_down) {
			return;
		}
		if (response_timed_out(esp_timer_get_time())) {
			expire();
			return;
		}
		received_at = sent_at;
		work_pending = false;
		executing = true;
	}
	// Subscribers may print to the console. Keep USB servicing available while
	// they run, but retain both frame buffers until the operation finishes.
	const size_t length =
		protocol::execute_data(command, input + 6, payload_size, received_at,
							   component_services(), response, kFrameCapacity);
	const Guard guard;
	executing = false;
	if (ticket != generation || shutting_down) {
		adsbee::reset_ingestion();
		return;
	}
	send_response(length);
}
#endif

void reply() {
#if CONFIG_AIRGREP_USB_SERVICE_COMMANDS
	const auto command = decoder.command();
	const size_t payload_size =
		decoder.frame_length() >= 6 ? decoder.frame_length() - 6 : 0;
#endif
	sent_at = esp_timer_get_time();
	auto immediate =
		protocol::route(decoder, {response, kFrameCapacity}, hello);
	decoder.reset();
	input_size = 0;
	if (immediate) {
		pending_action = immediate->after_send;
		send_response(immediate->size);
		return;
	}
#if CONFIG_AIRGREP_USB_SERVICE_COMMANDS
	const uint32_t ticket = generation;
	work_pending = true;
	if (!loop::try_submit("USB command", [ticket, command, payload_size](
											 loop::Handle, loop::Value&) {
			execute_request(ticket, command, payload_size);
			return loop::Result::done;
		})) {
		send_response(protocol::encode_status(Status::internal_error,
											  {response, kFrameCapacity}));
	}
#else
	send_response(protocol::encode_status(Status::unknown_command,
										  {response, kFrameCapacity}));
#endif
}

void receive() {
	const Guard guard;
	if (shutting_down) {
		return;
	}
	if (!mounted || reset_required || transmitting || work_pending ||
		executing || committed_action != Command::status) {
		clear_input();
		return;
	}
	uint8_t bytes[64];
	size_t size = 0;
	while ((size = tud_vendor_n_read(kData, bytes, sizeof bytes)) != 0) {
		for (size_t i = 0; i < size; ++i) {
			const int64_t now = esp_timer_get_time();
			if (decoder.timed_out(now) || input_size == kFrameCapacity) {
				expire();
				return;
			}
			input[input_size++] = bytes[i];
			if (decoder.feed(bytes[i], now)) {
				reply();
				return;
			}
		}
	}
}

bool reset_control(uint8_t rhport, uint8_t stage,
				   const tusb_control_request_t* request) {
	if (request->bmRequestType != 0x41 ||
		request->bRequest != AIRGREP_USB_RESET_SESSION ||
		request->wValue != 0 || request->wIndex != kDataInterface ||
		request->wLength != 0) {
		return false;
	}
	if (stage != CONTROL_STAGE_SETUP) {
		return true;
	}
	const Guard guard;
	if (shutting_down) {
		return false;
	}
	reset_session();
	reset_required = false;
	return tud_control_status(rhport, request);
}

void device_event(tinyusb_event_t* event, void*) {
	usb_console_set_service_task();
	const Guard guard;
	if (shutting_down) {
		return;
	}
	if (event->id == TINYUSB_EVENT_ATTACHED) {
		// A bus reset may abort transfers without an intervening DETACHED
		// event.
		reset_session();
		transmitting = reset_required = false;
		if (!mounted) {
			ESP_ERROR_CHECK(esp_pm_lock_acquire(sleep_lock));
		}
		mounted = true;
	} else if (event->id == TINYUSB_EVENT_DETACHED && mounted) {
		reset_session();
		transmitting = reset_required = false;
		ESP_ERROR_CHECK(esp_pm_lock_release(sleep_lock));
		mounted = false;
	}
}

void restart(Command action) {
	{
		const Guard guard;
		shutting_down = true;
	}
	usb_console_stop();
	tud_disconnect();
	ESP_ERROR_CHECK(tinyusb_driver_uninstall());
	// Give the host an observable detach before the ROM/application enumerates.
	vTaskDelay(pdMS_TO_TICKS(100));
	if (action == Command::enter_bootloader) {
		usb_phy_config_t config{};
		config.controller = USB_PHY_CTRL_SERIAL_JTAG;
		config.target = USB_PHY_TARGET_INT;
		usb_phy_handle_t phy = nullptr;
		ESP_ERROR_CHECK(usb_new_phy(&config, &phy));
		REG_SET_BIT(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
	}
	esp_restart();
}

void control(void*) {
	for (;;) {
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
		Command action = Command::status;
		{
			const Guard guard;
			action = committed_action;
			const int64_t now = esp_timer_get_time();
			if (decoder.timed_out(now) || response_timed_out(now)) {
				expire();
			}
		}
		if (action == Command::reboot || action == Command::enter_bootloader) {
			restart(action);
		}
	}
}

void transmitted() {
	const Guard guard;
	if (shutting_down || !transmitting) {
		return;
	}
	transmitting = false;
	if (!response_size) {
		return;
	}
	if (response_timed_out(esp_timer_get_time())) {
		expire();
		return;
	}
	if (response_offset < response_size) {
		send_chunk();
		return;
	}
	response_size = response_offset = 0;
	sent_at = 0;
	if (!reset_required && (pending_action == Command::reboot ||
							pending_action == Command::enter_bootloader)) {
		committed_action = pending_action;
		xTaskNotifyGive(control_task);
	}
	pending_action = Command::status;
}
}

esp_err_t start() {
	input = static_cast<uint8_t*>(
		heap_caps_malloc(kFrameCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	response = static_cast<uint8_t*>(
		heap_caps_malloc(kFrameCapacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	if (!input || !response) {
		heap_caps_free(input);
		heap_caps_free(response);
		return ESP_ERR_NO_MEM;
	}
	mutex = xSemaphoreCreateMutex();
	if (!mutex) {
		return ESP_ERR_NO_MEM;
	}
	ESP_ERROR_CHECK(
		esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "usb", &sleep_lock));
	if (xTaskCreatePinnedToCore(control, "usb-control", 4096, nullptr, 5,
								&control_task, 0) != pdPASS) {
		return ESP_ERR_NO_MEM;
	}
	static const char language[] = {0x09, 0x04};

	static const char* strings[] = {language,
									CONFIG_TINYUSB_DESC_MANUFACTURER_STRING,
									device_info()->device_id,
									device_serial(),
									CONFIG_TINYUSB_DESC_CDC_STRING,
									"airgrep Data"};
	auto config = TINYUSB_DEFAULT_CONFIG(device_event);
	config.task.xCoreID = 0;
	config.descriptor.string = strings;
	config.descriptor.string_count = sizeof strings / sizeof strings[0];
	// The ROM's USB download keeps the USB controller out of reset across a
	// restart. The driver's reset of it then does nothing and the host keeps
	// the ROM's device until the cable is pulled. Espressif's Arduino core
	// clears the same two bits before it starts TinyUSB. Both are 0 after
	// any other start.
	REG_CLR_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_IO_MUX_RESET_DISABLE);
	REG_CLR_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_USB_RESET_DISABLE);
	ESP_ERROR_CHECK(tinyusb_driver_install(&config));
	tinyusb_config_cdcacm_t cdc{};
	cdc.cdc_port = TINYUSB_CDC_ACM_0;
	ESP_ERROR_CHECK(tinyusb_cdcacm_init(&cdc));
	return usb_console_init();
}
}

extern "C" void tud_vendor_rx_cb(uint8_t idx, const uint8_t*,
#if CFG_TUD_API_V0_19_COMPAT
								 uint16_t
#else
								 uint32_t
#endif
) {
	if (idx == 0) {
		usb::receive();
	}
}

extern "C" void tud_vendor_tx_cb(uint8_t idx, uint32_t sent_bytes) {
	if (idx == 0 && sent_bytes) {
		usb::transmitted();
	}
}

extern "C" bool tud_vendor_control_xfer_cb(
	uint8_t rhport, uint8_t stage, const tusb_control_request_t* request) {
	return usb::reset_control(rhport, stage, request);
}
#else
esp_err_t usb::start() { return ESP_OK; }
#endif
