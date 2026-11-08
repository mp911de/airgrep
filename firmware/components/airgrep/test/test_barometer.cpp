#include "baro/bmp580_i2c.hpp"
#include <cmath>
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[barometer]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(pressure, "Barometer pressure decodes signed 24-bit little endian data in hPa") {
	TEST_ASSERT_FLOAT_WITHIN(0.001f, 1013.25f, barometer::decode_pressure(0x40, 0xf3, 0x62));
	TEST_ASSERT_FLOAT_WITHIN(0.00001f, -1.0f, barometer::decode_pressure(0x00, 0xe7, 0xff));
}

CASE(temperature, "Barometer temperature retains fractions and negative values") {
	TEST_ASSERT_EQUAL_FLOAT(25.5f, barometer::decode_temperature(0, 0x80, 0x19));
	TEST_ASSERT_EQUAL_FLOAT(-10.25f, barometer::decode_temperature(0, 0xc0, 0xf5));
}

CASE(status, "Barometer STATUS separates NVM readiness and errors") {
	const auto good = barometer::decode_status(0xf3);
	TEST_ASSERT_TRUE(good.ready);
	TEST_ASSERT_FALSE(good.error);
	TEST_ASSERT_FALSE(good.command_error);
	const auto failed = barometer::decode_status(0x0c);
	TEST_ASSERT_FALSE(failed.ready);
	TEST_ASSERT_TRUE(failed.error);
	TEST_ASSERT_TRUE(failed.command_error);
}

CASE(odr, "Barometer ODR_CONFIG separates power mode, rate and deep standby") {
	const auto normal = barometer::decode_odr(0xf1);
	TEST_ASSERT_EQUAL_UINT(1, normal.mode);
	TEST_ASSERT_EQUAL_UINT(28, normal.rate);
	TEST_ASSERT_TRUE(normal.deep_disabled);
	const auto forced = barometer::decode_odr(0x02);
	TEST_ASSERT_EQUAL_UINT(2, forced.mode);
	TEST_ASSERT_EQUAL_UINT(0, forced.rate);
	TEST_ASSERT_FALSE(forced.deep_disabled);
}

CASE(osr, "Barometer OSR_CONFIG decodes independent ratios and pressure enable") {
	const auto selected = barometer::decode_osr(0x5a);
	TEST_ASSERT_EQUAL_UINT(4, selected.temperature);
	TEST_ASSERT_EQUAL_UINT(8, selected.pressure);
	TEST_ASSERT_TRUE(selected.pressure_enabled);
	const auto maximum = barometer::decode_osr(0xbf);
	TEST_ASSERT_EQUAL_UINT(128, maximum.temperature);
	TEST_ASSERT_EQUAL_UINT(128, maximum.pressure);
	TEST_ASSERT_FALSE(maximum.pressure_enabled);
}

CASE(iir, "Barometer DSP_IIR decodes filter coefficients including bypass") {
	const auto selected = barometer::decode_iir(0xea);
	TEST_ASSERT_EQUAL_UINT(3, selected.temperature);
	TEST_ASSERT_EQUAL_UINT(31, selected.pressure);
	TEST_ASSERT_EQUAL_UINT(0, barometer::decode_iir(0).temperature);
	TEST_ASSERT_EQUAL_UINT(127, barometer::decode_iir(0x38).pressure);
}

CASE(altitude, "Pressure altitude uses standard sea level pressure and rejects invalid pressure") {
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, barometer::pressure_altitude(1013.25f));
	TEST_ASSERT_FLOAT_WITHIN(1.0f, 1000, barometer::pressure_altitude(898.746f));
	TEST_ASSERT_TRUE(std::isnan(barometer::pressure_altitude(0)));
	TEST_ASSERT_TRUE(std::isnan(barometer::pressure_altitude(-1)));
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(pressure);
	RUN_TEST(temperature);
	RUN_TEST(status);
	RUN_TEST(odr);
	RUN_TEST(osr);
	RUN_TEST(iir);
	RUN_TEST(altitude);
	return UNITY_END();
}
#endif
