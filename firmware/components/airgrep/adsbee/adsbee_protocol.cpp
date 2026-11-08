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

#include "adsbee_protocol.hpp"
#include <cstring>

namespace adsbee {
	bool Reply::append(const char* line, size_t size) {
		if (length > sizeof text - 2 || size > sizeof text - length - 2) {
			return false;
		}
		std::memcpy(text + length, line, size);
		length += size;
		text[length++] = '\n';
		text[length] = '\0';
		return true;
	}

	bool acceptable_command(const char* command, size_t capacity) {
		return command && *command && std::strlen(command) < capacity &&
			!std::strpbrk(command, "\r\n");
	}

	uint16_t crc16(const char* text, size_t comma) {
		uint16_t crc = 0xffff;
		for (size_t i = 0; i < comma; i++) {
			crc ^= static_cast<uint16_t>(static_cast<uint8_t>(text[i])) << 8;
			for (unsigned bit = 0; bit < 8; bit++)
				crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
		}
		return crc;
	}

	bool valid_csbee(const char* text, size_t length) {
		if (length < 5 || text[0] != '#') {
			return false;
		}
		size_t comma = length;
		while (comma && text[comma - 1] != ',') --comma;
		if (!comma || length == comma || length - comma > 4) {
			return false;
		}
		uint16_t expected = 0;
		for (size_t i = comma; i < length; i++) {
			char c = text[i];
			unsigned digit;
			if (c >= '0' && c <= '9') digit = c - '0';
			else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
			else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
			else return false;
			expected = (expected << 4) | digit;
		}
		uint16_t crc = crc16(text, comma);
		return expected == static_cast<uint16_t>((crc >> 8) | (crc << 8));
	}

	bool Power::permits_tx(int64_t now_us) const {
		/*return on && updated_us > 0 && error_us < updated_us &&
			now_us >= updated_us && now_us - updated_us <= Link::kStaleTimeoutUs;*/

		return true;
	}

	void Link::update_power(Power power, int64_t now_us) {
		poll(now_us);
		power_ = power;
		poll(now_us);
	}

	void Link::poll(int64_t now_us) {
		if (!power_.permits_tx(now_us)) {
			powered_since_us_ = received_us_ = 0;
		}
		else if (!powered_since_us_) powered_since_us_ = now_us;
	}

	void Link::received(int64_t received_us) {
		if (powered_since_us_ && received_us >= powered_since_us_ && received_us >
			received_us_)
			received_us_ = received_us;
	}

	bool Link::permits_tx(int64_t now_us) const {
		return power_.permits_tx(now_us) && received_us_ > 0 && now_us >= received_us_ &&
			now_us - received_us_ <= kRxTimeoutUs;
	}
}
