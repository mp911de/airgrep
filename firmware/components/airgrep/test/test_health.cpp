#include <initializer_list>
#include <utility>

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

HealthInputs fine() {
	HealthInputs s{};
	s.now_us = kNow;
	s.last_update.expander_us = s.last_update.charger_us =
		s.last_update.gauge_us = s.last_update.usb_c_us =
			s.last_update.barometer_us = s.last_update.adsbee_us =
				s.last_update.gnss_us = kNow;
	s.gnss_bbr = true;
	s.gnss_antenna = adsbee::AntennaStatus::ok;
	s.target_decided = true;
	s.stop = ChargeStop::none;
	s.internal_total = s.psram_total = 1000;
	s.internal_free = s.psram_free = 800;
	return s;
}

uint16_t only(power_mode_t mode, const HealthInputs& s) {
	return health::evaluate_health(mode, s);
}
}

CASE(all_fine, "A fine snapshot is 0x0 in every mode") {
	for (const power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(0, only(mode, fine()));
	}
}

CASE(unknown, "UNKNOWN mode is 0x0 whatever the snapshot") {
	HealthInputs s{};
	s.cpu_high = true;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_UNKNOWN, s));
}

CASE(failures, "The first seven causes are the failures") {
	TEST_ASSERT_EQUAL_HEX16(
		health::kRequiredI2cDeviceMissing | health::kAdsbee | health::kGnss |
			health::kAntennaShort | health::kChargerOvervoltage |
			health::kChargerOverheated | health::kBatteryOverheated,
		health::kFailures);
}

CASE(expander, "A stale expander flags I2C in every mode") {
	HealthInputs s = fine();
	s.last_update.expander_us = kNow - 5000001;
	for (const power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(health::kRequiredI2cDeviceMissing,
								only(mode, s));
	}
	s.last_update.expander_us = kNow - 5000000;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_ON, s));
}

CASE(charger_stale, "A stale charger flags I2C in ON and CHARGER, not in OFF") {
	HealthInputs s = fine();
	s.last_update.charger_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kRequiredI2cDeviceMissing,
							only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(health::kRequiredI2cDeviceMissing,
							only(POWER_MODE_CHARGER, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
}

CASE(charger_overvoltage, "Any live overvoltage bit flags the charger") {
	for (const power_mode_t mode : kModes) {
		HealthInputs s = fine();
		s.fault0.vbus_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kChargerOvervoltage, only(mode, s));
		s = fine();
		s.fault0.battery_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kChargerOvervoltage, only(mode, s));
		s = fine();
		s.fault0.ac1_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kChargerOvervoltage, only(mode, s));
		s = fine();
		s.fault0.ac2_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kChargerOvervoltage, only(mode, s));
		s = fine();
		s.fault1.system_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kChargerOvervoltage, only(mode, s));
		s = fine();
		s.fault1.otg_overvoltage = true;
		TEST_ASSERT_EQUAL_HEX16(health::kChargerOvervoltage, only(mode, s));
	}
	HealthInputs s = fine();
	s.fault0.bus_overcurrent = true;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_ON, s));
}

CASE(charger_overheated, "A thermal shutdown flags the charger") {
	HealthInputs s = fine();
	s.fault1.thermal_shutdown = true;
	for (const power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(health::kChargerOverheated, only(mode, s));
	}
}

CASE(charger_faults_need_charger, "A stale charger shows no fault of its own") {
	HealthInputs s = fine();
	s.last_update.charger_us = 0;
	s.fault0.vbus_overvoltage = s.fault1.thermal_shutdown = true;
	TEST_ASSERT_EQUAL_HEX16(health::kRequiredI2cDeviceMissing,
							only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
}

CASE(gauge_and_usb_c, "Gauge and USB-C controller count in ON and CHARGER") {
	HealthInputs s = fine();
	s.last_update.gauge_us = 0;
	s.last_update.usb_c_us = 0;
	const uint16_t both = health::kGauge | health::kUsbC;
	TEST_ASSERT_EQUAL_HEX16(both, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(both, only(POWER_MODE_CHARGER, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
}

CASE(on_only, "Barometer and ADSBee link count in ON only") {
	HealthInputs s = fine();
	s.last_update.barometer_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kBarometer, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_CHARGER, s));
	s = fine();
	s.last_update.adsbee_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kAdsbee, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_CHARGER, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
}

CASE(gnss, "A stale frame flags the GNSS receiver in ON") {
	HealthInputs s = fine();
	s.last_update.gnss_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kGnss, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_CHARGER, s));
}

CASE(gnss_frame, "The frame flags the antenna and the backup battery") {
	HealthInputs s = fine();
	s.gnss_antenna = adsbee::AntennaStatus::shorted;
	TEST_ASSERT_EQUAL_HEX16(health::kAntennaShort, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_CHARGER, s));
	s.gnss_antenna = adsbee::AntennaStatus::open;
	TEST_ASSERT_EQUAL_HEX16(health::kAntennaOpen, only(POWER_MODE_ON, s));
	s.gnss_bbr = false;
	TEST_ASSERT_EQUAL_HEX16(health::kAntennaOpen | health::kBackupBattery,
							only(POWER_MODE_ON, s));
	s.gnss_antenna = adsbee::AntennaStatus::unreported;
	TEST_ASSERT_EQUAL_HEX16(health::kBackupBattery, only(POWER_MODE_ON, s));
	s.last_update.gnss_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kGnss, only(POWER_MODE_ON, s));
}

