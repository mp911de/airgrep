#include "decoder.h"

void decoder_init(decoder_t* d, decoder_cb on_message, void* ctx) {
	d->len = 0;
	d->discarding = false;
	d->on_message = on_message;
	d->ctx = ctx;
}

void decoder_error(decoder_t* d) {
	d->len = 0;
	d->discarding = true;
}

void decoder_feed(decoder_t* d, const char* data, size_t len) {
	for (size_t i = 0; i < len; i++) {
		char c = data[i];
		if (c == '\n') {
			if (!d->discarding && d->len > 0) {
				d->buf[d->len] = '\0';
				d->on_message(d->buf, d->len,
							d->buf[0] == '#' ? DECODER_CSBEE : DECODER_AT_REPLY,
							d->ctx);
			}
			d->len = 0;
			d->discarding = false;
			continue;
		}
		if (c == '\r' || d->discarding) {
			continue;
		}
		if (d->len == DECODER_MAX - 1) {
			decoder_error(d);
			continue;
		}
		d->buf[d->len++] = c;
	}
}
