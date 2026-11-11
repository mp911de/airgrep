#include "system/threshold.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[threshold]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(rising,
	 "Rising threshold trips at its limit and releases at its release point") {
	Threshold hot{40, 38};
	TEST_ASSERT_FALSE(hot.update(39.9f));
	TEST_ASSERT_TRUE(hot.update(40));
	TEST_ASSERT_TRUE(hot.update(38.1f));
	TEST_ASSERT_FALSE(hot.update(38));
}

CASE(falling,
	 "Falling threshold trips at its limit and releases at its release point") {
	Threshold cold{4, 6};
	TEST_ASSERT_FALSE(cold.update(4.1f));
	TEST_ASSERT_TRUE(cold.update(4));
	TEST_ASSERT_TRUE(cold.update(5.9f));
	TEST_ASSERT_FALSE(cold.update(6));
}

CASE(gap, "Threshold holds its state inside the gap in both directions") {
	Threshold warm{33, 31};
	TEST_ASSERT_FALSE(warm.update(32));
	TEST_ASSERT_FALSE(warm.tripped());
	warm.update(35);
	TEST_ASSERT_TRUE(warm.update(32));
	TEST_ASSERT_TRUE(warm.tripped());
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(rising);
	RUN_TEST(falling);
	RUN_TEST(gap);
	return UNITY_END();
}
#endif
