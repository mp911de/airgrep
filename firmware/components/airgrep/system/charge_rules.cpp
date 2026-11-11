#include "charge_rules.hpp"

#include <algorithm>
#include <cmath>

namespace charge_control {
namespace {
constexpr int64_t kNegotiationTimeoutUs = 5000000;
constexpr unsigned kInputLimitMaxMa = 3300;  // IINDPM range maximum
// lower and upper bounds of charger power
constexpr unsigned kMinimumChargeMa = 500, kMaximumChargeMa = 2000;
constexpr unsigned kChargeStepMa = 10;

// thermal throttling
constexpr unsigned kWarmChargeMa = 1000, kWarmerChargeMa = 500;
constexpr unsigned kThermalLimitC = 100;

constexpr int64_t kMinuteUs = 60000000;
constexpr int64_t kOffUs = 5 * kMinuteUs, kFailureMemoryUs = 10 * kMinuteUs;

// battery/charger failure threshold
constexpr unsigned kFailureLatchThreshold = 10;

// Without a battery and with charging off, the charger holds BAT close to 2.5
// V.
constexpr unsigned kNoBatteryVBATThreshold = 2600;
}  // namespace

charger::TargetState Rules::apply_rules(const Inputs& in) {
	charger::TargetState target;
	target.mode = in.mode;
	target.decided = true;
	target.thermal_limit_c = kThermalLimitC;
	input(in, target);
	charge(in, target);
	return target;
}

void Rules::input(const Inputs& in, charger::TargetState& target) {
	const bool vbus = in.charger_usable && in.vbus;
	if (vbus && !vbus_) {
		vbus_since_us_ = in.now_us;
	}
	vbus_ = vbus;
	if (!vbus) {
		return;
	}
	target.waiting_us = in.now_us - vbus_since_us_;
	const bool negotiating = in.usb_c_usable && in.usb_c_negotiating;
	if ((in.detecting || negotiating) &&
		target.waiting_us < kNegotiationTimeoutUs) {
		target.supply = charger::Supply::waiting;
		return;
	}
	if (in.usb_c_usable && in.advertised_ma) {
		target.supply = charger::Supply::usb_c;
		target.input_ma = std::min(in.advertised_ma, kInputLimitMaxMa);
		target.hardware_limit =
			in.advertised_ma <= CHARGER_HARDWARE_INPUT_LIMIT_MA;
		return;
	}
	target.supply = charger::Supply::detection;
	target.vbus_type = in.vbus_type;
	target.input_ma = std::max(in.detected_ma, kMinimumChargeMa);
}

void Rules::charge(const Inputs& in, charger::TargetState& target) {
	target.charge_ma =
		std::clamp(in.capacity_mah / 2 / kChargeStepMa * kChargeStepMa,
				   kMinimumChargeMa, kMaximumChargeMa);
	if (std::isfinite(in.pack_c)) {
		temperature_seen_ = true;
		warm_.update(in.pack_c);
		warmer_.update(in.pack_c);
		hot_.update(in.pack_c);
		cold_.update(in.pack_c);
	}
	const charger::ChargeStop battery_stop = battery(in);
	if (!temperature_seen_) {
		target.charge_ma = kMinimumChargeMa;
		target.stop = battery_stop;
	} else {
		if (warmer_.tripped()) {
			target.charge_ma = std::min(target.charge_ma, kWarmerChargeMa);
		} else if (warm_.tripped()) {
			target.charge_ma = std::min(target.charge_ma, kWarmChargeMa);
		}
		target.stop = hot_.tripped()    ? charger::ChargeStop::hot
					  : cold_.tripped() ? charger::ChargeStop::cold
										: battery_stop;
	}
	target.charge_enabled = target.stop == charger::ChargeStop::none;
}

// Every VBAT_OVP that ends the ok state counts as one failure.
// Only a restart clears the lockout, because an opened
// pack protection reads like a removed pack.
charger::ChargeStop Rules::battery(const Inputs& in) {
	const int64_t now = in.now_us;
	// TODO: encapsulate timeout in method instead of spreading timeout checks
	// across the entire code base
	if (!locked_ && failures_ && now - failure_us_ > kFailureMemoryUs) {
		failures_ = 0;
	}
	if (in.charger_usable) {
		if (in.battery_overvoltage && battery_ == Battery::ok) {
			battery_ = Battery::overvoltage;
			off_since_us_ = failure_us_ = now;
			locked_ = ++failures_ >= kFailureLatchThreshold;
		} else if (battery_ == Battery::overvoltage &&
				   !in.battery_overvoltage && in.vbat_mv &&
				   now - off_since_us_ >= kOffUs) {
			battery_ = in.vbat_mv >= kNoBatteryVBATThreshold ? Battery::ok
															 : Battery::absent;
		} else if (battery_ == Battery::absent &&
				   in.vbat_mv >= kNoBatteryVBATThreshold) {
			battery_ = Battery::ok;
		}
	}
	if (locked_) {
		return charger::ChargeStop::locked;
	}
	switch (battery_) {
		case Battery::overvoltage:
			return charger::ChargeStop::overvoltage;
		case Battery::absent:
			return charger::ChargeStop::no_battery;
		default:
			return charger::ChargeStop::none;
	}
}
}  // namespace charge_control
