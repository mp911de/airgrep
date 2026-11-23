#pragma once

#include <cstdint>

#include "device.h"

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

// USB protocol 1 carries the value, a validity byte and three reserved bytes.
struct OptionalFloat {
	float value = 0;
	uint8_t valid = 0;
	uint8_t reserved[3]{};
};

// Hours until the pack is empty at a negative rate, until it is full at a
// positive one.
OptionalFloat calculate_charge_duration(float soc, float rate);

struct StateData {
	Status status = Status::unknown;
	uint16_t raw_vcell = 0, raw_soc = 0, raw_status = 0, raw_version = 0;
	uint16_t raw_crate = 0;
	float volts = 0, soc = 0, rate = 0;
	bool low = false;
	uint16_t version = 0;
	Alerts alerts{};
	OptionalFloat charge_duration_hours;
	int64_t updated_us = 0, error_us = 0;
	EspErrorCode last_error = 0;
	bool identity_known = false, configured = false;

	// The gauge sits on the pack side, so no gauge means no pack and a live
	// cell reading means one is fitted.
	BatteryDetected battery() const {
		if (status == Status::off) {
			return BatteryDetected::absent;
		}
		if (status == Status::ok && updated_us && updated_us >= error_us &&
			volts >= 2.5f) {
			return BatteryDetected::voltage_present;
		}
		return BatteryDetected::unknown;
	}
};
}
