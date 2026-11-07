#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Turns the ADSBee byte stream into LF-terminated messages, drops CR, skips
// empty lines and resynchronises after a UART error or an overlong line by
// discarding through the next LF. Lines starting with '#' are CSBEE reports,
// everything else is an AT reply.

#define DECODER_MAX 512

typedef enum {
	DECODER_AT_REPLY,
	DECODER_CSBEE,
} decoder_kind_t;

typedef void (*decoder_cb)(const char* text, size_t len, decoder_kind_t kind, void* ctx);

typedef struct {
	char buf[DECODER_MAX];
	size_t len;
	bool discarding;
	decoder_cb on_message;
	void* ctx;
} decoder_t;

void decoder_init(decoder_t* d, decoder_cb on_message, void* ctx);
void decoder_feed(decoder_t* d, const char* data, size_t len);
void decoder_error(decoder_t* d);

#ifdef __cplusplus
}
#endif
