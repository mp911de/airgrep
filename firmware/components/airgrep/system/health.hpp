#pragma once

#include <cstddef>
#include <cstdint>

#include "adsbee/adsbee_protocol.hpp"
#include "charger/bq25798_i2c.hpp"
#include "charger/charge_target.hpp"
#include "power_mode.h"

namespace health {
enum : uint16_t {
	kRequiredI2cDeviceMissing = 1 << 0,
	kAdsbee = 1 << 1,
	kGnss = 1 << 2,
	kAntennaShort = 1 << 3,
	kChargerOvervoltage = 1 << 4,
	kChargerOverheated = 1 << 5,
	kBatteryOverheated = 1 << 6,
	kAntennaOpen = 1 << 7,
	kBackupBattery = 1 << 8,
	kBarometer = 1 << 9,
	kGauge = 1 << 10,
	kUsbC = 1 << 11,
	kBatteryLow = 1 << 12,
	kBatteryCold = 1 << 13,
	kMemory = 1 << 14,
	kCpu = 1 << 15,
};
constexpr uint16_t kFailures = kRequiredI2cDeviceMissing | kAdsbee | kGnss |
							   kAntennaShort | kChargerOvervoltage |
							   kChargerOverheated | kBatteryOverheated;

struct HealthInputs {
	int64_t now_us = 0;
	struct {
		int64_t expander_us = 0, charger_us = 0, gauge_us = 0, usb_c_us = 0,
				barometer_us = 0, adsbee_us = 0, gnss_us = 0;
	} last_update;
	bool expander_ready = false, charger_ready = false, gauge_ready = false,
		 usb_c_ready = false, barometer_ready = false, adsbee_ready = false;
	charger::Fault0 fault0{};
	charger::Fault1 fault1{};
	bool gnss_bbr = false;
	adsbee::AntennaStatus gnss_antenna = adsbee::AntennaStatus::unreported;
	bool target_decided = false;
	charger::ChargeStop stop = charger::ChargeStop::none;
	bool battery_low = false;
	// The carrier gauge measures the pack and the thermistor sits in it.
	bool battery_absent = false;
	size_t internal_free = 0, internal_total = 0, psram_free = 0,
		   psram_total = 0;
	bool cpu_high = false;
};

uint16_t evaluate_health(power_mode_t mode, const HealthInputs& s);
// True once every component the mode needs is ready, so a check after a mode
// change need not wait for the periodic one.
bool ready(power_mode_t mode, const HealthInputs& s);
// "OK (0x0)" or "ALERT (0x206 adsbee, gnss, barometer)".
void format(uint16_t mask, char* out, size_t size);
}
