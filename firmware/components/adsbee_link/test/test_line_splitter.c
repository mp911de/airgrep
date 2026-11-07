#include <string.h>
#include "line_splitter.h"
#include "unity.h"

typedef struct {
	int count;
	char last[64];
	bool last_csbee;
} capture_t;

static void capture(const char* line, size_t len, bool csbee, void* ctx) {
	capture_t* c = ctx;
	c->count++;
	strlcpy(c->last, line, sizeof c->last);
	c->last_csbee = csbee;
}

TEST_CASE (
"CRLF line is delivered without CR"
,
"[adsbee]"
)
{
    capture_t c = {0};
    line_splitter_t s;
    line_splitter_init(&s, capture, &c);
    line_splitter_feed(&s, "OK\r\n", 4);
    TEST_ASSERT_EQUAL(1, c.count);
    TEST_ASSERT_EQUAL_STRING("OK", c.last);
    TEST_ASSERT_FALSE(c.last_csbee);
}

TEST_CASE (
"Hash prefix marks a CSBEE line"
,
"[adsbee]"
)
{
    capture_t c = {0};
    line_splitter_t s;
    line_splitter_init(&s, capture, &c);
    line_splitter_feed(&s, "#A:ABC\n", 7);
    TEST_ASSERT_TRUE(c.last_csbee);
}

TEST_CASE (
"Lines split across chunks and empty lines are skipped"
,
"[adsbee]"
)
{
    capture_t c = {0};
    line_splitter_t s;
    line_splitter_init(&s, capture, &c);
    line_splitter_feed(&s, "AT+", 3);
    line_splitter_feed(&s, "X\n\n", 3);
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
    line_splitter_t s;
    line_splitter_init(&s, capture, &c);
    line_splitter_feed(&s, "garb", 4);
    line_splitter_error(&s);
    line_splitter_feed(&s, "age\nOK\n", 7);
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
    line_splitter_t s;
    line_splitter_init(&s, capture, &c);
    char big[LINE_SPLITTER_MAX + 10];
    memset(big, 'x', sizeof big);
    line_splitter_feed(&s, big, sizeof big);
    line_splitter_feed(&s, "\nOK\n", 4);
    TEST_ASSERT_EQUAL(1, c.count);
    TEST_ASSERT_EQUAL_STRING("OK", c.last);
}
