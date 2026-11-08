// Copyright 2026 the original author or authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "bmp580_i2c.hpp"
#include <cmath>

namespace barometer {
	// ICAO standard atmosphere, troposphere: h = T0 / L * (1 - (p / p0) ^ (R * L / (g * M))).
	namespace isa {
		constexpr float sea_level_hpa = 1013.25f;
		constexpr float sea_level_k = 288.15f;
		constexpr float lapse_k_per_m = 0.0065f;
		constexpr float gas_constant = 8.31446f; // J / (mol K)
		constexpr float gravity = 9.80665f; // m / s^2
		constexpr float molar_mass_air = 0.0289644f; // kg / mol
		constexpr float exponent = gas_constant * lapse_k_per_m / (gravity *
			molar_mass_air);
		constexpr float scale_m = sea_level_k / lapse_k_per_m;
	} // namespace isa

	namespace {
		// Data registers hold signed 24-bit fixed point values.
		constexpr int32_t kData24SignBit = 1 << 23;
		constexpr int32_t kData24Range = 1 << 24;
		constexpr float kPressurePaPerLsb = 1.0f / 64;
		constexpr float kTemperatureCPerLsb = 1.0f / 65536;
		constexpr float kPaPerHPa = 100;

		int32_t signed24(uint8_t xlsb, uint8_t lsb, uint8_t msb) {
			int32_t raw = uint32_t(xlsb) | (uint32_t(lsb) << 8) | (uint32_t(msb) << 16);
			if (raw & kData24SignBit) raw -= kData24Range;
			return raw;
		}
	}

	float pressure_altitude(float hpa) {
		if (hpa <= 0) return NAN;
		return isa::scale_m * (1 - std::pow(hpa / isa::sea_level_hpa, isa::exponent));
	}

	Iir decode_iir(uint8_t raw) {
		return {(1u << (raw & kIirFieldMask)) - 1,
			(1u << ((raw >> kIirPressureShift) & kIirFieldMask)) - 1};
	}

	Osr decode_osr(uint8_t raw) {
		return {1u << (raw & kOsrFieldMask), 1u << ((raw >> kOsrPressureShift) & kOsrFieldMask),
			bool(raw & kOsrPressureEnable)};
	}

	Odr decode_odr(uint8_t raw) {
		return {unsigned(raw & kOdrPowerModeMask), unsigned((raw >> kOdrRateShift) & kOdrRateMask),
			bool(raw & kOdrDeepDisable)};
	}

	NvmStatus decode_status(uint8_t raw) {
		return {bool(raw & kStatusNvmReady), bool(raw & kStatusNvmError),
			bool(raw & kStatusNvmCommandError)};
	}

	float decode_temperature(uint8_t xlsb, uint8_t lsb, uint8_t msb) {
		return signed24(xlsb, lsb, msb) * kTemperatureCPerLsb;
	}

	float decode_pressure(uint8_t xlsb, uint8_t lsb, uint8_t msb) {
		return signed24(xlsb, lsb, msb) * kPressurePaPerLsb / kPaPerHPa;
	}
}
