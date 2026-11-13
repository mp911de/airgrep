#include <initializer_list>

#include "system/health.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[health]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

using charger::ChargeStop;
using health::HealthInputs;

namespace {
constexpr int64_t kNow = 100000000;
constexpr power_mode_t kModes[] = {POWER_MODE_ON, POWER_MODE_CHARGER,
								   POWER_MODE_OFF};

// Every component fresh and fine, a decided target, a half-full pack.
HealthInputs fine() {
	HealthInputs s{};
	s.now_us = kNow;
	s.expander_us = s.charger_us = s.gauge_us = s.usb_c_us = s.barometer_us =
		s.adsbee_us = s.gnss_us = kNow;
	s.gnss_fix = s.gnss_utc = s.gnss_bbr = true;
	s.target_decided = true;
	s.stop = ChargeStop::none;
	s.soc = 50;
	s.internal_total = s.psram_total = 1000;
	s.internal_free = s.psram_free = 800;
	return s;
}

uint16_t only(power_mode_t mode, const HealthInputs& s) {
	return health::evaluate_health(mode, s);
}
}  // namespace

CASE(all_fine, "A fine snapshot is 0x0 in every mode") {
	for (power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(0, only(mode, fine()));
	}
}

CASE(unknown, "UNKNOWN mode is 0x0 whatever the snapshot") {
	HealthInputs s{};
	s.cpu_high = true;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_UNKNOWN, s));
}

CASE(expander, "A stale expander is flagged in every mode") {
	HealthInputs s = fine();
	s.expander_us = kNow - 5000001;
	for (power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(health::kExpander, only(mode, s));
	}
	s.expander_us = kNow - 5000000;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_ON, s));
}

CASE(charger_stale,
	 "A stale charger is flagged in ON and CHARGER, not in OFF") {
	HealthInputs s = fine();
	s.charger_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kCharger, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(health::kCharger, only(POWER_MODE_CHARGER, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
}

CASE(charger_overvoltage, "Any live overvoltage bit flags the charger") {
	for (power_mode_t mode : kModes) {
		HealthInputs s = fine();
		s.fault0.vbus_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kCharger, only(mode, s));
		s = fine();
		s.fault0.battery_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kCharger, only(mode, s));
		s = fine();
		s.fault0.ac1_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kCharger, only(mode, s));
		s = fine();
		s.fault0.ac2_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kCharger, only(mode, s));
		s = fine();
		s.fault1.system_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kCharger, only(mode, s));
		s = fine();
		s.fault1.otg_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kCharger, only(mode, s));
	}
	HealthInputs s = fine();
	s.fault0.bus_overcurrent = true;
	s.fault1.thermal_shutdown = true;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_ON, s));
}

CASE(gauge_and_usb_c, "Gauge and USB-C controller count in ON and CHARGER") {
	HealthInputs s = fine();
	s.gauge_us = 0;
	s.usb_c_us = 0;
	const uint16_t both = health::kGauge | health::kUsbC;
	TEST_ASSERT_EQUAL_HEX16(both, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(both, only(POWER_MODE_CHARGER, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
}

CASE(on_only, "Barometer and ADSBee link count in ON only") {
	HealthInputs s = fine();
	s.barometer_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kBarometer, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_CHARGER, s));
	s = fine();
	s.adsbee_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kAdsbee, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_CHARGER, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
}

CASE(gnss, "No fix, no UTC, no BBR or a stale frame flag GNSS in ON") {
	HealthInputs s = fine();
	s.gnss_fix = false;
	TEST_ASSERT_EQUAL_HEX16(health::kGnss, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_CHARGER, s));
	s = fine();
	s.gnss_utc = false;
	TEST_ASSERT_EQUAL_HEX16(health::kGnss, only(POWER_MODE_ON, s));
	s = fine();
	s.gnss_bbr = false;
	TEST_ASSERT_EQUAL_HEX16(health::kGnss, only(POWER_MODE_ON, s));
	s = fine();
	s.gnss_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kGnss, only(POWER_MODE_ON, s));
}

CASE(gnss_needs_adsbee, "Without the ADSBee link GNSS is not checked") {
	HealthInputs s = fine();
	s.adsbee_us = 0;
	s.gnss_fix = false;
	s.gnss_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kAdsbee, only(POWER_MODE_ON, s));
}

