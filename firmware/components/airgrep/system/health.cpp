#include "health.hpp"

#include <cstdio>

#include "device.h"

namespace health {
namespace {
constexpr float kLowSocPercent = 10;

bool overvoltage(const charger::Fault0& f0, const charger::Fault1& f1) {
	return f0.vbus_overvoltage || f0.battery_overvoltage ||
		   f0.ac1_overvoltage || f0.ac2_overvoltage || f1.system_overvoltage ||
		   f1.otg_overvoltage;
}

// More than half of a region in use. A region without memory is fine.
bool crowded(size_t free, size_t total) { return free * 2 < total; }

bool gnss_problem(const HealthInputs& s) {
	return !s.gnss_fix || !s.gnss_utc || !s.gnss_bbr ||
		   !state_usable(s.gnss_us, s.now_us);
}
}  // namespace

uint16_t evaluate_health(power_mode_t mode, const HealthInputs& s) {
	if (mode == POWER_MODE_UNKNOWN) {
		return 0;
	}
	const bool on = mode == POWER_MODE_ON, off = mode == POWER_MODE_OFF;
	auto usable = [&](int64_t updated_us) {
		return state_usable(updated_us, s.now_us);
	};
	uint16_t mask = 0;
	if (!usable(s.expander_us)) {
		mask |= kExpander;
	}
	// In OFF without a pack the charger has no supply, so it counts only
	// when it answers.
	const bool charger_usable = usable(s.charger_us);
	if (off ? charger_usable && overvoltage(s.fault0, s.fault1)
			: !charger_usable || overvoltage(s.fault0, s.fault1)) {
		mask |= kCharger;
	}
	if (!off) {
		if (!usable(s.gauge_us)) {
			mask |= kGauge;
		}
		if (!usable(s.usb_c_us)) {
			mask |= kUsbC;
		}
	}
	if (on) {
		if (!usable(s.barometer_us)) {
			mask |= kBarometer;
		}
		if (!usable(s.adsbee_us)) {
			mask |= kAdsbee;
		} else if (gnss_problem(s)) {
			mask |= kGnss;
		}
	}
	const bool decided = s.target_decided && (!off || charger_usable);
	const bool thermal = s.stop == charger::ChargeStop::hot ||
						 s.stop == charger::ChargeStop::cold;
	const bool unchargeable = s.stop == charger::ChargeStop::no_battery ||
							  s.stop == charger::ChargeStop::locked;
	if ((decided && (thermal || (!off && unchargeable))) ||
		(on && usable(s.gauge_us) && s.soc <= kLowSocPercent)) {
		mask |= kBattery;
	}
	if (crowded(s.internal_free, s.internal_total) ||
		crowded(s.psram_free, s.psram_total)) {
		mask |= kMemory;
	}
	if (s.cpu_high) {
		mask |= kCpu;
	}
	return mask;
}

bool ready(power_mode_t mode, const HealthInputs& s) {
	const bool charging =
		s.expander_ready && s.charger_ready && s.gauge_ready && s.usb_c_ready;
	switch (mode) {
		case POWER_MODE_ON:
			return charging && s.barometer_ready && s.adsbee_ready;
		case POWER_MODE_CHARGER:
			return charging;
		case POWER_MODE_OFF:
			return s.expander_ready;
		default:
			return false;
	}
}

void format(uint16_t mask, char* out, size_t size) {
	static const char* names[] = {"expander",  "charger", "gauge", "usb_c",
								  "barometer", "adsbee",  "gnss",  "battery",
								  "memory",    "cpu"};
	if (!mask) {
		snprintf(out, size, "OK (0x0)");
		return;
	}
	size_t used = snprintf(out, size, "ALERT (0x%X", mask);
	const char* separator = " ";
	for (size_t bit = 0; bit < sizeof names / sizeof *names; bit++) {
		if (mask & (1u << bit) && used < size) {
			used += snprintf(out + used, size - used, "%s%s", separator,
							 names[bit]);
			separator = ", ";
		}
	}
	if (used < size) {
		snprintf(out + used, size - used, ")");
	}
}
}  // namespace health
