#pragma once

#include <cstdint>
#include <limits>

#include "charger/charge_target.hpp"
#include "threshold.hpp"

namespace charge_control {
struct ChargeInputs {
	int64_t now_us = 0;
	charger::ControlMode mode = charger::ControlMode::automatic;
	bool charger_usable = false, vbus = false, detecting = false;
	unsigned detected_ma = 0;
	uint8_t detected_vbus_type = 0;
	float pack_c = std::numeric_limits<float>::quiet_NaN();
	unsigned vbat_mv = 0;              // 0 when not available
	bool battery_overvoltage = false;  // VBAT_OVP live or latched
	bool usb_c_usable = false, usb_c_negotiating = false;
	unsigned advertised_ma = 0;
	unsigned capacity_mah = 0;
};

class Rules {
   public:
	charger::TargetState apply_rules(const ChargeInputs& in);

   private:
	void decide_input_limit(const ChargeInputs& in,
							charger::TargetState& target);
	void decide_charge_current(const ChargeInputs& in,
							   charger::TargetState& target);
	charger::ChargeStop decide_charge_stop(const ChargeInputs& in);

	enum class Battery : uint8_t { ok, overvoltage, absent };

	Threshold warm_{33, 31}, warmer_{36, 34}, hot_{40, 38}, cold_{4, 6};
	bool temperature_seen_ = false, vbus_ = false;
	int64_t vbus_since_us_ = 0;
	Battery battery_ = Battery::ok;
	bool locked_ = false;
	unsigned failures_ = 0;
	int64_t last_overvoltage_us_ = 0;
};
}
