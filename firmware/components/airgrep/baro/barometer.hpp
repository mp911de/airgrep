#pragma once

#include <cstddef>
#include "bmp580_i2c.hpp"
#include "esp_err.h"
#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"
#include "loop.hpp"
#include "device.h"

namespace barometer {
#define BMP_I2C_ADDR 0x46
#define BMP_STARTUP_MS 4
// tstandby is 2.5 ms, rounded up to whole loop milliseconds.
#define BMP_STANDBY_MS 3
#define BMP_POLL_PERIOD_MS 1000
	// BMP580 datasheet: register map section 7, soft reset 4.3.10, timings table 4.
	constexpr uint8_t kRegChipId = 0x01;
	constexpr uint8_t kRegIntConfig = 0x14;
	constexpr uint8_t kRegTempDataXlsb = 0x1d;
	constexpr uint8_t kRegStatus = 0x28;
	constexpr uint8_t kRegDspIir = 0x31;
	constexpr uint8_t kRegOsrConfig = 0x36;
	constexpr uint8_t kRegOdrConfig = 0x37;
	constexpr uint8_t kRegCmd = 0x7e;
	constexpr uint8_t kChipIdBmp580 = 0x50;
	constexpr uint8_t kCmdSoftReset = 0xb6;
	constexpr uint8_t kIntOpenDrain = 1 << 2;
	constexpr uint8_t kIntEnable = 1 << 3;
	constexpr uint8_t kPowerModeStandby = 0;
	constexpr uint8_t kPowerModeNormal = 1;
	constexpr uint8_t kOdrRate1Hz = 0x1c;
	constexpr uint8_t kOdrConfigStandby1Hz = kOdrDeepDisable | kOdrRate1Hz <<
		kOdrRateShift |
		kPowerModeStandby;
	constexpr uint8_t kOdrConfigNormal1Hz = kOdrDeepDisable | kOdrRate1Hz << kOdrRateShift
		|
		kPowerModeNormal;

	const i2c_device_config_t i2c_config = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = BMP_I2C_ADDR,
		.scl_speed_hz = I2C_BUS_SPEED, .scl_wait_us = 0, .flags = {},
	};

	// State::data mirrors TEMP_DATA_XLSB..PRESS_DATA_MSB, registers 0x1D..0x22.
	static constexpr size_t kDataTemperatureOffset = 0;
	static constexpr size_t kDataPressureOffset = 3;
	static constexpr size_t kDataLength = 6;

	struct State {
		Status status = Status::unknown;
		uint8_t chip_id = 0, raw_status = 0, raw_odr = 0, raw_osr = 0, raw_iir = 0,
				raw_int = 0;
		uint8_t data[kDataLength]{};
		NvmStatus nvm{};
		Odr odr{};
		Osr osr{};
		Iir iir{};
		float pressure_hpa = 0, temperature_c = 0, altitude_m = 0;
		int64_t updated_us = 0, error_us = 0;
		esp_err_t last_error = ESP_OK;
		bool identity_known = false, configured = false;

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
	// Posts the soft reset. The outcome shows in State on the next poll.
	esp_err_t reset();
}
