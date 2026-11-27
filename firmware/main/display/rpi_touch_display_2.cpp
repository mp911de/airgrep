#include <algorithm>

#include "display/display.hpp"
#include "driver/i2c_master.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "p4_maintenance.hpp"
#include "screen_mode.hpp"
#include "system/device_info.h"
#include "usb/usb_device.hpp"
#if CONFIG_AIRGREP_FPS_OVERLAY
#include "stats_overlay.hpp"
#endif

namespace {
constexpr char TAG[] = "touch-display-2";
constexpr int PANEL_W = 720, PANEL_H = 1280;
constexpr int LEFT = (PANEL_W - display::HEIGHT) / 2;
constexpr int TOP = (PANEL_H - display::WIDTH) / 2;
i2c_master_dev_handle_t touch;
i2c_master_dev_handle_t panel_mcu;
esp_lcd_panel_handle_t lcd;
static_assert(sizeof(lv_color_t) == 3);
lv_color_t* scanout;
display::ViewState model;

void delay_ms(unsigned ms) { vTaskDelay(pdMS_TO_TICKS(ms) + 1); }

esp_err_t add_device(i2c_master_bus_handle_t bus, uint8_t address,
					 i2c_master_dev_handle_t* device) {
	i2c_device_config_t config{};
	config.device_address = address;
	config.scl_speed_hz = 100000;
	return i2c_master_bus_add_device(bus, &config, device);
}

esp_err_t write_panel_reg(uint8_t reg, uint8_t value) {
	const uint8_t bytes[]{reg, value};
	return i2c_master_transmit(panel_mcu, bytes, sizeof bytes, 50);
}

void blank_backlight() { ESP_ERROR_CHECK_WITHOUT_ABORT(write_panel_reg(3, 0)); }

void read_touch(lv_indev_t*, lv_indev_data_t* data) {
	static lv_point_t point{};
	static bool down{};
	const uint8_t status_reg[]{0x81, 0x4e};
	uint8_t report[9]{};
	if (i2c_master_transmit_receive(touch, status_reg, sizeof status_reg,
									report, sizeof report, 20) != ESP_OK) {
		down = false;
	} else if (report[0] & 0x80) {
		const int x = report[2] | (report[3] << 8);
		const int y = report[4] | (report[5] << 8);
		down = (report[0] & 15) && x >= LEFT && x < LEFT + display::HEIGHT &&
			   y >= TOP && y < TOP + display::WIDTH;
		if (down) {
			point = {y - TOP, LEFT + display::HEIGHT - 1 - x};
		}
		const uint8_t ack[]{0x81, 0x4e, 0};
		if (i2c_master_transmit(touch, ack, sizeof ack, 20) != ESP_OK) {
			down = false;
		}
	}
	data->point = point;
	data->state = down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void flush(lv_display_t* panel, const lv_area_t* area, uint8_t* data) {
	const auto* pixels = reinterpret_cast<const lv_color16_t*>(data);
	const int stride = static_cast<int>(
		lv_display_get_buf_active(panel)->header.stride / sizeof(*pixels));
	const int x_start = LEFT + display::HEIGHT - 1 - area->y2;
	const int y_start = TOP + area->x1;
	const int columns = lv_area_get_width(area);
	const int rows = lv_area_get_height(area);
	for (int x = 0; x < columns; ++x) {
		auto* dest = scanout + (y_start + x) * PANEL_W + x_start;
		for (int y = rows - 1; y >= 0; --y) {
			*dest++ = lv_color16_to_color(pixels[y * stride + x]);
		}
	}
	ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(
		lcd, x_start, y_start, x_start + rows, y_start + columns, scanout));
	lv_display_flush_ready(panel);
}

struct Register {
	uint8_t command, value;
};
constexpr Register initialization[]{
#include "ili9881_5inch.inc"
};

esp_err_t reset_panel() {
	i2c_master_bus_config_t bus_config{};
	bus_config.i2c_port = I2C_NUM_0;
	bus_config.sda_io_num = GPIO_NUM_7;
	bus_config.scl_io_num = GPIO_NUM_8;
	bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
	bus_config.glitch_ignore_cnt = 7;
	i2c_master_bus_handle_t bus{};
	ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG, "I2C bus");

