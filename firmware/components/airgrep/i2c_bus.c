#include "i2c_bus.h"

#include "board.h"

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

i2c_master_bus_handle_t i2c_bus_handle(void) { return s_bus; }

esp_err_t i2c_bus_add_if_present(const i2c_device_config_t* config,
								 i2c_master_dev_handle_t* device) {
	if (!s_bus) {
		return ESP_ERR_INVALID_STATE;
	}
	if (*device) {
		return ESP_OK;
	}
	// The I3C broadcast would switch the BMP580 away from I2C.
	if (config->device_address == 0x7e) {
		return ESP_ERR_INVALID_ARG;
	}
	if (i2c_master_probe(s_bus, config->device_address,
						 CONFIG_AIRGREP_I2C_TIMEOUT_MS) != ESP_OK) {
		return ESP_ERR_NOT_FOUND;
	}
	return i2c_master_bus_add_device(s_bus, config, device);
}
