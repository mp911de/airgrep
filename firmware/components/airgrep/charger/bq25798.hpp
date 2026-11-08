#pragma once

#include "bq25798_i2c.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "loop.hpp"
#include "i2c_bus.h"
#include "device.h"

namespace charger {
#define CHARGER_I2C_ADDR 0x6b
#define CHARGER_POLL_PERIOD_MS 2000
#define CHARGER_WATCHDOG_MS 20000

	const i2c_device_config_t i2c_config = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = CHARGER_I2C_ADDR,
		.scl_speed_hz = I2C_BUS_SPEED, .scl_wait_us = 0, .flags = {},
	};

	struct State {
		Status status = Status::unknown;
		uint8_t raw[0x49]{};
		Identity identity{};
		Status0 status0{}; // input limits, watchdog, power good, VBUS present
		Status1 status1{}; // charge phase, VBUS source type, BC1.2 done
		Status2 status2{}; // ICO, thermal regulation, D+/D- detection, VBAT present
		Status3 status3{}; // input FETs, ADC done, VSYS regulation, safety timers
		Status4 status4{}; // OTG battery low, TS temperature zones
		Fault0 fault0{}, fault_flag0{}; // overvoltage and overcurrent, live and latched
		Fault1 fault1{}, fault_flag1{}; // VSYS, OTG and thermal shutdown, live and latched
		Flag0 flag0{}; // latched status changes, poor source
		Control0 control0{}; // charge enable, HIZ, termination
		Control1 control1{}; // watchdog
		Control5 control5{}; // ship FET, IBAT sensing, external ILIM
		AdcControl adc_control{};
		unsigned charge_current_ma = 0, input_current_ma = 0, thermal_limit_c = 0;
		int ibus_ma = 0, ibat_ma = 0;
		unsigned vbus_mv = 0, vbat_mv = 0, vsys_mv = 0;
		float ts_percent = 0, die_c = 0;
		int64_t updated_us = 0, error_us = 0, adc_ready_us = 0;
		esp_err_t last_error = ESP_OK;
		bool identity_known = false, configured = false, adc_valid = false;

		template <typename F>
		static void update(F f) {
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
