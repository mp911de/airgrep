#pragma once

#include <cstddef>
#include <cstdint>

#include "charger/bq25798_i2c.hpp"
#include "charger/charge_target.hpp"
#include "power_mode.h"

// The health check: one bit per component the operating mode needs that is
// not usable. A bit says where to look, the diagnostic Commands say why.
namespace health {
enum : uint16_t {
	kExpander = 1 << 0,
	kCharger = 1 << 1,
	kGauge = 1 << 2,
	kUsbC = 1 << 3,
	kBarometer = 1 << 4,
	kAdsbee = 1 << 5,
	kGnss = 1 << 6,
	kBattery = 1 << 7,
	kMemory = 1 << 8,
	kCpu = 1 << 9,
};

// What the evaluation reads, copied from the driver States. A *_us field is
// the last successful update, 0 for never.
struct HealthInputs {
	int64_t now_us = 0;
	int64_t expander_us = 0, charger_us = 0, gauge_us = 0, usb_c_us = 0,
			barometer_us = 0, adsbee_us = 0, gnss_us = 0;
	bool expander_ready = false, charger_ready = false, gauge_ready = false,
		 usb_c_ready = false, barometer_ready = false, adsbee_ready = false;
	charger::Fault0 fault0{};
	charger::Fault1 fault1{};
	bool gnss_fix = false, gnss_utc = false, gnss_bbr = false;
	bool target_decided = false;  // false in MANUAL
	charger::ChargeStop stop = charger::ChargeStop::none;
	float soc = 0;  // percent
	size_t internal_free = 0, internal_total = 0, psram_free = 0,
		   psram_total = 0;
	bool cpu_high = false;  // stored by the periodic task
};

uint16_t evaluate_health(power_mode_t mode, const HealthInputs& s);
// True once every component the mode needs is ready, so a check after a mode
// change need not wait for the periodic one.
bool ready(power_mode_t mode, const HealthInputs& s);
// "OK (0x0)" or "ALERT (0x62 charger, adsbee, gnss)".
void format(uint16_t mask, char* out, size_t size);
}  // namespace health
