#include "i2c_bus.h"
#include "board.h"
#include "esp_log.h"

static const char* TAG = "i2c";
static i2c_master_bus_handle_t s_bus;

esp_err_t i2c_bus_init(void) {
	if (s_bus) {
		return ESP_OK;
	}
	const i2c_master_bus_config_t config = {
		.i2c_port = I2C_NUM_0,
		.sda_io_num = BOARD_PIN_SDA,
		.scl_io_num = BOARD_PIN_SCL,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = false,
	};
	return i2c_new_master_bus(&config, &s_bus);
}

i2c_master_bus_handle_t i2c_bus_handle(void) {
	return s_bus;
}

bool i2c_bus_present(uint8_t address) {
	return i2c_master_probe(s_bus, address, 20) == ESP_OK;
}

void i2c_bus_log_scan(void) {
	for (uint8_t a = I2C_BUS_SCAN_FIRST; a <= I2C_BUS_SCAN_LAST; a++) {
		if (i2c_bus_present(a)) {
			ESP_LOGI(TAG, "device at 0x%02x", a);
		}
	}
}
