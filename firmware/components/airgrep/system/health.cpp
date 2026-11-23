#include "health.hpp"

#include <cstdio>

#include "device.h"

namespace health {
namespace {
bool overvoltage(const charger::Fault0& f0, const charger::Fault1& f1) {
	return f0.vbus_overvoltage || f0.battery_overvoltage ||
		   f0.ac1_overvoltage || f0.ac2_overvoltage || f1.system_overvoltage ||
		   f1.otg_overvoltage;
}

bool exhausted(size_t free, size_t total) { return free * 10 < total; }

bool needs_gauge(power_mode_t mode, const HealthInputs& s) {
	return !s.battery_absent &&
		   (mode == POWER_MODE_ON || mode == POWER_MODE_CHARGER);
}

// What the GNSS frame says. A missing fix is no cause, the receiver may
// still search.
uint16_t gnss_causes(const HealthInputs& s) {
	if (!state_usable(s.last_update.gnss_us, s.now_us)) {
		return kGnss;
	}
	return (s.gnss_antenna == adsbee::AntennaStatus::shorted ? kAntennaShort
															 : 0) |
		   (s.gnss_antenna == adsbee::AntennaStatus::open ? kAntennaOpen : 0) |
		   (s.gnss_bbr ? 0 : kBackupBattery);
}
}

uint16_t evaluate_health(power_mode_t mode, const HealthInputs& s) {
	if (mode == POWER_MODE_UNKNOWN) {
		return 0;
	}
	const bool on = mode == POWER_MODE_ON, off = mode == POWER_MODE_OFF;
	const auto usable = [&](int64_t updated_us) {
		return state_usable(updated_us, s.now_us);
	};
	uint16_t mask = 0;
	// In OFF without a pack the charger has no supply, so it counts only
	// when it answers.
	const bool charger_usable = usable(s.last_update.charger_us);
	if (!usable(s.last_update.expander_us) || (!off && !charger_usable)) {
		mask |= kRequiredI2cDeviceMissing;
	}
	if (charger_usable) {
		if (overvoltage(s.fault0, s.fault1)) {
			mask |= kChargerOvervoltage;
		}
		if (s.fault1.thermal_shutdown) {
			mask |= kChargerOverheated;
		}
	}
	if (needs_gauge(mode, s)) {
		if (!usable(s.last_update.gauge_us)) {
			mask |= kGauge;
		} else if (on && s.battery_low) {
			mask |= kBatteryLow;
		}
	}
	if (!off && !usable(s.last_update.usb_c_us)) {
		mask |= kUsbC;
	}
	if (on) {
		if (!usable(s.last_update.barometer_us)) {
			mask |= kBarometer;
		}
		if (!usable(s.last_update.adsbee_us)) {
			mask |= kAdsbee;
		} else {
			mask |= gnss_causes(s);
		}
	}
	if (s.target_decided && !s.battery_absent && (!off || charger_usable)) {
		if (s.stop == charger::ChargeStop::hot) {
			mask |= kBatteryOverheated;
		}
		if (s.stop == charger::ChargeStop::cold) {
			mask |= kBatteryCold;
		}
	}
	if (exhausted(s.internal_free, s.internal_total) ||
		exhausted(s.psram_free, s.psram_total)) {
		mask |= kMemory;
	}
	if (s.cpu_high) {
		mask |= kCpu;
	}
	return mask;
}

bool ready(power_mode_t mode, const HealthInputs& s) {
	const bool charging = s.expander_ready && s.charger_ready &&
						  (!needs_gauge(mode, s) || s.gauge_ready) &&
						  s.usb_c_ready;
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
	static const char* const names[] = {"i2c",
										"adsbee",
										"gnss",
										"antenna_short",
										"charger_overvoltage",
										"charger_overheated",
										"battery_overheated",
										"antenna_open",
										"backup_battery",
										"barometer",
										"gauge",
										"usb_c",
										"battery_low",
										"battery_cold",
										"memory",
										"cpu"};
	if (!mask) {
		(void)snprintf(out, size, "OK (0x0)");
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
		(void)snprintf(out + used, size - used, ")");
	}
}
}
