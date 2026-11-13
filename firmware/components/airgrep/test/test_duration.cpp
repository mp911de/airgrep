#include "duration.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[duration]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

namespace {
const char* formatted(float hours) {
	static char text[16];
	format_hours_minutes(hours, text, sizeof text);
	return text;
}
}  // namespace

CASE(rounding, "Hours format as HH:MM, half a minute rounds up") {
	TEST_ASSERT_EQUAL_STRING("06:37", formatted(6.61f));
	TEST_ASSERT_EQUAL_STRING("06:36", formatted(6.6f));
	TEST_ASSERT_EQUAL_STRING("00:08", formatted(0.125f));
	TEST_ASSERT_EQUAL_STRING("00:07", formatted(0.124f));
	TEST_ASSERT_EQUAL_STRING("00:00", formatted(0));
}

CASE(carry, "Rounding carries into the hour and hours widen past 99") {
	TEST_ASSERT_EQUAL_STRING("02:00", formatted(1.999f));
	TEST_ASSERT_EQUAL_STRING("192:18", formatted(192.3f));
}

CASE(length, "Formatting returns the characters written") {
	char text[16];
	TEST_ASSERT_EQUAL(5, format_hours_minutes(6.61f, text, sizeof text));
	TEST_ASSERT_EQUAL(6, format_hours_minutes(192.3f, text, sizeof text));
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(rounding);
	RUN_TEST(carry);
	RUN_TEST(length);
	return UNITY_END();
}
#endif
