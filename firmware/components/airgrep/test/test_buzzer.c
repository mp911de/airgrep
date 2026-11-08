#include "buzzer/buzzer_pattern.h"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[buzzer]")
#else
#define CASE(name, description) static void name(void)
void setUp(void) {}
void tearDown(void) {}
#endif

CASE(short_scheme, "Short beep lookup returns one 50 ms tone") {
	const buzzer_scheme_t* scheme = buzzer_scheme_find("short");
	TEST_ASSERT_NOT_NULL(scheme);
	TEST_ASSERT_EQUAL_UINT(1, scheme->count);
	TEST_ASSERT_TRUE(scheme->steps[0].on);
	TEST_ASSERT_EQUAL_UINT(50, scheme->steps[0].ms);
	TEST_ASSERT_FALSE(scheme->repeat);
	TEST_ASSERT_NULL(buzzer_scheme_find("missing"));
}

CASE(patterns, "Long double alarm and sweep have the specified tone patterns") {
	const buzzer_scheme_t* scheme = buzzer_scheme_find("long");
	TEST_ASSERT_NOT_NULL(scheme);
	TEST_ASSERT_EQUAL_UINT(300, scheme->steps[0].ms);
	scheme = buzzer_scheme_find("double");
	TEST_ASSERT_NOT_NULL(scheme);
	TEST_ASSERT_EQUAL_UINT(3, scheme->count);
	TEST_ASSERT_TRUE(scheme->steps[0].on);
	TEST_ASSERT_FALSE(scheme->steps[1].on);
	TEST_ASSERT_TRUE(scheme->steps[2].on);
	TEST_ASSERT_EQUAL_UINT(50, scheme->steps[0].ms);
	TEST_ASSERT_EQUAL_UINT(50, scheme->steps[2].ms);
	scheme = buzzer_scheme_find("alarm");
	TEST_ASSERT_NOT_NULL(scheme);
	TEST_ASSERT_TRUE(scheme->repeat);
	TEST_ASSERT_TRUE(scheme->steps[0].on);
	TEST_ASSERT_FALSE(scheme->steps[1].on);
	scheme = buzzer_scheme_find("sweep");
	TEST_ASSERT_NOT_NULL(scheme);
	TEST_ASSERT_EQUAL_UINT(11, scheme->count);
	TEST_ASSERT_EQUAL_UINT(3500, scheme->steps[0].hz);
	TEST_ASSERT_EQUAL_UINT(4000, scheme->steps[5].hz);
	TEST_ASSERT_EQUAL_UINT(4500, scheme->steps[10].hz);
	TEST_ASSERT_FALSE(scheme->repeat);
}

CASE(frequency, "Tone frequency defaults to 4 kHz and sweep ignores overrides") {
	const buzzer_scheme_t* scheme = buzzer_scheme_find("short");
	TEST_ASSERT_EQUAL_UINT(4000, buzzer_step_frequency(&scheme->steps[0], 0));
	TEST_ASSERT_EQUAL_UINT(4200, buzzer_step_frequency(&scheme->steps[0], 4200));
	scheme = buzzer_scheme_find("sweep");
	TEST_ASSERT_EQUAL_UINT(3500, buzzer_step_frequency(&scheme->steps[0], 4200));
	TEST_ASSERT_EQUAL_UINT(4500, buzzer_step_frequency(&scheme->steps[10], 4200));
	TEST_ASSERT_EQUAL_STRING("short", buzzer_scheme_at(0)->name);
	TEST_ASSERT_EQUAL_STRING("sweep", buzzer_scheme_at(4)->name);
	TEST_ASSERT_NULL(buzzer_scheme_at(5));
}

CASE(gate, "Mute and unknown suppress on while off always passes") {
	TEST_ASSERT_FALSE(buzzer_gate(BUZZER_MUTE_UNKNOWN, true));
	TEST_ASSERT_FALSE(buzzer_gate(BUZZER_MUTE_ON, true));
	TEST_ASSERT_TRUE(buzzer_gate(BUZZER_MUTE_OFF, true));
	TEST_ASSERT_TRUE(buzzer_gate(BUZZER_MUTE_UNKNOWN, false));
	TEST_ASSERT_TRUE(buzzer_gate(BUZZER_MUTE_ON, false));
	TEST_ASSERT_TRUE(buzzer_gate(BUZZER_MUTE_OFF, false));
}

#ifndef ESP_PLATFORM
int main(void) {
	UNITY_BEGIN();
	RUN_TEST(short_scheme);
	RUN_TEST(patterns);
	RUN_TEST(frequency);
	RUN_TEST(gate);
	return UNITY_END();
}
#endif
