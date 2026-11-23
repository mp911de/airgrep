#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "airgrep.h"

// Call all display functions on the UI thread.

#ifdef __cplusplus
extern "C" {
#endif

#define AIRGREP_DISPLAY_ABI 1u

typedef struct {
	int32_t width, height;
	// Shows what the connection read. A different serial starts a new display.
	void (*follow)(airgrep_connection* connection, const char* serial);
	// Lets this much time pass. True when the pixels changed.
	bool (*step)(uint32_t elapsed_ms);
	// Width times height pixels of four bytes, blue, green, red and one unused.
	const uint8_t* (*pixels)(void);
	void (*touch)(int32_t panel_x, int32_t panel_y, bool down);
	bool (*muted)(void);
	// What the display made of the last picture. Valid until the next call.
	const char* (*explained)(void);
} airgrep_display_api;

// Returns a table for the lifetime of the loaded library, or NULL for an
// unsupported ABI version or table size.
typedef const airgrep_display_api* airgrep_display_getter(uint32_t version,
														  size_t size);
typedef airgrep_display_getter* airgrep_display_getter_ptr;
airgrep_display_getter airgrep_display_get_api;

#ifdef __cplusplus
}
#endif
