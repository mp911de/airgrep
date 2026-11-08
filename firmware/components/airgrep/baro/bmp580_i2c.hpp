#pragma once

#include <cstdint>

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
}
