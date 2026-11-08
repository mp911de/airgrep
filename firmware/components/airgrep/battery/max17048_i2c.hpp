#pragma once

#include <cstdint>

namespace battery {
	float decode_vcell(uint16_t raw);
	float decode_soc(uint16_t raw);
	float decode_crate(uint16_t raw);

	struct Alerts {
		bool reset, voltage_high, voltage_low, voltage_reset, low_soc, soc_change;
	};

	Alerts decode_status(uint16_t raw);
	// STATUS value that clears RI and keeps every other alert bit.
	uint16_t acknowledge_reset(uint16_t raw_status);

	struct Remaining {
		bool valid;
		float hours;
	};

	// Rates below 1 percent per hour are too small for a useful estimate.
	Remaining remaining_time(float soc, float rate);
}
