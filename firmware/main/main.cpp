#include "adsbee/adsbee_link.hpp"
#include "baro/barometer.hpp"
#include "battery/max17048.hpp"
#include "buzzer/buzzer.h"
#include "charger/bq25798.hpp"
#include "console.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "i2c_expander/tca9534.hpp"
#include "usb/fusb303.hpp"
#include <cstdio>
#include "i2c_bus.h"
#include "loop.hpp"
#include "sdkconfig.h"
#include "settings/settings.hpp"
#include "tz/tz.hpp"
#include "wifi/wifi_ap.hpp"
#include "device_info.h"
#if CONFIG_AIRGREP_HWTEST
#include "hwtest.hpp"
#endif

static const char* TAG = "airgrep";

using Completion = loop::Callable<void(esp_err_t)>;

struct Component {
	const char* name;
	void (*init)(Completion complete);
};

static bool is_csbee(const adsbee::Message& m) {
	return m.kind == DECODER_CSBEE;
}

static void log_csbee(const adsbee::Message& m, void*) {
	ESP_LOGV(TAG, "csbee %s", m.text);
}

static void start_adsbee(Completion complete) {
	if (!adsbee::subscribe(is_csbee, log_csbee, nullptr)) {
		ESP_LOGW(TAG, "csbee subscriber not registered");
	}
	complete(adsbee::start());
}

// Boot order is the table order. The loop and the I/O ports come first, the expander last
// because buzzer, adsbee and wifi subscribe to its pins during their init.
static const Component components[] = {
	{"Event loop", [](Completion complete) { loop::start(); complete(ESP_OK); }},
	{"Settings", [](Completion complete) { complete(settings::init()); }},
	{"Time zones", [](Completion complete) { complete(tz::init()); }},
	{"I2C", [](Completion complete) { complete(i2c_bus_init()); }},
	{"UART", [](Completion complete) { complete(adsbee::uart_init()); }},
	{"ADSbee", start_adsbee},
	{
		"Wi-Fi",
		[](Completion complete) { complete(wifi::start(device_info()->device_id)); }
	},
	{"Charger", charger::init},
	{"Battery", battery::init},
	{"Baro", barometer::init},
	{"FUSB", fusb::init},
	{"Buzzer", [](Completion complete) { complete(buzzer_init()); }},
	{"I2CExpander", expander::init},
};

// Same label column as the banner rows above it.
#define BOOT_LABEL_WIDTH 16

// A row whose init blocked the boot for longer than this shows its time. Rows that complete
// later from the loop did not block and show none.
#define BOOT_SLOW_MS 50

static bool in_init;

static void report(const Component& component, esp_err_t result, int64_t started_us) {
	if (result == ESP_OK) std::printf("%-*s OK", BOOT_LABEL_WIDTH, component.name);
	else if (result == ESP_ERR_NOT_FOUND) std::printf("%-*s OFF", BOOT_LABEL_WIDTH, component.name);
	else std::printf("%-*s FAIL %s", BOOT_LABEL_WIDTH, component.name, esp_err_to_name(result));
	int64_t blocked_us = esp_timer_get_time() - started_us;
	if (in_init && blocked_us >= BOOT_SLOW_MS * 1000)
		std::printf(" (%lld ms)", (long long)(blocked_us / 1000));
	std::puts("");
}

extern "C" void app_main(void) {
	console_banner();
	for (const Component& component : components) {
		int64_t started_us = esp_timer_get_time();
		in_init = true;
		component.init([&component, started_us](esp_err_t outcome) {
			report(component, outcome, started_us);
		});
		in_init = false;
	}
#if CONFIG_AIRGREP_HWTEST
	hwtest::run();
#endif
	ESP_ERROR_CHECK(console_start());
}
