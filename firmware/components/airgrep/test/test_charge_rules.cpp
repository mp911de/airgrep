#include <cmath>

#include "system/charge_rules.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[charge]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

using charge_control::ChargeInputs;
using charge_control::Rules;
using charger::ChargeStop;
using charger::Supply;

namespace {
constexpr int64_t kSecond = 1000000;

ChargeInputs sdp_port(int64_t now_us) {
	ChargeInputs in{};
	in.now_us = now_us;
	in.charger_usable = true;
	in.vbus = true;
	in.detected_ma = 500;
	constexpr uint8_t standard_downstream_port = 1;
	in.detected_vbus_type = standard_downstream_port;
	in.pack_c = 25;
	in.usb_c_usable = true;
	in.capacity_mah = 2000;
	return in;
}

ChargeInputs advertising(int64_t now_us, unsigned ma) {
	ChargeInputs in = sdp_port(now_us);
	in.advertised_ma = ma;
	return in;
}

ChargeInputs at(int64_t now_us, float celsius) {
	ChargeInputs in = sdp_port(now_us);
	in.pack_c = celsius;
	return in;
}
}

CASE(cc_wins,
	 "A 1.5 A or 3 A CC advertisement wins over the detection result") {
	Rules rules;
	auto target = rules.apply_rules(advertising(0, 3000));
	TEST_ASSERT_TRUE(target.supply == Supply::usb_c);
	TEST_ASSERT_EQUAL_UINT(3000, target.input_ma);
	TEST_ASSERT_FALSE(target.hardware_limit);
	target = rules.apply_rules(advertising(kSecond, 1500));
	TEST_ASSERT_EQUAL_UINT(1500, target.input_ma);
	TEST_ASSERT_TRUE(target.hardware_limit);
}

CASE(detection, "Default CC leaves the input target to the detection result") {
	Rules rules;
	ChargeInputs in = sdp_port(0);
	in.detected_ma = 1500;
	in.detected_vbus_type = 3;
	const auto target = rules.apply_rules(in);
	TEST_ASSERT_TRUE(target.supply == Supply::detection);
	TEST_ASSERT_EQUAL_UINT8(3, target.detected_vbus_type);
	TEST_ASSERT_EQUAL_UINT(1500, target.input_ma);
	TEST_ASSERT_TRUE(target.hardware_limit);
}

CASE(stale_usb_c, "A stale USB-C State counts as default CC") {
	Rules rules;
	ChargeInputs in = advertising(0, 3000);
	in.usb_c_usable = false;
	const auto target = rules.apply_rules(in);
	TEST_ASSERT_TRUE(target.supply == Supply::detection);
	TEST_ASSERT_EQUAL_UINT(500, target.input_ma);
	TEST_ASSERT_TRUE(target.hardware_limit);
}

CASE(no_vbus,
	 "Without VBUS the hardware limit is on and IINDPM is left alone") {
	Rules rules;
	ChargeInputs in = advertising(0, 3000);
	in.vbus = false;
	const auto target = rules.apply_rules(in);
	TEST_ASSERT_TRUE(target.supply == Supply::none);
	TEST_ASSERT_EQUAL_UINT(0, target.input_ma);
	TEST_ASSERT_TRUE(target.hardware_limit);
}

CASE(wait,
	 "While a chip negotiates or detects only the hardware limit is set") {
	Rules rules;
	ChargeInputs in = advertising(0, 3000);
	in.detecting = true;
	auto target = rules.apply_rules(in);
	TEST_ASSERT_TRUE(target.supply == Supply::waiting);
	TEST_ASSERT_EQUAL_UINT(0, target.input_ma);
	TEST_ASSERT_TRUE(target.hardware_limit);
	in = advertising(kSecond, 3000);
	in.usb_c_negotiating = true;
	target = rules.apply_rules(in);
	TEST_ASSERT_TRUE(target.supply == Supply::waiting);
	TEST_ASSERT_EQUAL_INT64(kSecond, target.vbus_elapsed_us);
	target = rules.apply_rules(advertising(2 * kSecond, 3000));
	TEST_ASSERT_TRUE(target.supply == Supply::usb_c);
	TEST_ASSERT_EQUAL_UINT(3000, target.input_ma);
}

