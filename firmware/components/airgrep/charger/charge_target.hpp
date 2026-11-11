#pragma once

#include <cstdint>

namespace charger {
// ILIM_HIZ divider, docs/firmware.md: the input limit while EN_EXTILIM is set.
#define CHARGER_HARDWARE_INPUT_LIMIT_MA 1500

enum class ControlMode : uint8_t { automatic, manual };
// What the input target follows.
enum class Supply : uint8_t { none, waiting, usb_c, detection };
enum class ChargeStop : uint8_t {
	none,
	cold,
	hot,
	overvoltage,
	no_battery,
	locked
};

// What charge control decided.
struct TargetState {
	ControlMode mode = ControlMode::automatic;
	bool decided = false;  // false in MANUAL and before the first run
	Supply supply = Supply::none;
	uint8_t vbus_type = 0;   // VBUS_STAT behind a detection result
	int64_t waiting_us = 0;  // since VBUS appeared
	unsigned input_ma = 0;   // 0 leaves IINDPM to the charger's detection
	bool hardware_limit = true;
	unsigned charge_ma = 0;
	bool charge_enabled = false;
	ChargeStop stop = ChargeStop::none;
	unsigned thermal_limit_c = 0;
};
}  // namespace charger
