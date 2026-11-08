#include "charger/bq25798_i2c.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[charger]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(status0, "Charger status 0 separates watchdog and power presence") {
	const auto value = charger::decode_status0(0xa9);
	TEST_ASSERT_TRUE(value.input_limit);
	TEST_ASSERT_FALSE(value.voltage_limit);
	TEST_ASSERT_TRUE(value.watchdog);
	TEST_ASSERT_TRUE(value.power_good);
	TEST_ASSERT_FALSE(value.ac2);
	TEST_ASSERT_FALSE(value.ac1);
	TEST_ASSERT_TRUE(value.vbus);
}

CASE(status1, "Charger status1 register decodes datasheet values") {
	const auto v = charger::decode_status1(0x67);
	TEST_ASSERT_EQUAL_UINT8(3, v.charge);
	TEST_ASSERT_EQUAL_UINT8(3, v.vbus);
	TEST_ASSERT_TRUE(v.detection_done);
}

CASE(status2, "Charger status2 register decodes datasheet values") {
	const auto v = charger::decode_status2(0x85);
	TEST_ASSERT_EQUAL_UINT8(2, v.optimizer);
	TEST_ASSERT_TRUE(v.thermal);
	TEST_ASSERT_FALSE(v.detection);
	TEST_ASSERT_TRUE(v.battery);
}

CASE(status3, "Charger status3 register decodes datasheet values") {
	const auto v = charger::decode_status3(0xb6);
	TEST_ASSERT_TRUE(v.ac2);
	TEST_ASSERT_FALSE(v.ac1);
	TEST_ASSERT_TRUE(v.adc_done);
	TEST_ASSERT_TRUE(v.minimum_system);
	TEST_ASSERT_FALSE(v.charge_timer);
	TEST_ASSERT_TRUE(v.trickle_timer);
	TEST_ASSERT_TRUE(v.precharge_timer);
}

CASE(status4, "Charger status4 register decodes datasheet values") {
	const auto v = charger::decode_status4(0x15);
	TEST_ASSERT_TRUE(v.otg_low);
	TEST_ASSERT_FALSE(v.cold);
	TEST_ASSERT_TRUE(v.cool);
	TEST_ASSERT_FALSE(v.warm);
	TEST_ASSERT_TRUE(v.hot);
}

CASE(fault0, "Charger fault0 register decodes datasheet values") {
	const auto v = charger::decode_fault0(0xa5);
	TEST_ASSERT_TRUE(v.battery_regulation);
	TEST_ASSERT_FALSE(v.vbus_overvoltage);
	TEST_ASSERT_TRUE(v.battery_overvoltage);
	TEST_ASSERT_FALSE(v.bus_overcurrent);
	TEST_ASSERT_FALSE(v.battery_overcurrent);
	TEST_ASSERT_TRUE(v.converter_overcurrent);
	TEST_ASSERT_FALSE(v.ac2_overvoltage);
	TEST_ASSERT_TRUE(v.ac1_overvoltage);
}

CASE(fault1, "Charger fault1 register decodes datasheet values") {
	const auto v = charger::decode_fault1(0x94);
	TEST_ASSERT_TRUE(v.system_short);
	TEST_ASSERT_FALSE(v.system_overvoltage);
	TEST_ASSERT_FALSE(v.otg_overvoltage);
	TEST_ASSERT_TRUE(v.otg_undervoltage);
	TEST_ASSERT_TRUE(v.thermal_shutdown);
}

CASE(flag0, "Charger flag0 register decodes datasheet values") {
	const auto v = charger::decode_flag0(0x5a);
	TEST_ASSERT_FALSE(v.input_limit);
	TEST_ASSERT_TRUE(v.voltage_limit);
	TEST_ASSERT_FALSE(v.watchdog);
	TEST_ASSERT_TRUE(v.poor_source);
	TEST_ASSERT_TRUE(v.power_good);
	TEST_ASSERT_FALSE(v.ac2);
	TEST_ASSERT_TRUE(v.ac1);
	TEST_ASSERT_FALSE(v.vbus);
}

CASE(fault_flag0, "Charger fault flag0 register decodes datasheet values") {
	const auto v = charger::decode_fault_flag0(0x5a);
	TEST_ASSERT_FALSE(v.battery_regulation);
	TEST_ASSERT_TRUE(v.vbus_overvoltage);
	TEST_ASSERT_FALSE(v.battery_overvoltage);
	TEST_ASSERT_TRUE(v.bus_overcurrent);
	TEST_ASSERT_TRUE(v.battery_overcurrent);
	TEST_ASSERT_FALSE(v.converter_overcurrent);
	TEST_ASSERT_TRUE(v.ac2_overvoltage);
	TEST_ASSERT_FALSE(v.ac1_overvoltage);
}

CASE(fault_flag1, "Charger fault flag1 register decodes datasheet values") {
	const auto v = charger::decode_fault_flag1(0x60);
	TEST_ASSERT_FALSE(v.system_short);
	TEST_ASSERT_TRUE(v.system_overvoltage);
	TEST_ASSERT_TRUE(v.otg_overvoltage);
	TEST_ASSERT_FALSE(v.otg_undervoltage);
	TEST_ASSERT_FALSE(v.thermal_shutdown);
}

CASE(control0, "Charger control0 register decodes datasheet values") {
	TEST_ASSERT_TRUE(charger::decode_control0(0x04).high_impedance);
	TEST_ASSERT_FALSE(charger::decode_control0(0xa2).high_impedance);
}

