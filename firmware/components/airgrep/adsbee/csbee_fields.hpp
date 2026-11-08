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

#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstring>

// Field access for CSBEE lines: "#PREFIX:a,b,c,...,CRC". Numeric parsers treat an empty
// field as absent and return the fallback.
namespace csbee {
	struct Field {
		const char* text;
		size_t length;
		bool empty() const { return length == 0; }
	};

	// Splits the payload between the prefix and the CRC's comma into exactly count fields.
	inline bool split(const char* text, size_t length, size_t prefix, size_t count,
					Field* fields) {
		size_t comma = length;
		while (comma && text[comma - 1] != ',') --comma;
		if (!comma || comma < prefix) return false;
		size_t end = comma - 1, found = 0, begin = prefix;
		for (size_t i = prefix; i <= end; i++) {
			if (i == end || text[i] == ',') {
				if (found == count) return false;
				fields[found++] = {text + begin, i - begin};
				begin = i + 1;
			}
		}
		return found == count;
	}

	inline void copy(const Field& field, char* out, size_t capacity) {
		size_t n = field.length < capacity - 1 ? field.length : capacity - 1;
		std::memcpy(out, field.text, n);
		out[n] = 0;
	}

	inline long integer(const Field& field, long fallback = 0) {
		if (field.empty()) return fallback;
		char buffer[24];
		copy(field, buffer, sizeof buffer);
		return std::strtol(buffer, nullptr, 10);
	}

	inline unsigned long hex(const Field& field) {
		if (field.empty()) return 0;
		char buffer[24];
		copy(field, buffer, sizeof buffer);
		return std::strtoul(buffer, nullptr, 16);
	}

	inline float decimal(const Field& field, float fallback = 0) {
		if (field.empty()) return fallback;
		char buffer[24];
		copy(field, buffer, sizeof buffer);
		return std::strtof(buffer, nullptr);
	}
}
