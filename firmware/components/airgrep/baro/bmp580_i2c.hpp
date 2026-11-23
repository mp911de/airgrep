#pragma once

#include <cstddef>
#include <cstdint>

#include "device.h"

namespace barometer {
// Register field layout, BMP580 datasheet section 7.
static constexpr uint8_t kStatusNvmReady = 1 << 1;
static constexpr uint8_t kStatusNvmError = 1 << 2;
static constexpr uint8_t kStatusNvmCommandError = 1 << 3;
static constexpr uint8_t kOdrPowerModeMask = 0x03;
static constexpr unsigned kOdrRateShift = 2;
static constexpr uint8_t kOdrRateMask = 0x1f;
static constexpr uint8_t kOdrDeepDisable = 1 << 7;
static constexpr uint8_t kOsrFieldMask = 0x07;
static constexpr unsigned kOsrPressureShift = 3;
static constexpr uint8_t kOsrPressureEnable = 1 << 6;
static constexpr uint8_t kIirFieldMask = 0x07;
static constexpr unsigned kIirPressureShift = 3;

float decode_pressure(uint8_t xlsb, uint8_t lsb, uint8_t msb);
float decode_temperature(uint8_t xlsb, uint8_t lsb, uint8_t msb);

struct NvmStatus {
	bool ready, error, command_error;
};

NvmStatus decode_status(uint8_t raw);

struct Odr {
	unsigned mode, rate;
	bool deep_disabled;
};

Odr decode_odr(uint8_t raw);

struct Osr {
	unsigned temperature, pressure;
	bool pressure_enabled;
};

Osr decode_osr(uint8_t raw);

struct Iir {
	unsigned temperature, pressure;
};

Iir decode_iir(uint8_t raw);
float pressure_altitude(float hpa);

// State::data mirrors TEMP_DATA_XLSB..PRESS_DATA_MSB, registers 0x1D..0x22.
static constexpr size_t kDataTemperatureOffset = 0;
static constexpr size_t kDataPressureOffset = 3;
static constexpr size_t kDataLength = 6;

struct StateData {
	Status status = Status::unknown;
	uint8_t chip_id = 0, raw_status = 0, raw_odr = 0, raw_osr = 0, raw_iir = 0,
			raw_int = 0, raw_fifo = 0, raw_osr_eff = 0;
	uint8_t data[kDataLength]{};
	NvmStatus nvm{};
	Odr odr{};
	Osr osr{};
	Iir iir{};
	float pressure_hpa = 0, temperature_c = 0, altitude_m = 0;
	int64_t updated_us = 0, error_us = 0;
	EspErrorCode last_error = 0;
	bool identity_known = false, configured = false;
};
}