CASE(gnss_needs_adsbee, "Without the ADSBee link GNSS is not checked") {
	HealthInputs s = fine();
	s.last_update.adsbee_us = 0;
	s.last_update.gnss_us = 0;
	s.gnss_bbr = false;
	s.gnss_antenna = adsbee::AntennaStatus::shorted;
	TEST_ASSERT_EQUAL_HEX16(health::kAdsbee, only(POWER_MODE_ON, s));
}

CASE(battery_temperature,
	 "A hot or cold stop flags the battery in every mode") {
	const std::pair<ChargeStop, uint16_t> stops[] = {
		{ChargeStop::hot, health::kBatteryOverheated},
		{ChargeStop::cold, health::kBatteryCold}};
	for (auto [stop, cause] : stops) {
		HealthInputs s = fine();
		s.stop = stop;
		for (const power_mode_t mode : kModes) {
			TEST_ASSERT_EQUAL_HEX16(cause, only(mode, s));
		}
		s.last_update.charger_us = 0;
		TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
	}
}

CASE(no_battery, "Without a pack its gauge and its temperature say nothing") {
	HealthInputs s = fine();
	s.battery_absent = true;
	s.last_update.gauge_us = 0;
	s.stop = ChargeStop::cold;
	for (const power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(0, only(mode, s));
	}
	s.battery_absent = false;
	TEST_ASSERT_EQUAL_HEX16(health::kGauge | health::kBatteryCold,
							only(POWER_MODE_ON, s));
	s.expander_ready = s.charger_ready = s.usb_c_ready = true;
	s.barometer_ready = s.adsbee_ready = true;
	for (auto mode : {POWER_MODE_ON, POWER_MODE_CHARGER}) {
		s.battery_absent = false;
		TEST_ASSERT_FALSE(health::ready(mode, s));
		s.battery_absent = true;
		TEST_ASSERT_TRUE(health::ready(mode, s));
	}
	s.last_update.gauge_us = kNow;
	s.battery_low = true;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_ON, s));
}

CASE(not_charging, "A stop for another reason is no cause") {
	for (const ChargeStop stop : {ChargeStop::no_battery, ChargeStop::locked,
								  ChargeStop::overvoltage}) {
		HealthInputs s = fine();
		s.stop = stop;
		for (const power_mode_t mode : kModes) {
			TEST_ASSERT_EQUAL_HEX16(0, only(mode, s));
		}
	}
}

CASE(manual, "MANUAL skips the charge control battery checks") {
	HealthInputs s = fine();
	s.target_decided = false;
	s.stop = ChargeStop::hot;
	for (const power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(0, only(mode, s));
	}
}

CASE(soc, "A low battery flags the battery in ON only") {
	HealthInputs s = fine();
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_ON, s));
	s.battery_low = true;
	TEST_ASSERT_EQUAL_HEX16(health::kBatteryLow, only(POWER_MODE_ON, s));
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_CHARGER, s));
	s.last_update.gauge_us = 0;
	TEST_ASSERT_EQUAL_HEX16(health::kGauge, only(POWER_MODE_ON, s));
}

CASE(memory, "Less than 10 % free in either region flags memory") {
	HealthInputs s = fine();
	s.internal_free = 100;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_OFF, s));
	s.internal_free = 99;
	for (const power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(health::kMemory, only(mode, s));
	}
	s = fine();
	s.psram_free = 50;
	TEST_ASSERT_EQUAL_HEX16(health::kMemory, only(POWER_MODE_ON, s));
	s = fine();
	s.psram_total = s.psram_free = 0;
	TEST_ASSERT_EQUAL_HEX16(0, only(POWER_MODE_ON, s));
}

CASE(cpu, "The stored CPU bit counts in every mode") {
	HealthInputs s = fine();
	s.cpu_high = true;
	for (const power_mode_t mode : kModes) {
		TEST_ASSERT_EQUAL_HEX16(health::kCpu, only(mode, s));
	}
}

CASE(ready, "A mode is ready once every component it needs is ready") {
	HealthInputs s{};
	for (const power_mode_t mode : kModes) {
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
	char text[256];
	health::format(0, text, sizeof text);
	TEST_ASSERT_EQUAL_STRING("OK (0x0)", text);
	health::format(health::kAdsbee | health::kGnss | health::kBarometer, text,
				   sizeof text);
	TEST_ASSERT_EQUAL_STRING("ALERT (0x206 adsbee, gnss, barometer)", text);
	health::format(0xffff, text, sizeof text);
	TEST_ASSERT_EQUAL_STRING(
		"ALERT (0xFFFF i2c, adsbee, gnss, antenna_short, charger_overvoltage, "
		"charger_overheated, battery_overheated, antenna_open, "
		"backup_battery, barometer, gauge, usb_c, battery_low, battery_cold, "
		"memory, cpu)",
		text);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(all_fine);
	RUN_TEST(unknown);
	RUN_TEST(failures);
	RUN_TEST(expander);
	RUN_TEST(charger_stale);
	RUN_TEST(charger_overvoltage);
	RUN_TEST(charger_overheated);
	RUN_TEST(charger_faults_need_charger);
	RUN_TEST(gauge_and_usb_c);
	RUN_TEST(on_only);
	RUN_TEST(gnss);
	RUN_TEST(gnss_frame);
	RUN_TEST(gnss_needs_adsbee);
	RUN_TEST(battery_temperature);
	RUN_TEST(no_battery);
	RUN_TEST(not_charging);
	RUN_TEST(manual);
	RUN_TEST(soc);
	RUN_TEST(memory);
	RUN_TEST(cpu);
	RUN_TEST(ready);
	RUN_TEST(format);
	return UNITY_END();
}
#endif
