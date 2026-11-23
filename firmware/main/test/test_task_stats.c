#include "task_stats.h"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[system]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(pairing,
	 "Tasks pair by kernel task number, sort by CPU and report created and "
	 "deleted ones") {
	task_sample_t before[] = {
		{0, "loop", 100, 0, 10, 2, 2816},
		{1, "IDLE0", 900, 0, 0, 1, 1024},
		{2, "gone", 50, 1, 5, 2, 512},
	};
	task_sample_t after[] = {
		{1, "IDLE0", 1700, 0, 0, 1, 1024},
		{0, "loop", 300, 0, 10, 2, 2800},
		{3, "new", 10, 1, 5, 2, 256},
	};
	task_row_t rows[8];
	size_t count = task_stats_rows(before, 3, after, 3, 1000, rows, 8);
	TEST_ASSERT_EQUAL_UINT(4, count);
	TEST_ASSERT_EQUAL(TASK_ROW_SAMPLED, rows[0].kind);
	TEST_ASSERT_EQUAL_STRING("IDLE0", rows[0].sample->name);
	TEST_ASSERT_EQUAL_UINT(80, rows[0].core_cpu_percent);
	TEST_ASSERT_EQUAL_STRING("loop", rows[1].sample->name);
	TEST_ASSERT_EQUAL_UINT(20, rows[1].core_cpu_percent);
	TEST_ASSERT_EQUAL(TASK_ROW_CREATED, rows[2].kind);
	TEST_ASSERT_EQUAL_STRING("new", rows[2].sample->name);
	TEST_ASSERT_EQUAL(TASK_ROW_DELETED, rows[3].kind);
	TEST_ASSERT_EQUAL_STRING("gone", rows[3].sample->name);
}

CASE(capacity, "Rows stop at the capacity") {
	task_sample_t before[] = {{0, "a", 0, 0, 1, 1, 0}};
	task_sample_t after[] = {{0, "a", 10, 0, 1, 1, 0},
							 {1, "b", 10, 0, 1, 1, 0}};
	task_row_t rows[1];
	TEST_ASSERT_EQUAL_UINT(1,
						   task_stats_rows(before, 1, after, 2, 100, rows, 1));
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(pairing);
	RUN_TEST(capacity);
	return UNITY_END();
}
#endif