CASE(battery_temperature,
	 "A hot or cold stop flags the battery in every mode") {
	for (ChargeStop stop : {ChargeStop::hot, ChargeStop::cold}) {
		HealthInputs s = fine();
		s.stop = stop;
		for (power_mode_t mode : kModes) {
			TEST_ASSERT_EQUAL_HEX16(health::kBattery, only(mode, s));
		}
		s.charger_us = 0;
		TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
	}
}

CASE(battery_missing, "No battery or a lockout counts in ON and CHARGER only") {
	for (ChargeStop stop : {ChargeStop::no_battery, ChargeStop::locked}) {
		HealthInputs s = fine();
		s.stop = stop;
		TEST_ASSERT_EQUAL_HEX16(health::kBattery, only(POWER_MODE_ON, s));
		TEST_ASSERT_EQUAL_HEX16(health::kBattery, only(POWER_MODE_CHARGER, s));
		TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
	}
	HealthInputs s = fine();
	s.stop = ChargeStop::overvoltage;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_ON, s));
}

CASE(manual, "MANUAL skips the charge control battery checks") {
	HealthInputs s = fine();
	s.target_decided = false;
	s.stop = ChargeStop::hot;
	for (power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(0, only(mode, s));
	}
}

CASE(soc, "SoC at or below 10 % flags the battery in ON only") {
	HealthInputs s = fine();
	s.soc = 11;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_ON, s));
	s.soc = 10;
	TEST_ASSERT_EQUAL_HEX16(health::kBattery, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_CHARGER, s));
	s.gauge_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kGauge, only(POWER_MODE_ON, s));
}

CASE(memory, "More than 50 % used in either region flags memory") {
	HealthInputs s = fine();
	s.internal_free = 500;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
	s.internal_free = 499;
	for (power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(health::kMemory, only(mode, s));
	}
	s = fine();
	s.psram_free = 100;
	TEST_ASSERT_EQUAL_HEX16(health::kMemory, only(POWER_MODE_ON, s));
	s = fine();
	s.psram_total = s.psram_free = 0;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_ON, s));
}

CASE(cpu, "The stored CPU bit counts in every mode") {
	HealthInputs s = fine();
	s.cpu_high = true;
	for (power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(health::kCpu, only(mode, s));
	}
}

CASE(ready, "A mode is ready once every component it needs is ready") {
	HealthInputs s{};
	for (power_mode_t mode : kModes) {
		TEST_ASSERT_FALSE(health::ready(mode, s));
	}
	s.expander_ready = true;
	TEST_ASSERT_TRUE(health::ready(POWER_MODE_OFF, s));
	TEST_ASSERT_FALSE(health::ready(POWER_MODE_CHARGER, s));
	s.charger_ready = s.gauge_ready = s.usb_c_ready = true;
	TEST_ASSERT_TRUE(health::ready(POWER_MODE_CHARGER, s));
	TEST_ASSERT_FALSE(health::ready(POWER_MODE_ON, s));
	s.barometer_ready = true;
	TEST_ASSERT_FALSE(health::ready(POWER_MODE_ON, s));
	s.adsbee_ready = true;
	TEST_ASSERT_TRUE(health::ready(POWER_MODE_ON, s));
	TEST_ASSERT_FALSE(health::ready(POWER_MODE_UNKNOWN, s));
}

CASE(format, "The Health text names the set bits in bit order") {
	char text[96];
	health::format(0, text, sizeof text);
	TEST_ASSERT_EQUAL_STRING("OK (0x0)", text);
	health::format(health::kCharger | health::kAdsbee | health::kGnss, text,
				   sizeof text);
	TEST_ASSERT_EQUAL_STRING("ALERT (0x62 charger, adsbee, gnss)", text);
	health::format(0x3ff, text, sizeof text);
	TEST_ASSERT_EQUAL_STRING(
		"ALERT (0x3FF expander, charger, gauge, usb_c, barometer, adsbee, "
		"gnss, battery, memory, cpu)",
		text);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(all_fine);
	RUN_TEST(unknown);
	RUN_TEST(expander);
	RUN_TEST(charger_stale);
	RUN_TEST(charger_overvoltage);
	RUN_TEST(gauge_and_usb_c);
	RUN_TEST(on_only);
	RUN_TEST(gnss);
	RUN_TEST(gnss_needs_adsbee);
	RUN_TEST(battery_temperature);
	RUN_TEST(battery_missing);
	RUN_TEST(manual);
	RUN_TEST(soc);
	RUN_TEST(memory);
	RUN_TEST(cpu);
	RUN_TEST(ready);
	RUN_TEST(format);
	return UNITY_END();
}
#endif