CASE(control1, "Charger control1 register decodes datasheet values") {
	const auto v = charger::decode_control1(0x8c);
	TEST_ASSERT_EQUAL_UINT32(20000, v.watchdog_ms);
	TEST_ASSERT_TRUE(v.reset);
	TEST_ASSERT_EQUAL_UINT32(40000, charger::decode_control1(0x85).watchdog_ms);
}

CASE(control5, "Charger control5 register decodes datasheet values") {
	const auto v = charger::decode_control5(0x96);
	TEST_ASSERT_TRUE(v.ship_fet);
	TEST_ASSERT_FALSE(v.discharge_sensing);
	TEST_ASSERT_TRUE(v.external_limit);
}

CASE(temperature_control, "Charger temperature control register decodes datasheet values") {
	TEST_ASSERT_EQUAL_UINT32(120, charger::decode_temperature_control(0xc0));
	TEST_ASSERT_EQUAL_UINT32(80, charger::decode_temperature_control(0x40));
}

CASE(adc_control, "Charger adc control register decodes datasheet values") {
	const auto v = charger::decode_adc_control(0x80);
	TEST_ASSERT_TRUE(v.enabled);
	TEST_ASSERT_TRUE(v.continuous);
	TEST_ASSERT_EQUAL_UINT32(24, v.sample_ms);
	TEST_ASSERT_FALSE(charger::decode_adc_control(0x40).continuous);
}

CASE(adc_disable0, "Charger adc disable0 register decodes datasheet values") {
	TEST_ASSERT_EQUAL_UINT32(7, charger::decode_adc_disable0(0));
	TEST_ASSERT_EQUAL_UINT32(5, charger::decode_adc_disable0(0x83));
}

CASE(adc_disable1, "Charger adc disable1 register decodes datasheet values") {
	TEST_ASSERT_EQUAL_UINT32(4, charger::decode_adc_disable1(0));
	TEST_ASSERT_EQUAL_UINT32(2, charger::decode_adc_disable1(0x9f));
}

CASE(identity, "Charger identity register decodes datasheet values") {
	const auto v = charger::decode_identity(0x19);
	TEST_ASSERT_EQUAL_UINT8(3, v.part);
	TEST_ASSERT_EQUAL_UINT8(1, v.revision);
}

CASE(charge_current, "Charger charge current register decodes datasheet values") {
	TEST_ASSERT_EQUAL_UINT32(1800, charger::decode_charge_current(0x00, 0xb4));
	TEST_ASSERT_EQUAL_UINT32(5000, charger::decode_charge_current(0x01, 0xf4));
}

CASE(input_current, "Charger input current register decodes datasheet values") {
	TEST_ASSERT_EQUAL_UINT32(500, charger::decode_input_current(0x00, 0x32));
	TEST_ASSERT_EQUAL_UINT32(3000, charger::decode_input_current(0x01, 0x2c));
}

CASE(ibus, "Charger ibus register decodes datasheet values") {
	TEST_ASSERT_EQUAL_INT(-1000, charger::decode_ibus(0xfc, 0x18));
	TEST_ASSERT_EQUAL_INT(1500, charger::decode_ibus(0x05, 0xdc));
}

CASE(ibat, "Charger ibat register decodes datasheet values") {
	TEST_ASSERT_EQUAL_INT(-500, charger::decode_ibat(0xfe, 0x0c));
	TEST_ASSERT_EQUAL_INT(1800, charger::decode_ibat(0x07, 0x08));
}

CASE(vbus, "Charger vbus register decodes datasheet values") {
	TEST_ASSERT_EQUAL_UINT32(5000, charger::decode_adc_mv(0x13, 0x88));
}

CASE(vbat, "Charger vbat register decodes datasheet values") {
	TEST_ASSERT_EQUAL_UINT32(4200, charger::decode_adc_mv(0x10, 0x68));
}

CASE(vsys, "Charger vsys register decodes datasheet values") {
	TEST_ASSERT_EQUAL_UINT32(3600, charger::decode_adc_mv(0x0e, 0x10));
}

CASE(ts, "Charger ts register decodes datasheet values") {
	TEST_ASSERT_FLOAT_WITHIN(0.00001f, 50.0f, charger::decode_ts(0x02, 0x00));
}

CASE(tdie, "Charger tdie register decodes datasheet values") {
	TEST_ASSERT_FLOAT_WITHIN(0.001f, -40.0f, charger::decode_tdie(0xff, 0xb0));
	TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, charger::decode_tdie(0x00, 0x32));
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(tdie);
	RUN_TEST(ts);
	RUN_TEST(vsys);
	RUN_TEST(vbat);
	RUN_TEST(vbus);
	RUN_TEST(ibat);
	RUN_TEST(ibus);
	RUN_TEST(input_current);
	RUN_TEST(charge_current);
	RUN_TEST(identity);
	RUN_TEST(adc_disable1);
	RUN_TEST(adc_disable0);
	RUN_TEST(adc_control);
	RUN_TEST(temperature_control);
	RUN_TEST(control5);
	RUN_TEST(control1);
	RUN_TEST(control0);
	RUN_TEST(fault_flag1);
	RUN_TEST(fault_flag0);
	RUN_TEST(flag0);
	RUN_TEST(fault1);
	RUN_TEST(fault0);
	RUN_TEST(status4);
	RUN_TEST(status3);
	RUN_TEST(status2);
	RUN_TEST(status1);
	RUN_TEST(status0);
	return UNITY_END();
}
#endif
