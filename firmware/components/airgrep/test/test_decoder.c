#include <string.h>

#include "adsbee/adsbee_decoder.h"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[adsbee]")
#else
#define CASE(name, description) static void name(void)
void setUp(void) {}
void tearDown(void) {}
#endif

typedef struct {
	int count;
	char last[64];
	decoder_kind_t last_kind;
} capture_t;

static void capture(const char* text, size_t len, decoder_kind_t kind,
					void* ctx) {
	capture_t* c = ctx;
	c->count++;
	TEST_ASSERT_EQUAL_UINT(strlen(text), len);
	strlcpy(c->last, text, sizeof c->last);
	c->last_kind = kind;
}

CASE(complete_line, "Complete line is delivered as an AT reply") {
	capture_t c = {0};
	decoder_t d;
	adsbee_decoder_init(&d, capture, &c);
	adsbee_decoder_feed(&d, "OK\n", 3);
	TEST_ASSERT_EQUAL(1, c.count);
	TEST_ASSERT_EQUAL_STRING("OK", c.last);
	TEST_ASSERT_EQUAL(DECODER_AT_REPLY, c.last_kind);
}

CASE(crlf, "CRLF line is delivered without CR") {
	capture_t c = {0};
	decoder_t d;
	adsbee_decoder_init(&d, capture, &c);
	adsbee_decoder_feed(&d, "OK\r\n", 4);
	TEST_ASSERT_EQUAL(1, c.count);
	TEST_ASSERT_EQUAL_STRING("OK", c.last);
}

CASE(csbee, "Hash prefix classifies a CSBEE line") {
	capture_t c = {0};
	decoder_t d;
	adsbee_decoder_init(&d, capture, &c);
	adsbee_decoder_feed(&d, "#A:ABC\n", 7);
	TEST_ASSERT_EQUAL(DECODER_CSBEE, c.last_kind);
	TEST_ASSERT_EQUAL_STRING("#A:ABC", c.last);
}

CASE(split_and_empty, "Lines split across chunks and empty lines are skipped") {
	capture_t c = {0};
	decoder_t d;
	adsbee_decoder_init(&d, capture, &c);
	adsbee_decoder_feed(&d, "AT+", 3);
	adsbee_decoder_feed(&d, "X\n\n\r\n", 5);
	TEST_ASSERT_EQUAL(1, c.count);
	TEST_ASSERT_EQUAL_STRING("AT+X", c.last);
}

CASE(after_error,
	 "After an error the partial line is dropped through the next LF") {
	capture_t c = {0};
	decoder_t d;
	adsbee_decoder_init(&d, capture, &c);
	adsbee_decoder_feed(&d, "garb", 4);
	adsbee_decoder_error(&d);
	adsbee_decoder_feed(&d, "age\nOK\n", 7);
	TEST_ASSERT_EQUAL(1, c.count);
	TEST_ASSERT_EQUAL_STRING("OK", c.last);
}

CASE(overlong, "Overlong line is discarded") {
	capture_t c = {0};
	decoder_t d;
	adsbee_decoder_init(&d, capture, &c);
	char big[ADSBEE_DECODER_SIZE + 10];
	memset(big, 'x', sizeof big);
	adsbee_decoder_feed(&d, big, sizeof big);
	adsbee_decoder_feed(&d, "\nOK\n", 4);
	TEST_ASSERT_EQUAL(1, c.count);
	TEST_ASSERT_EQUAL_STRING("OK", c.last);
}

#ifndef ESP_PLATFORM
int main(void) {
	UNITY_BEGIN();
	RUN_TEST(complete_line);
	RUN_TEST(crlf);
	RUN_TEST(csbee);
	RUN_TEST(split_and_empty);
	RUN_TEST(after_error);
	RUN_TEST(overlong);
	return UNITY_END();
}
#endif
