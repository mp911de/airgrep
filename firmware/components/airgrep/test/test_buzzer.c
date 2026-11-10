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

CASE(frequency,
	 "Tone frequency defaults to 4 kHz and sweep ignores overrides") {
	const buzzer_scheme_t* scheme = buzzer_scheme_find("short");
	TEST_ASSERT_EQUAL_UINT(4000, buzzer_step_frequency(&scheme->steps[0], 0));
	TEST_ASSERT_EQUAL_UINT(4200,
						   buzzer_step_frequency(&scheme->steps[0], 4200));
	scheme = buzzer_scheme_find("sweep");
	TEST_ASSERT_EQUAL_UINT(3500,
						   buzzer_step_frequency(&scheme->steps[0], 4200));
	TEST_ASSERT_EQUAL_UINT(4500,
						   buzzer_step_frequency(&scheme->steps[10], 4200));
	TEST_ASSERT_EQUAL_STRING("short", buzzer_scheme_at(0)->name);
	TEST_ASSERT_EQUAL_STRING("sweep", buzzer_scheme_at(4)->name);
	TEST_ASSERT_EQUAL_STRING("startup", buzzer_scheme_at(5)->name);
	TEST_ASSERT_EQUAL_STRING("shutdown", buzzer_scheme_at(6)->name);
	TEST_ASSERT_NULL(buzzer_scheme_at(7));
}

CASE(startup,
	 "Startup chime rises to resonance at reduced volume, others play full") {
	const buzzer_scheme_t* scheme = buzzer_scheme_find("startup");
	TEST_ASSERT_NOT_NULL(scheme);
	TEST_ASSERT_EQUAL_UINT(5, scheme->count);
	TEST_ASSERT_FALSE(scheme->repeat);
	TEST_ASSERT_EQUAL_UINT(3200, scheme->steps[0].hz);
	TEST_ASSERT_FALSE(scheme->steps[1].on);
	TEST_ASSERT_EQUAL_UINT(4000, scheme->steps[4].hz);
	TEST_ASSERT_EQUAL_UINT(10, scheme->volume);
	TEST_ASSERT_EQUAL_UINT(100, buzzer_scheme_find("short")->volume);
	TEST_ASSERT_EQUAL_UINT(512, buzzer_volume_duty(100, 512));
	TEST_ASSERT_EQUAL_UINT(204, buzzer_volume_duty(40, 512));
	TEST_ASSERT_EQUAL_UINT(0, buzzer_volume_duty(0, 512));
	TEST_ASSERT_EQUAL_UINT(512, buzzer_volume_duty(255, 512));
}

CASE(shutdown, "Shutdown chime is the startup chime in reverse") {
	const buzzer_scheme_t* up = buzzer_scheme_find("startup");
	const buzzer_scheme_t* down = buzzer_scheme_find("shutdown");
	TEST_ASSERT_NOT_NULL(down);
	TEST_ASSERT_EQUAL_UINT(up->count, down->count);
	TEST_ASSERT_EQUAL_UINT(up->volume, down->volume);
	TEST_ASSERT_FALSE(down->repeat);
	for (size_t i = 0; i < down->count; i++) {
		const buzzer_step_t* a = &up->steps[up->count - 1 - i];
		const buzzer_step_t* b = &down->steps[i];
		TEST_ASSERT_EQUAL(a->on, b->on);
		TEST_ASSERT_EQUAL_UINT(a->ms, b->ms);
		TEST_ASSERT_EQUAL_UINT(a->hz, b->hz);
	}
	TEST_ASSERT_EQUAL_UINT(4000, down->steps[0].hz);
	TEST_ASSERT_EQUAL_UINT(3200, down->steps[4].hz);
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
	RUN_TEST(startup);
	RUN_TEST(shutdown);
	RUN_TEST(gate);
	return UNITY_END();
}
#endif