CASE(wait_ends, "The wait ends 5 s after VBUS appeared") {
	Rules rules;
	ChargeInputs in = sdp_port(0);
	in.detecting = true;
	TEST_ASSERT_TRUE(rules.apply_rules(in).supply == Supply::waiting);
	in.now_us = 5 * kSecond - 1;
	TEST_ASSERT_TRUE(rules.apply_rules(in).supply == Supply::waiting);
	in.now_us = 5 * kSecond;
	const auto target = rules.apply_rules(in);
	TEST_ASSERT_TRUE(target.supply == Supply::detection);
	TEST_ASSERT_EQUAL_UINT(500, target.input_ma);
}

CASE(stale_negotiation,
	 "A negotiating but stale USB-C State does not hold the wait") {
	Rules rules;
	ChargeInputs in = sdp_port(0);
	in.usb_c_usable = false;
	in.usb_c_negotiating = true;
	TEST_ASSERT_TRUE(rules.apply_rules(in).supply == Supply::detection);
}

CASE(detach, "Detach from a 3 A source sets the hardware limit at once") {
	Rules rules;
	TEST_ASSERT_FALSE(rules.apply_rules(advertising(0, 3000)).hardware_limit);
	auto target = rules.apply_rules(sdp_port(kSecond / 4));
	TEST_ASSERT_TRUE(target.hardware_limit);
	TEST_ASSERT_EQUAL_UINT(500, target.input_ma);
	ChargeInputs gone = sdp_port(kSecond / 2);
	gone.vbus = false;
	TEST_ASSERT_TRUE(rules.apply_rules(gone).hardware_limit);
	ChargeInputs back = advertising(10 * kSecond, 3000);
	back.detecting = true;
	target = rules.apply_rules(back);
	TEST_ASSERT_TRUE(target.supply == Supply::waiting);
	TEST_ASSERT_EQUAL_INT64(0, target.vbus_elapsed_us);
	TEST_ASSERT_TRUE(target.hardware_limit);
}

CASE(capacity, "Charge target is 0.5 C between 500 and 2000 mA") {
	Rules rules;
	ChargeInputs in = sdp_port(0);
	TEST_ASSERT_EQUAL_UINT(1000, rules.apply_rules(in).charge_ma);
	in.capacity_mah = 600;
	TEST_ASSERT_EQUAL_UINT(500, rules.apply_rules(in).charge_ma);
	in.capacity_mah = 5000;
	TEST_ASSERT_EQUAL_UINT(2000, rules.apply_rules(in).charge_ma);
	in.capacity_mah = 1234;
	TEST_ASSERT_EQUAL_UINT(610, rules.apply_rules(in).charge_ma);
}

CASE(thermal_steps,
	 "Warm packs charge at 1000 mA, then 500 mA, released 2 C lower") {
	Rules rules;
	auto in = at(0, 30);
	in.capacity_mah = 5000;
	TEST_ASSERT_EQUAL_UINT(2000, rules.apply_rules(in).charge_ma);
	in.pack_c = 33;
	TEST_ASSERT_EQUAL_UINT(1000, rules.apply_rules(in).charge_ma);
	in.pack_c = 36;
	TEST_ASSERT_EQUAL_UINT(500, rules.apply_rules(in).charge_ma);
	in.pack_c = 34.5f;
	TEST_ASSERT_EQUAL_UINT(500, rules.apply_rules(in).charge_ma);
	in.pack_c = 34;
	TEST_ASSERT_EQUAL_UINT(1000, rules.apply_rules(in).charge_ma);
	in.pack_c = 31.5f;
	TEST_ASSERT_EQUAL_UINT(1000, rules.apply_rules(in).charge_ma);
	in.pack_c = 31;
	TEST_ASSERT_EQUAL_UINT(2000, rules.apply_rules(in).charge_ma);
}

CASE(hot_stop, "Charging stops at 40 C and resumes at 38 C") {
	Rules rules;
	auto target = rules.apply_rules(at(0, 40));
	TEST_ASSERT_FALSE(target.charge_enabled);
	TEST_ASSERT_TRUE(target.stop == ChargeStop::hot);
	TEST_ASSERT_FALSE(rules.apply_rules(at(0, 38.5f)).charge_enabled);
	target = rules.apply_rules(at(0, 38));
	TEST_ASSERT_TRUE(target.charge_enabled);
	TEST_ASSERT_TRUE(target.stop == ChargeStop::none);
}

