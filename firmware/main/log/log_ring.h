#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	char* data;
	size_t capacity;
	uint64_t first, end, write;
	bool dropping;
} log_ring_t;

// The caller serializes access and supplies nonempty storage that outlives the ring.
void log_ring_init(log_ring_t* ring, char* storage, size_t capacity);
void log_ring_append(log_ring_t* ring, const char* text, size_t length);
uint64_t log_ring_begin(const log_ring_t* ring, const char* tags, size_t last);
bool log_ring_next(const log_ring_t* ring, uint64_t* cursor, const char* tags,
					char* line, size_t capacity);
