#include "system/power_mode.h"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[power]")
#else
#define CASE(name, description) static void name(void)
void setUp(void) {}
void tearDown(void) {}
#endif

static const power_input_t yes = {true, true}, no = {true, false},
						   unread = {false, false};

CASE(modes, "Power mode follows the operating mode table") {
	TEST_ASSERT_EQUAL(POWER_MODE_CHARGER, power_mode_decode(no, yes));
	TEST_ASSERT_EQUAL(POWER_MODE_ON, power_mode_decode(yes, yes));
	TEST_ASSERT_EQUAL(POWER_MODE_ON, power_mode_decode(yes, no));
	TEST_ASSERT_EQUAL(POWER_MODE_OFF, power_mode_decode(no, no));
}

CASE(unread_inputs, "Power mode is unknown without the deciding input") {
	TEST_ASSERT_EQUAL(POWER_MODE_UNKNOWN, power_mode_decode(unread, yes));
	TEST_ASSERT_EQUAL(POWER_MODE_UNKNOWN, power_mode_decode(no, unread));
	TEST_ASSERT_EQUAL(POWER_MODE_ON, power_mode_decode(yes, unread));
}

CASE(chimes, "Entering ON says hello and only leaving ON says goodbye") {
	TEST_ASSERT_EQUAL_STRING(
		"startup", power_mode_chime(POWER_MODE_UNKNOWN, POWER_MODE_ON));
	TEST_ASSERT_EQUAL_STRING(
		"startup", power_mode_chime(POWER_MODE_CHARGER, POWER_MODE_ON));
	TEST_ASSERT_EQUAL_STRING(
		"shutdown", power_mode_chime(POWER_MODE_ON, POWER_MODE_CHARGER));
	TEST_ASSERT_EQUAL_STRING("shutdown",
							 power_mode_chime(POWER_MODE_ON, POWER_MODE_OFF));
	TEST_ASSERT_NULL(power_mode_chime(POWER_MODE_UNKNOWN, POWER_MODE_CHARGER));
	TEST_ASSERT_NULL(power_mode_chime(POWER_MODE_UNKNOWN, POWER_MODE_OFF));
	TEST_ASSERT_NULL(power_mode_chime(POWER_MODE_CHARGER, POWER_MODE_OFF));
	TEST_ASSERT_NULL(power_mode_chime(POWER_MODE_ON, POWER_MODE_ON));
}

CASE(names, "Power mode names match the console output") {
	TEST_ASSERT_EQUAL_STRING("UNKNOWN", power_mode_name(POWER_MODE_UNKNOWN));
	TEST_ASSERT_EQUAL_STRING("ON", power_mode_name(POWER_MODE_ON));
	TEST_ASSERT_EQUAL_STRING("CHARGER", power_mode_name(POWER_MODE_CHARGER));
	TEST_ASSERT_EQUAL_STRING("OFF", power_mode_name(POWER_MODE_OFF));
}

#ifndef ESP_PLATFORM
int main(void) {
	UNITY_BEGIN();
	RUN_TEST(modes);
	RUN_TEST(unread_inputs);
	RUN_TEST(chimes);
	RUN_TEST(names);
	return UNITY_END();
}
#endif
