#include <string.h>
#include "log_ring.h"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[logs]")
#else
#define CASE(name, description) static void name(void)
void setUp(void) {}
void tearDown(void) {}
#endif

CASE(replay, "Log replay returns complete lines in order") {
	char storage[64], line[64];
	log_ring_t ring;
	log_ring_init(&ring, storage, sizeof storage);
	log_ring_append(&ring, "first\nsecond\npartial", 20);
	uint64_t cursor = log_ring_begin(&ring, NULL, 0);
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
	TEST_ASSERT_EQUAL_STRING("first\n", line);
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
	TEST_ASSERT_EQUAL_STRING("second\n", line);
	TEST_ASSERT_FALSE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
}

CASE(wrap, "Ring wrap discards whole oldest lines and resumes stale readers") {
	char storage[16], line[64];
	log_ring_t ring;
	log_ring_init(&ring, storage, sizeof storage);
	log_ring_append(&ring, "first\nsecond\n", 13);
	uint64_t cursor = log_ring_begin(&ring, NULL, 0);
	log_ring_append(&ring, "third\n", 6);
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
	TEST_ASSERT_EQUAL_STRING("second\n", line);
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
	TEST_ASSERT_EQUAL_STRING("third\n", line);
}

CASE(filter, "Tag filtering matches exact tags and ignores message text") {
	char storage[256], line[128];
	log_ring_t ring;
	log_ring_init(&ring, storage, sizeof storage);
	const char* text = "I (1) i2c_extra: no\nI (2) bee: mentions i2c\n\033[0;32mI (3) i2c: yes\033[0m\nD (4) loop: yes\n";
	log_ring_append(&ring, text, strlen(text));
	uint64_t cursor = log_ring_begin(&ring, "i2c,loop", 0);
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, "i2c,loop", line, sizeof line));
	TEST_ASSERT_EQUAL_STRING("\033[0;32mI (3) i2c: yes\033[0m\n", line);
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, "i2c,loop", line, sizeof line));
	TEST_ASSERT_EQUAL_STRING("D (4) loop: yes\n", line);
	TEST_ASSERT_FALSE(log_ring_next(&ring, &cursor, "i2c,loop", line, sizeof line));
}

CASE(last, "Replay last N counts matching complete lines") {
	char storage[128], line[64];
	log_ring_t ring;
	log_ring_init(&ring, storage, sizeof storage);
	const char* text = "I (1) i2c: old\nI (2) i2c: new\nI (3) bee: other\n";
	log_ring_append(&ring, text, strlen(text));
	uint64_t cursor = log_ring_begin(&ring, "i2c", 1);
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, "i2c", line, sizeof line));
	TEST_ASSERT_EQUAL_STRING("I (2) i2c: new\n", line);
	TEST_ASSERT_FALSE(log_ring_next(&ring, &cursor, "i2c", line, sizeof line));
}

CASE(fragments, "Fragmented logs publish only at newline and oversized lines recover") {
	char storage[16], line[32];
	log_ring_t ring;
	log_ring_init(&ring, storage, sizeof storage);
	log_ring_append(&ring, "par", 3);
	uint64_t cursor = log_ring_begin(&ring, NULL, 0);
	TEST_ASSERT_FALSE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
	log_ring_append(&ring, "tial\n", 5);
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
	TEST_ASSERT_EQUAL_STRING("partial\n", line);
	const char* oversized = "01234567890123456789\nnext\n";
	log_ring_append(&ring, oversized, strlen(oversized));
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
	TEST_ASSERT_EQUAL_STRING("next\n", line);
	TEST_ASSERT_FALSE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
}

CASE(reader_limit, "A 1024-byte reader preserves line boundaries for joined fragments") {
	char storage[2048], fragment[800], line[1024];
	memset(fragment, 'x', sizeof fragment);
	log_ring_t ring;
	log_ring_init(&ring, storage, sizeof storage);
	log_ring_append(&ring, fragment, sizeof fragment);
	log_ring_append(&ring, fragment, sizeof fragment);
	log_ring_append(&ring, "\nnext\n", 6);
	uint64_t cursor = log_ring_begin(&ring, NULL, 0);
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
	TEST_ASSERT_EQUAL_UINT(1023, strlen(line));
	TEST_ASSERT_EQUAL_STRING(" [truncated]\n", line + strlen(line) - 13);
	TEST_ASSERT_TRUE(log_ring_next(&ring, &cursor, NULL, line, sizeof line));
	TEST_ASSERT_EQUAL_STRING("next\n", line);
}

#ifndef ESP_PLATFORM
int main(void) {
	UNITY_BEGIN();
	RUN_TEST(replay);
	RUN_TEST(wrap);
	RUN_TEST(filter);
	RUN_TEST(last);
	RUN_TEST(fragments);
	RUN_TEST(reader_limit);
	return UNITY_END();
}
#endif