CASE(cold_stop, "Charging stops at 4 C and resumes at 6 C") {
	Rules rules;
	TEST_ASSERT_TRUE(rules.apply_rules(at(0, 4.5f)).charge_enabled);
	auto target = rules.apply_rules(at(0, 4));
	TEST_ASSERT_FALSE(target.charge_enabled);
	TEST_ASSERT_TRUE(target.stop == ChargeStop::cold);
	TEST_ASSERT_FALSE(rules.apply_rules(at(0, 5.5f)).charge_enabled);
	TEST_ASSERT_TRUE(rules.apply_rules(at(0, 6)).charge_enabled);
}

CASE(lost_temperature, "A lost temperature keeps a stop") {
	Rules rules;
	rules.apply_rules(at(0, 41));
	const auto target = rules.apply_rules(ChargeInputs{.capacity_mah = 2000});
	TEST_ASSERT_FALSE(target.charge_enabled);
	TEST_ASSERT_TRUE(target.stop == ChargeStop::hot);
}

CASE(no_temperature,
	 "Without a valid temperature since start charging runs at 500 mA") {
	Rules rules;
	const auto target = rules.apply_rules(at(0, NAN));
	TEST_ASSERT_TRUE(target.charge_enabled);
	TEST_ASSERT_EQUAL_UINT(500, target.charge_ma);
	TEST_ASSERT_EQUAL_UINT(1000, rules.apply_rules(at(0, 25)).charge_ma);
}

CASE(treg, "Every AUTO target sets TREG to 100 C") {
	Rules rules;
	TEST_ASSERT_EQUAL_UINT(100, rules.apply_rules(sdp_port(0)).thermal_limit_c);
}

namespace {
constexpr int64_t kMinute = 60 * kSecond;

ChargeInputs battery(int64_t now_us, unsigned vbat_mv,
					 bool overvoltage = false) {
	ChargeInputs in = sdp_port(now_us);
	in.vbat_mv = vbat_mv;
	in.battery_overvoltage = overvoltage;
	return in;
}

int64_t overvoltage_retry_time(Rules& rules, int64_t now_us) {
	rules.apply_rules(battery(now_us, 4200));
	rules.apply_rules(battery(now_us + kSecond, 4300, true));
	return now_us + kSecond + 5 * kMinute;
}
}

CASE(overvoltage_stop,
	 "VBAT_OVP stops charging for 5 min, then a pack resumes") {
	Rules rules;
	TEST_ASSERT_TRUE(rules.apply_rules(battery(0, 4200)).charge_enabled);
	auto target = rules.apply_rules(battery(kSecond, 4300, true));
	TEST_ASSERT_FALSE(target.charge_enabled);
	TEST_ASSERT_TRUE(target.stop == ChargeStop::overvoltage);
	TEST_ASSERT_FALSE(
		rules.apply_rules(battery(4 * kMinute, 4150)).charge_enabled);
	TEST_ASSERT_TRUE(
		rules.apply_rules(battery(kSecond + 5 * kMinute, 4150)).charge_enabled);
}

CASE(overvoltage_persists,
	 "VBAT_OVP still present after 5 min keeps charging off") {
	Rules rules;
	rules.apply_rules(battery(0, 4200));
	rules.apply_rules(battery(kSecond, 4300, true));
	TEST_ASSERT_FALSE(
		rules.apply_rules(battery(6 * kMinute, 4300, true)).charge_enabled);
	TEST_ASSERT_TRUE(
		rules.apply_rules(battery(7 * kMinute, 4150)).charge_enabled);
}

CASE(no_pack,
	 "Below 2.6 V after the off time is no pack, a fitted pack resumes") {
	Rules rules;
	rules.apply_rules(battery(0, 4200));
	rules.apply_rules(battery(kSecond, 4300, true));
	auto target = rules.apply_rules(battery(6 * kMinute, 2500));
	TEST_ASSERT_FALSE(target.charge_enabled);
	TEST_ASSERT_TRUE(target.stop == ChargeStop::no_battery);
	TEST_ASSERT_FALSE(
		rules.apply_rules(battery(60 * kMinute, 2550)).charge_enabled);
	TEST_ASSERT_TRUE(
		rules.apply_rules(battery(61 * kMinute, 3700)).charge_enabled);
}

