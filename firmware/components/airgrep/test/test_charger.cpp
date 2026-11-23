#include <cmath>

#include "charger/bq25798_i2c.hpp"
#include "device.h"
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
	TEST_ASSERT_TRUE(charger::decode_control0(0xa2).charge_enabled);
	TEST_ASSERT_FALSE(charger::decode_control0(0x04).charge_enabled);
}

CASE(encode_limits,
	 "Charger limit encoders keep reserved bits and round down") {
	TEST_ASSERT_EQUAL_HEX16(0x00b4, charger::encode_current(0x0000, 1800));
	TEST_ASSERT_EQUAL_HEX16(0xfe32, charger::encode_current(0xfe00, 505));
	TEST_ASSERT_EQUAL_HEX16(0x012c, charger::encode_current(0x0000, 3000));
	TEST_ASSERT_EQUAL_UINT32(3000, charger::decode_input_current(0x01, 0x2c));
	TEST_ASSERT_EQUAL_HEX8(0xab,
						   charger::encode_temperature_control(0xeb, 100));
	TEST_ASSERT_EQUAL_UINT32(100, charger::decode_temperature_control(0xab));
}

CASE(detected_current,
	 "Charger detection result follows table 7-4, unknown as 500 mA") {
	const auto sdp = charger::decode_status1(0x03);
	TEST_ASSERT_EQUAL_UINT32(500, charger::detected_ma(sdp, true));
	TEST_ASSERT_EQUAL_UINT32(
		1500, charger::detected_ma(charger::decode_status1(0x05), true));
	TEST_ASSERT_EQUAL_UINT32(
		1500, charger::detected_ma(charger::decode_status1(0x07), true));
	TEST_ASSERT_EQUAL_UINT32(
		1500, charger::detected_ma(charger::decode_status1(0x09), true));
	TEST_ASSERT_EQUAL_UINT32(
		500, charger::detected_ma(charger::decode_status1(0x0b), true));
	TEST_ASSERT_EQUAL_UINT32(
		1000, charger::detected_ma(charger::decode_status1(0x0d), true));
	TEST_ASSERT_EQUAL_UINT32(
		500, charger::detected_ma(charger::decode_status1(0x11), true));
	TEST_ASSERT_EQUAL_UINT32(0, charger::detected_ma(sdp, false));
}

CASE(detecting, "Charger detects while VBUS is present and BC1.2 is not done") {
	TEST_ASSERT_TRUE(charger::detecting(true, charger::decode_status1(0x00)));
	TEST_ASSERT_FALSE(charger::detecting(true, charger::decode_status1(0x07)));
	TEST_ASSERT_FALSE(charger::detecting(false, charger::decode_status1(0x00)));
}

CASE(pack_celsius,
	 "Pack temperature needs a valid ADC and an estimate in range") {
	TEST_ASSERT_FLOAT_WITHIN(0.5f, charger::estimate_ntc_c(50),
							 charger::pack_celsius(true, 50));
	TEST_ASSERT_TRUE(std::isnan(charger::pack_celsius(false, 50)));
	TEST_ASSERT_TRUE(std::isnan(charger::pack_celsius(true, 0)));
}

CASE(usable, "A State is usable up to 5 s after its last successful Poll") {
	TEST_ASSERT_FALSE(state_usable(0, 1000));
	TEST_ASSERT_TRUE(state_usable(1000, 1000 + 5000000));
	TEST_ASSERT_FALSE(state_usable(1000, 1001 + 5000000));
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

CASE(temperature_control,
	 "Charger temperature control register decodes datasheet values") {
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

CASE(charge_current,
	 "Charger charge current register decodes datasheet values") {
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

CASE(ntc_temperature,
	 "Charger NTC estimate follows the selected divider and beta curve") {
	// Independently calculated TS ratios for the nominal 100k B4262 sensor.
	TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, charger::estimate_ntc_c(75.278928f));
	TEST_ASSERT_FLOAT_WITHIN(0.05f, 25.0f, charger::estimate_ntc_c(51.900308f));
	TEST_ASSERT_FLOAT_WITHIN(0.05f, 60.0f, charger::estimate_ntc_c(20.884195f));
	TEST_ASSERT_FLOAT_WITHIN(0.05f, 85.0f, charger::estimate_ntc_c(9.902284f));
	TEST_ASSERT_FLOAT_WITHIN(
		0.05f, 25.0f, charger::estimate_ntc_c(charger::decode_ts(0x02, 0x13)));
}

CASE(
	ntc_invalid,
	"Charger NTC estimate rejects invalid ratios and sensor range violations") {
	const float invalid[] = {-1.0f,  0.0f,   0.001f, 90.0f,   90.35295f,
							 100.0f, 101.0f, NAN,    INFINITY};
	for (const float percent : invalid) {
		TEST_ASSERT_TRUE(std::isnan(charger::estimate_ntc_c(percent)));
	}
}

CASE(detect_battery,
	 "Battery detection calls a VBAT drop above 1500 mV no battery") {
	TEST_ASSERT_EQUAL(BatteryDetected::test_present,
					  charger::detect_battery(4100, 4050));
	TEST_ASSERT_EQUAL(BatteryDetected::test_present,
					  charger::detect_battery(4100, 2600));
	TEST_ASSERT_EQUAL(BatteryDetected::absent,
					  charger::detect_battery(4100, 2500));
	TEST_ASSERT_EQUAL(BatteryDetected::test_present,
					  charger::detect_battery(3000, 3100));
}

CASE(tdie, "Charger tdie register decodes datasheet values") {
	TEST_ASSERT_FLOAT_WITHIN(0.001f, -40.0f, charger::decode_tdie(0xff, 0xb0));
	TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, charger::decode_tdie(0x00, 0x32));
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(encode_limits);
	RUN_TEST(detected_current);
	RUN_TEST(detecting);
	RUN_TEST(pack_celsius);
	RUN_TEST(usable);
	RUN_TEST(detect_battery);
	RUN_TEST(tdie);
	RUN_TEST(ts);
	RUN_TEST(ntc_temperature);
	RUN_TEST(ntc_invalid);
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
