#pragma once

#include "fusb303_i2c.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "loop.hpp"
#include "i2c_bus.h"
#include "device.h"

namespace fusb {
#define FUSB_I2C_ADDR 0x21
#define FUSB_POLL_PERIOD_MS 250
// tEN: first I2C access no earlier than 100 ms after VDD valid and EN_N low.
#define FUSB_ENABLE_DELAY_MS 100

	const i2c_device_config_t i2c_config = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = FUSB_I2C_ADDR,
		.scl_speed_hz = I2C_BUS_SPEED, .scl_wait_us = 0, .flags = {},
	};

	struct State {
		Status status = Status::unknown;
		uint8_t raw_identity = 0, raw_status = 0, raw_type = 0, raw_control1 = 0;
		Identity identity{};
		PortStatus port{};
		Role role = Role::none;
		Control control{};
		unsigned cc_limit_ma = 0;
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
	// Posts the soft reset. The outcome shows in State on the next poll.
	esp_err_t reset();
}