	esp_err_t detected = ESP_ERR_NOT_FOUND;
	for (unsigned attempt = 0; attempt < 20; ++attempt) {
		detected = i2c_master_probe(bus, 0x45, 50);
		if (detected == ESP_OK) {
			break;
		}
		delay_ms(100);
	}
	if (detected != ESP_OK) {
		ESP_LOGE(
			TAG,
			"I2C probe: onboard codec 0x18 %s, panel 0x45 %s, touch 0x5d %s",
			esp_err_to_name(i2c_master_probe(bus, 0x18, 50)),
			esp_err_to_name(detected),
			esp_err_to_name(i2c_master_probe(bus, 0x5d, 50)));
		return detected;
	}
	ESP_RETURN_ON_ERROR(add_device(bus, 0x45, &panel_mcu), TAG, "display MCU");
	constexpr uint8_t id_reg = 1;
	uint8_t id{};
	ESP_RETURN_ON_ERROR(i2c_master_transmit(panel_mcu, &id_reg, 1, 50), TAG,
						"check display power and ribbon");
	delay_ms(6);
	ESP_RETURN_ON_ERROR(i2c_master_receive(panel_mcu, &id, 1, 50), TAG,
						"MCU identification");
	ESP_RETURN_ON_FALSE((id & 15) == 8 || (id & 15) == 9, ESP_ERR_NOT_SUPPORTED,
						TAG, "unknown 5-inch panel ID 0x%02x", id);
	ESP_RETURN_ON_ERROR(write_panel_reg(3, 0), TAG, "backlight off");
	ESP_RETURN_ON_ERROR(write_panel_reg(2, 0), TAG, "assert reset");
	delay_ms(20);
	ESP_RETURN_ON_ERROR(write_panel_reg(2, 3), TAG,
						"release LCD and touch reset");
	delay_ms(50);
	ESP_RETURN_ON_ERROR(add_device(bus, 0x5d, &touch), TAG, "GT911");
	ESP_RETURN_ON_ERROR(i2c_master_probe(bus, 0x5d, 50), TAG, "probe GT911");
	ESP_LOGI(TAG, "Touch Display 2 revision 0x%02x and GT911 detected", id);
	return ESP_OK;
}
esp_err_t start_panel() {
	ESP_RETURN_ON_ERROR(reset_panel(), TAG, "panel reset");
	esp_ldo_channel_config_t power{};
	power.chan_id = 3;
	power.voltage_mv = 2500;
	esp_ldo_channel_handle_t rail{};
	ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&power, &rail), TAG,
						"DSI PHY power");
	esp_lcd_dsi_bus_config_t dsi_config{};
	dsi_config.num_data_lanes = 2;
	dsi_config.lane_bit_rate_mbps = 1000;
	esp_lcd_dsi_bus_handle_t dsi{};
	ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&dsi_config, &dsi), TAG,
						"DSI host");
	esp_lcd_panel_io_handle_t io{};
	esp_lcd_dbi_io_config_t dbi{};
	dbi.lcd_cmd_bits = 8;
	dbi.lcd_param_bits = 8;
	ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_dbi(dsi, &dbi, &io), TAG, "DCS");
	for (auto [command, value] : initialization) {
		const uint8_t page[]{0x98, 0x81, value};
		const bool select_page = command == 0xff;
		ESP_RETURN_ON_ERROR(
			esp_lcd_panel_io_tx_param(io, command, select_page ? page : &value,
									  select_page ? 3 : 1),
			TAG, "panel init");
	}
	const uint8_t zero = 0;
	ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x35, &zero, 1), TAG,
						"tear mode");
	ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x11, nullptr, 0), TAG,
						"sleep out");
	delay_ms(120);
	esp_lcd_dpi_panel_config_t dpi{};
	// Match the 240 MHz source divided by three to avoid horizontal timing
	// compensation.
	dpi.dpi_clock_freq_mhz = 80.0F;
	dpi.in_color_format = LCD_COLOR_FMT_RGB888;
	dpi.out_color_format = LCD_COLOR_FMT_RGB888;
	dpi.video_timing.h_size = PANEL_W;
	dpi.video_timing.v_size = PANEL_H;
	dpi.video_timing.hsync_front_porch = 110;
	dpi.video_timing.hsync_pulse_width = 2;
	dpi.video_timing.hsync_back_porch = 105;
	dpi.video_timing.vsync_front_porch = 100;
	dpi.video_timing.vsync_pulse_width = 2;
	dpi.video_timing.vsync_back_porch = 100;
	ESP_RETURN_ON_ERROR(esp_lcd_new_panel_dpi(dsi, &dpi, &lcd), TAG, "DPI");
	void* buffer{};
	ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(lcd, 1, &buffer),
						TAG, "scanout");
	scanout = static_cast<lv_color_t*>(buffer);
	std::fill_n(scanout, PANEL_W * PANEL_H, lv_color_t{});
	ESP_RETURN_ON_ERROR(
		esp_lcd_panel_draw_bitmap(lcd, 0, 0, PANEL_W, PANEL_H, scanout), TAG,
		"black margins");
	ESP_RETURN_ON_ERROR(esp_lcd_panel_init(lcd), TAG, "start scanout");
	ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x29, nullptr, 0), TAG,
						"display on");
	return esp_register_shutdown_handler(blank_backlight);
}
}  // namespace

