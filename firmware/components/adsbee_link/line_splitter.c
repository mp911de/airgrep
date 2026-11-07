#include "line_splitter.h"

void line_splitter_init(line_splitter_t* s, line_splitter_cb on_line, void* ctx) {
	s->len = 0;
	s->discarding = false;
	s->on_line = on_line;
	s->ctx = ctx;
}

void line_splitter_error(line_splitter_t* s) {
	s->len = 0;
	s->discarding = true;
}

void line_splitter_feed(line_splitter_t* s, const char* data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		char c = data[i];
		if (c == '\n') {
			if (!s->discarding && s->len > 0) {
				s->buf[s->len] = '\0';
				s->on_line(s->buf, s->len, s->buf[0] == '#', s->ctx);
			}
			s->len = 0;
			s->discarding = false;
			continue;
		}
		if (c == '\r' || s->discarding) {
			continue;
		}
		if (s->len == LINE_SPLITTER_MAX - 1) {
			line_splitter_error(s);
			continue;
		}
		s->buf[s->len++] = c;
	}
}
