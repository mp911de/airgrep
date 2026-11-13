#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>

// Writes hours as HH:MM, rounded to the nearest minute, half a minute rounds
// up. Hours widen past 99. Returns the characters written.
inline int format_hours_minutes(float hours, char* out, size_t size) {
	const long minutes = std::lround(hours * 60);
	return std::snprintf(out, size, "%02ld:%02ld", minutes / 60, minutes % 60);
}