static void run_display(void*) {
	if (start_panel() != ESP_OK) {
		vTaskDelete(nullptr);
		return;
	}
	lv_init();
	lv_tick_set_cb(
		[] { return static_cast<uint32_t>(esp_timer_get_time() / 1000); });
	auto* panel = lv_display_create(display::WIDTH, display::HEIGHT);
	constexpr size_t bytes = display::WIDTH * 32 * sizeof(uint16_t);
	void* pixels = heap_caps_aligned_alloc(
		LV_DRAW_BUF_ALIGN, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	if (!pixels) {
		pixels = heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, bytes,
										 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	}
	ESP_ERROR_CHECK(panel && pixels ? ESP_OK : ESP_ERR_NO_MEM);
	lv_display_set_color_format(panel, LV_COLOR_FORMAT_RGB565);
	lv_display_set_buffers(panel, pixels, nullptr, bytes,
						   LV_DISPLAY_RENDER_MODE_PARTIAL);
	lv_display_set_flush_cb(panel, flush);
	auto* input = lv_indev_create();
	ESP_ERROR_CHECK(input ? ESP_OK : ESP_ERR_NO_MEM);
	lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
	lv_indev_set_display(input, panel);
	lv_indev_set_read_cb(input, read_touch);
	display::Actions actions{};
	actions.set_dark_theme = [](bool dark) {
		model.dark_theme = dark;
		display::update(model);
	};
	actions.set_radar_range = [](uint8_t range) {
		model.radar_range_nm = range;
		display::update(model);
	};
	constexpr auto power =
		POWER_MODE_UNKNOWN;  // The Nano has no carrier power slider.
	model.mode = display::screen_mode(power, true);
	model.device_name = device_info()->device_id;
	model.version = esp_app_get_description()->version;
	display::create(panel, actions);
	display::update(model);
#if CONFIG_AIRGREP_FPS_OVERLAY
	display::attach_stats_overlay(panel);
#endif
	lv_refr_now(panel);
	ESP_ERROR_CHECK(write_panel_reg(3, 0x9f));
	ESP_LOGI(TAG, "Backlight on after first 800x480 frame");
	const uint32_t first_frame = lv_tick_get();
	for (;;) {
		if (model.mode == display::Mode::starting &&
			lv_tick_elaps(first_frame) >= 2000) {
			model.mode = display::screen_mode(power, false);
			display::update(model);
		}
		delay_ms(std::clamp<uint32_t>(lv_timer_handler(), 1, 20));
	}
}

extern "C" void app_main(void) {
	(void)device_info();
	ESP_ERROR_CHECK(xTaskCreate(run_display, "display", 16384, nullptr, 5,
								nullptr) == pdPASS
						? ESP_OK
						: ESP_ERR_NO_MEM);
	ESP_ERROR_CHECK_WITHOUT_ABORT(start_p4_maintenance());
	ESP_ERROR_CHECK_WITHOUT_ABORT(usb::start());
}
