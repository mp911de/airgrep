#include "system/cpu_load.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[system]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(windows, "CPU readers keep independent sampling windows") {
	cpu::Window<2> health, console;
	uint64_t idle[] = {750, 1000};
	auto counter = [&](size_t core) { return idle[core]; };
	auto load = health.sample(1000, counter);
	TEST_ASSERT_EQUAL_UINT(25, load.cores[0]);
	TEST_ASSERT_EQUAL_UINT(0, load.cores[1]);
	TEST_ASSERT_EQUAL_UINT(12, load.overall);
	TEST_ASSERT_EQUAL_UINT(25, load.busiest);

	idle[1] += 1500;
	load = health.sample(2000, counter);
	TEST_ASSERT_EQUAL_UINT(100, load.cores[0]);
	TEST_ASSERT_EQUAL_UINT(0, load.cores[1]);
	TEST_ASSERT_EQUAL_UINT(50, load.overall);
	TEST_ASSERT_EQUAL_UINT(100, load.busiest);
	TEST_ASSERT_EQUAL_UINT(62, console.sample(2000, counter).busiest);
	TEST_ASSERT_EQUAL_UINT(0, health.sample(2000, counter).busiest);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(windows);
	return UNITY_END();
}
#endif
