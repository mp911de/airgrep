#pragma once

#include <cstdint>
#include <optional>

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

// Hours until the pack is empty at a negative rate, until it is full at a
// positive one.
std::optional<float> calculate_charge_duration(float soc, float rate);
}  // namespace battery
