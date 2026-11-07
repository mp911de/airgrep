#include <string.h>
#include "decoder.h"
#include "unity.h"

typedef struct {
	int count;
	char last[64];
	decoder_kind_t last_kind;
} capture_t;

static void capture(const char* text, size_t len, decoder_kind_t kind, void* ctx) {
	capture_t* c = ctx;
	c->count++;
	strlcpy(c->last, text, sizeof c->last);
	c->last_kind = kind;
}

TEST_CASE (


"Complete line is delivered as an AT reply"
,
"[adsbee]"
)
{
    capture_t c = {0};
    decoder_t d;
    decoder_init(&d, capture, &c);
    decoder_feed(&d, "OK\n", 3);
    TEST_ASSERT_EQUAL(1, c.count);
    TEST_ASSERT_EQUAL_STRING("OK", c.last);
    TEST_ASSERT_EQUAL(DECODER_AT_REPLY, c.last_kind);
}

TEST_CASE (


"CRLF line is delivered without CR"
,
"[adsbee]"
)
{
    capture_t c = {0};
    decoder_t d;
    decoder_init(&d, capture, &c);
    decoder_feed(&d, "OK\r\n", 4);
    TEST_ASSERT_EQUAL(1, c.count);
    TEST_ASSERT_EQUAL_STRING("OK", c.last);
}

TEST_CASE (


"Hash prefix classifies a CSBEE line"
,
"[adsbee]"
)
{
    capture_t c = {0};
    decoder_t d;
    decoder_init(&d, capture, &c);
    decoder_feed(&d, "#A:ABC\n", 7);
    TEST_ASSERT_EQUAL(DECODER_CSBEE, c.last_kind);
    TEST_ASSERT_EQUAL_STRING("#A:ABC", c.last);
}

TEST_CASE (


"Lines split across chunks and empty lines are skipped"
,
"[adsbee]"
)
{
    capture_t c = {0};
    decoder_t d;
    decoder_init(&d, capture, &c);
    decoder_feed(&d, "AT+", 3);
    decoder_feed(&d, "X\n\n\r\n", 5);
    TEST_ASSERT_EQUAL(1, c.count);
    TEST_ASSERT_EQUAL_STRING("AT+X", c.last);
}

TEST_CASE (


"After an error the partial line is dropped through the next LF"
,
"[adsbee]"
)
{
    capture_t c = {0};
    decoder_t d;
    decoder_init(&d, capture, &c);
    decoder_feed(&d, "garb", 4);
    decoder_error(&d);
    decoder_feed(&d, "age\nOK\n", 7);
    TEST_ASSERT_EQUAL(1, c.count);
    TEST_ASSERT_EQUAL_STRING("OK", c.last);
}

TEST_CASE (


"Overlong line is discarded"
,
"[adsbee]"
)
{
    capture_t c = {0};
    decoder_t d;
    decoder_init(&d, capture, &c);
    char big[DECODER_MAX + 10];
    memset(big, 'x', sizeof big);
    decoder_feed(&d, big, sizeof big);
    decoder_feed(&d, "\nOK\n", 4);
    TEST_ASSERT_EQUAL(1, c.count);
    TEST_ASSERT_EQUAL_STRING("OK", c.last);
}
