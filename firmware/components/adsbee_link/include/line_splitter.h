#pragma once

#include <stdbool.h>
#include <stddef.h>

// Splits a byte stream into LF-terminated lines, drops CR, and resynchronises
// after a UART error by discarding through the next clean LF.

#define LINE_SPLITTER_MAX 512

typedef void (*line_splitter_cb)(const char* line, size_t len, bool csbee, void* ctx);

typedef struct {
	char buf[LINE_SPLITTER_MAX];
	size_t len;
	bool discarding;
	line_splitter_cb on_line;
	void* ctx;
} line_splitter_t;

void line_splitter_init(line_splitter_t* s, line_splitter_cb on_line, void* ctx);
void line_splitter_feed(line_splitter_t* s, const char* data, size_t len);
void line_splitter_error(line_splitter_t* s);
