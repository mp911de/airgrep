#include "battery/max17048_i2c.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[battery]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(vcell, "Battery VCELL decodes whole words in volts") {
	TEST_ASSERT_FLOAT_WITHIN(0.00001f, 4.0f, battery::decode_vcell(0xc800));
	TEST_ASSERT_FLOAT_WITHIN(0.0000001f, 0.000078125f, battery::decode_vcell(1));
}

CASE(soc, "Battery SOC retains fractional percentage") {
	TEST_ASSERT_FLOAT_WITHIN(0.0001f, 50.5f, battery::decode_soc(0x3280));
	TEST_ASSERT_EQUAL_FLOAT(100.0f, battery::decode_soc(0x6400));
}

CASE(crate, "Battery CRATE distinguishes charge and discharge rates") {
	TEST_ASSERT_FLOAT_WITHIN(0.0001f, 20.8f, battery::decode_crate(0x0064));
	TEST_ASSERT_FLOAT_WITHIN(0.0001f, -20.8f, battery::decode_crate(0xff9c));
	TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.208f, battery::decode_crate(0xffff));
}

CASE(status, "Battery STATUS decodes alerts in the high byte including RI") {
	const auto first = battery::decode_status(0x15ff);
	TEST_ASSERT_TRUE(first.reset);
	TEST_ASSERT_FALSE(first.voltage_high);
	TEST_ASSERT_TRUE(first.voltage_low);
	TEST_ASSERT_FALSE(first.voltage_reset);
	TEST_ASSERT_TRUE(first.low_soc);
	TEST_ASSERT_FALSE(first.soc_change);
	TEST_ASSERT_EQUAL_HEX16(0x14ff, battery::acknowledge_reset(0x15ff));
	TEST_ASSERT_EQUAL_HEX16(0x2a00, battery::acknowledge_reset(0x2a00));
	const auto second = battery::decode_status(0x2a00);
	TEST_ASSERT_FALSE(second.reset);
	TEST_ASSERT_TRUE(second.voltage_high);
	TEST_ASSERT_FALSE(second.voltage_low);
	TEST_ASSERT_TRUE(second.voltage_reset);
	TEST_ASSERT_FALSE(second.low_soc);
	TEST_ASSERT_TRUE(second.soc_change);
}

CASE(remaining, "Remaining time estimates charge and discharge but hides near zero rates") {
	const auto discharge = battery::remaining_time(40, -20);
	TEST_ASSERT_TRUE(discharge.valid);
	TEST_ASSERT_EQUAL_FLOAT(2, discharge.hours);
	const auto charge = battery::remaining_time(40, 20);
	TEST_ASSERT_TRUE(charge.valid);
	TEST_ASSERT_EQUAL_FLOAT(3, charge.hours);
	TEST_ASSERT_FALSE(battery::remaining_time(40, 0).valid);
	TEST_ASSERT_FALSE(battery::remaining_time(40, 0.832f).valid);
	TEST_ASSERT_FALSE(battery::remaining_time(40, -0.832f).valid);
	TEST_ASSERT_FALSE(battery::remaining_time(101, 20).valid);
	TEST_ASSERT_EQUAL_FLOAT(0, battery::remaining_time(100, 20).hours);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(vcell);
	RUN_TEST(soc);
	RUN_TEST(crate);
	RUN_TEST(status);
	RUN_TEST(remaining);
	return UNITY_END();
}
#endif
