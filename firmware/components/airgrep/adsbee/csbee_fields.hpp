#pragma once

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

// Field access for CSBEE lines: "#PREFIX:a,b,c,...,CRC". Numeric parsers treat
// the comma as the field boundary.
namespace csbee {
struct Field {
	const char* text;
	size_t length;
	bool empty() const { return length == 0; }
};

// Splits the payload between the prefix and the CRC's comma into exactly count
// fields.
inline bool split(const char* text, size_t length, size_t prefix, size_t count,
				  Field* fields) {
	size_t comma = length;
	while (comma && text[comma - 1] != ',') {
		--comma;
	}
	if (!comma || comma < prefix) {
		return false;
	}
	const size_t end = comma - 1;
	size_t found = 0, begin = prefix;
	for (size_t i = prefix; i <= end; i++) {
		if (i == end || text[i] == ',') {
			if (found == count) {
				return false;
			}
			fields[found++] = {text + begin, i - begin};
			begin = i + 1;
		}
	}
	return found == count;
}

inline void copy(const Field& field, char* out, size_t capacity) {
	const size_t n = field.length < capacity - 1 ? field.length : capacity - 1;
	std::memcpy(out, field.text, n);
	out[n] = 0;
}

template <typename T>
bool integer(Field field, T& value, int base = 10) {
	const char* end = field.text + field.length;
	const auto result = std::from_chars(field.text, end, value, base);
	return result.ec == std::errc{} && result.ptr == end;
}

template <typename T>
	requires std::is_enum_v<T>
bool integer(Field field, T& value, int base = 10) {
	std::underlying_type_t<T> code;
	if (!integer(field, code, base)) {
		return false;
	}
	value = static_cast<T>(code);
	return true;
}

template <typename T>
bool optional_integer(Field field, T& value) {
	return field.empty() || integer(field, value);
}

inline bool decimal(Field field, double& value) {
	char buffer[64];
	if (field.empty() || field.length >= sizeof buffer) {
		return false;
	}
	copy(field, buffer, sizeof buffer);
	if (std::strspn(buffer, "0123456789+-.eE") != field.length) {
		return false;
	}
	char* end = nullptr;
	errno = 0;
	value = std::strtod(buffer, &end);
	return !errno && end == buffer + field.length && std::isfinite(value);
}

inline bool decimal(Field field, float& value,
					double limit = std::numeric_limits<float>::max()) {
	double parsed = NAN;
	if (!decimal(field, parsed) || std::abs(parsed) > limit) {
		return false;
	}
	value = static_cast<float>(parsed);
	return value != 0 || parsed == 0;
}

inline bool position(Field lat, Field lon, float& latitude, float& longitude) {
	return decimal(lat, latitude, 90) && decimal(lon, longitude, 180);
}
}