CASE(one_count_per_period, "Repeated VBAT_OVP while off counts once") {
	Rules rules;
	int64_t now = 0;
	for (int i = 0; i < 9; ++i) {
		rules.apply_rules(battery(now, 4200));
		for (int j = 1; j <= 20; ++j) {
			rules.apply_rules(battery(now + j * kSecond, 4300, true));
		}
		now += 6 * kMinute;
	}
	TEST_ASSERT_TRUE(rules.apply_rules(battery(now, 4150)).charge_enabled);
}

CASE(lockout, "The 10th failure within 10 min of the last locks charging off") {
	Rules rules;
	int64_t now = 0;
	for (int i = 0; i < 9; ++i) {
		now = overvoltage_retry_time(rules, now);
	}
	TEST_ASSERT_TRUE(rules.apply_rules(battery(now, 4150)).charge_enabled);
	now = overvoltage_retry_time(rules, now);
	auto target = rules.apply_rules(battery(now, 4150));
	TEST_ASSERT_FALSE(target.charge_enabled);
	TEST_ASSERT_TRUE(target.stop == ChargeStop::locked);
	TEST_ASSERT_FALSE(
		rules.apply_rules(battery(now + 60 * kMinute, 2500)).charge_enabled);
	target = rules.apply_rules(battery(now + 120 * kMinute, 3800));
	TEST_ASSERT_TRUE(target.stop == ChargeStop::locked);
}

CASE(counter_reset, "Failures more than 10 min apart never add up") {
	Rules rules;
	int64_t now = 0;
	for (int i = 0; i < 12; ++i) {
		now = overvoltage_retry_time(rules, now);
		rules.apply_rules(battery(now, 4150));
		now += 11 * kMinute;
		rules.apply_rules(battery(now, 4150));
	}
	TEST_ASSERT_TRUE(rules.apply_rules(battery(now, 4150)).charge_enabled);
}

CASE(counts_while_off, "A VBAT_OVP during a temperature stop counts too") {
	Rules rules;
	int64_t now = 0;
	for (int i = 0; i < 10; ++i) {
		ChargeInputs in = battery(now, 4300, true);
		in.pack_c = 41;
		rules.apply_rules(in);
		now += 6 * kMinute;
		in = battery(now, 4150);
		in.pack_c = 41;
		rules.apply_rules(in);
	}
	TEST_ASSERT_TRUE(rules.apply_rules(battery(now, 4150)).stop ==
					 ChargeStop::locked);
}

CASE(temperature_first, "A temperature stop is named before an overvoltage") {
	Rules rules;
	rules.apply_rules(battery(0, 4200));
	rules.apply_rules(battery(kSecond, 4300, true));
	ChargeInputs in = battery(2 * kSecond, 4300, true);
	in.pack_c = 41;
	TEST_ASSERT_TRUE(rules.apply_rules(in).stop == ChargeStop::hot);
}

CASE(stale_charger_keeps_overvoltage,
	 "A stale charger State keeps the off state") {
	Rules rules;
	rules.apply_rules(battery(0, 4200));
	rules.apply_rules(battery(kSecond, 4300, true));
	ChargeInputs in = battery(6 * kMinute, 4150);
	in.charger_usable = false;
	TEST_ASSERT_FALSE(rules.apply_rules(in).charge_enabled);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(cc_wins);
	RUN_TEST(detection);
	RUN_TEST(stale_usb_c);
	RUN_TEST(no_vbus);
	RUN_TEST(wait);
	RUN_TEST(wait_ends);
	RUN_TEST(stale_negotiation);
	RUN_TEST(detach);
	RUN_TEST(capacity);
	RUN_TEST(thermal_steps);
	RUN_TEST(hot_stop);
	RUN_TEST(cold_stop);
	RUN_TEST(lost_temperature);
	RUN_TEST(no_temperature);
	RUN_TEST(treg);
	RUN_TEST(overvoltage_stop);
	RUN_TEST(overvoltage_persists);
	RUN_TEST(no_pack);
	RUN_TEST(one_count_per_period);
	RUN_TEST(lockout);
	RUN_TEST(counter_reset);
	RUN_TEST(counts_while_off);
	RUN_TEST(temperature_first);
	RUN_TEST(stale_charger_keeps_overvoltage);
	return UNITY_END();
}
#endif
