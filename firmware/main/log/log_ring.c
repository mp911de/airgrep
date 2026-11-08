#include "log_ring.h"
#include <string.h>

void log_ring_init(log_ring_t* ring, char* storage, size_t capacity) {
	*ring = (log_ring_t)
	{
		.data = storage, .capacity = capacity
	};
}

void log_ring_append(log_ring_t* ring, const char* text, size_t length) {
	for (size_t i = 0; i < length; i++) {
		if (ring->dropping) {
			if (text[i] == '\n') ring->dropping = false;
			continue;
		}
		if (ring->write - ring->first == ring->capacity) {
			if (ring->first == ring->end) {
				ring->write = ring->end;
				ring->dropping = text[i] != '\n';
				continue;
			}
			while (ring->data[ring->first++ % ring->capacity] != '\n') {
			}
		}
		ring->data[ring->write++ % ring->capacity] = text[i];
		if (text[i] == '\n') ring->end = ring->write;
	}
}

static char at(const log_ring_t* ring, uint64_t position) {
	return ring->data[position % ring->capacity];
}

static bool matches(const log_ring_t* ring, uint64_t begin, uint64_t end,
					const char* tags) {
	if (!tags || !*tags) return true;
	if (at(ring, begin) == '\033') {
		while (begin < end && at(ring, begin++) != 'm') {
		}
	}
	if (begin + 2 >= end || !strchr("EWIDV", at(ring, begin)) ||
		at(ring, begin + 1) != ' ')
		return false;
	begin += 2;
	if (at(ring, begin) == '(') {
		while (begin < end && at(ring, begin++) != ')') {
		}
		if (begin < end && at(ring, begin) == ' ') begin++;
	}
	uint64_t colon = begin;
	while (colon < end && at(ring, colon) != ':') colon++;
	if (colon == end) return false;
	while (*tags) {
		size_t length = strcspn(tags, ",");
		if (length == colon - begin) {
			size_t i = 0;
			while (i < length && tags[i] == at(ring, begin + i)) i++;
			if (i == length) return true;
		}
		tags += length;
		if (*tags) tags++;
	}
	return false;
}

uint64_t log_ring_begin(const log_ring_t* ring, const char* tags, size_t last) {
	if (last == 0) return ring->first;
	uint64_t end = ring->end;
	while (end > ring->first) {
		uint64_t begin = end - 1;
		while (begin > ring->first && at(ring, begin - 1) != '\n') begin--;
		if (matches(ring, begin, end, tags) && --last == 0) return begin;
		end = begin;
	}
	return ring->first;
}

bool log_ring_next(const log_ring_t* ring, uint64_t* cursor, const char* tags,
					char* line, size_t capacity) {
	if (*cursor < ring->first) *cursor = ring->first;
	if (capacity == 0) return false;
	while (*cursor < ring->end) {
		uint64_t begin = *cursor;
		size_t length = 0;
		while (*cursor < ring->end) {
			char c = at(ring, (*cursor)++);
			if (length + 1 < capacity) line[length++] = c;
			if (c == '\n') break;
		}
		if (matches(ring, begin, *cursor, tags)) {
			static const char suffix[] = " [truncated]\n";
			if (*cursor - begin > length && capacity >= sizeof suffix) {
				memcpy(line + length - sizeof suffix + 1, suffix, sizeof suffix - 1);
			}
			line[length] = '\0';
			return true;
		}
	}
	return false;
}
