#pragma once

#include <cstdint>

namespace charger {
// ILIM_HIZ divider, docs/firmware.md: the input limit while EN_EXTILIM is set.
#define CHARGER_HARDWARE_INPUT_LIMIT_MA 1500

enum class ControlMode : uint8_t { automatic, manual };
enum class Supply : uint8_t { none, waiting, usb_c, detection };
enum class ChargeStop : uint8_t {
	none,
	cold,
	hot,
	overvoltage,
	no_battery,
	locked
};

struct TargetState {
	ControlMode mode = ControlMode::automatic;
	bool target_available = false;
	Supply supply = Supply::none;
	uint8_t detected_vbus_type = 0;
	int64_t vbus_elapsed_us = 0;
	unsigned input_ma = 0;  // 0 leaves IINDPM to the charger's detection
	bool hardware_limit = true;
	unsigned charge_ma = 0;
	bool charge_enabled = false;
	ChargeStop stop = ChargeStop::none;
	unsigned thermal_limit_c = 0;
};
}
