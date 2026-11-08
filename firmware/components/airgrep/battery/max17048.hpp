#pragma once

#include "max17048_i2c.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "loop.hpp"
#include "i2c_bus.h"
#include "device.h"

namespace battery {
#define BATTERY_I2C_ADDR 0x36
#define BATTERY_POLL_PERIOD_MS 2000

	const i2c_device_config_t i2c_config = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = BATTERY_I2C_ADDR,
		.scl_speed_hz = I2C_BUS_SPEED, .scl_wait_us = 0, .flags = {},
	};

	struct State {
		Status status = Status::unknown;
		uint16_t raw_vcell = 0, raw_soc = 0, raw_crate = 0, raw_status = 0, raw_version = 0;
		float volts = 0, soc = 0, rate = 0;
		uint16_t version = 0;
		Alerts alerts{};
		Remaining remaining{};
		int64_t updated_us = 0, error_us = 0;
		esp_err_t last_error = ESP_OK;
		bool identity_known = false, configured = false;

		template <typename F> static void update(F f) {
			portENTER_CRITICAL(&mutex_);
			f(storage());
			portEXIT_CRITICAL(&mutex_);
		}
		static State copy();
	private:
		static State& storage();
		static portMUX_TYPE mutex_;
	};

	using Completion = loop::Callable<void(esp_err_t)>;
	void init(Completion complete);
	loop::Handle poll_handle();
}
