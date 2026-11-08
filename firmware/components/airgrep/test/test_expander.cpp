#include "i2c_expander/tca9534_i2c.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[expander]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(input, "Expander input decodes pin levels and active low controls") {
	const auto value = expander::decode_input(0x52);
	TEST_ASSERT_FALSE(value.high[0]);
	TEST_ASSERT_TRUE(value.high[1]);
	TEST_ASSERT_TRUE(value.high[4]);
	TEST_ASSERT_TRUE(value.high[6]);
	TEST_ASSERT_FALSE(value.high[7]);
	TEST_ASSERT_FALSE(value.mute);
	TEST_ASSERT_TRUE(value.wifi);
	TEST_ASSERT_TRUE(value.power);
	const auto opposite = expander::decode_input(0x81);
	TEST_ASSERT_TRUE(opposite.mute);
	TEST_ASSERT_FALSE(opposite.wifi);
	TEST_ASSERT_FALSE(opposite.power);
}

CASE(output, "Expander output decodes the output latch independently of inputs") {
	const auto value = expander::decode_output(0x30);
	TEST_ASSERT_FALSE(value.high[2]);
	TEST_ASSERT_FALSE(value.high[3]);
	TEST_ASSERT_TRUE(value.high[4]);
	TEST_ASSERT_TRUE(value.high[5]);
	TEST_ASSERT_FALSE(value.high[6]);
}

CASE(polarity, "Expander polarity bits identify inverted input pins") {
	const auto value = expander::decode_polarity(0x81);
	TEST_ASSERT_TRUE(value.inverted[0]);
	TEST_ASSERT_FALSE(value.inverted[1]);
	TEST_ASSERT_FALSE(value.inverted[6]);
	TEST_ASSERT_TRUE(value.inverted[7]);
}

CASE(configuration, "Expander configuration one means input and zero means output") {
	const auto value = expander::decode_configuration(0xf3);
	TEST_ASSERT_TRUE(value.input[0]);
	TEST_ASSERT_TRUE(value.input[1]);
	TEST_ASSERT_FALSE(value.input[2]);
	TEST_ASSERT_FALSE(value.input[3]);
	TEST_ASSERT_TRUE(value.input[4]);
	TEST_ASSERT_TRUE(value.input[7]);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(input);
	RUN_TEST(output);
	RUN_TEST(polarity);
	RUN_TEST(configuration);
	return UNITY_END();
}
#endif
