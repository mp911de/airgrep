// Copyright 2026 the original author or authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "adsbee_decoder.h"

void adsbee_decoder_init(decoder_t* d, decoder_cb on_message, void* ctx) {
	d->len = 0;
	d->discarding = false;
	d->on_message = on_message;
	d->ctx = ctx;
}

void adsbee_decoder_error(decoder_t* d) {
	d->len = 0;
	d->discarding = true;
}

void adsbee_decoder_feed(decoder_t* d, const char* data, size_t len) {
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
		if (d->len == ADSBEE_DECODER_SIZE - 1) {
			adsbee_decoder_error(d);
			continue;
		}
		d->buf[d->len++] = c;
	}
}
