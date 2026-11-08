#include "max17048_i2c.hpp"

namespace battery {
	namespace {
		// MAX17048 datasheet: LSb weights in register summary table 2, STATUS bits in figure 13.
		constexpr float kVcellVoltsPerLsb = 0.000078125f;
		constexpr float kSocPercentPerLsb = 1.0f / 256.0f;
		constexpr float kCratePercentPerHourPerLsb = 0.208f;
		constexpr float kRemainingDeadBandPercentPerHour = 1.0f;

		constexpr uint16_t kStatusRi = 1u << 8;
		constexpr uint16_t kStatusVh = 1u << 9;
		constexpr uint16_t kStatusVl = 1u << 10;
		constexpr uint16_t kStatusVr = 1u << 11;
		constexpr uint16_t kStatusHd = 1u << 12;
		constexpr uint16_t kStatusSc = 1u << 13;
	}

	float decode_vcell(uint16_t raw) { return raw * kVcellVoltsPerLsb; }
	float decode_soc(uint16_t raw) { return raw * kSocPercentPerLsb; }
	float decode_crate(uint16_t raw) { return static_cast<int16_t>(raw) * kCratePercentPerHourPerLsb; }
	Alerts decode_status(uint16_t raw) {
		return {bool(raw & kStatusRi), bool(raw & kStatusVh), bool(raw & kStatusVl),
			bool(raw & kStatusVr), bool(raw & kStatusHd), bool(raw & kStatusSc)};
	}
	uint16_t acknowledge_reset(uint16_t raw_status) { return raw_status & ~kStatusRi; }
	Remaining remaining_time(float soc, float rate) {
		if (!(soc >= 0 && soc <= 100) || (rate > -kRemainingDeadBandPercentPerHour && rate < kRemainingDeadBandPercentPerHour)) return {};
		return {true, rate < 0 ? soc / -rate : (100 - soc) / rate};
	}
}
