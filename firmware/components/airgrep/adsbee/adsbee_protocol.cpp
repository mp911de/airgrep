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
#include "csbee_fields.hpp"
#include <cstdlib>
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

	namespace {
		constexpr char kGnssPrefix[] = "#GNSS:";
		constexpr size_t kGnssFields = 34;
		using csbee::Field;
		using csbee::split;
		using csbee::integer;
		using csbee::decimal;
		using csbee::copy;
	}

	bool decode_gnss(const char* text, size_t length, Gnss& out) {
		if (length < sizeof kGnssPrefix - 1 || std::memcmp(
			text, kGnssPrefix, sizeof kGnssPrefix - 1) != 0)
			return false;
		Field f[kGnssFields];
		if (!split(text, length, sizeof kGnssPrefix - 1, kGnssFields, f)) return false;
		out = Gnss{};
		out.fix = integer(f[0], 0) == 1;
		out.quality = int8_t(integer(f[1], -1));
		out.satellites = int8_t(integer(f[2], -1));
		if (out.fix) {
			out.latitude = decimal(f[3], 0);
			out.longitude = decimal(f[4], 0);
			out.altitude_ft = int32_t(integer(f[5], 0));
			out.speed_kt = decimal(f[6], 0);
			out.track_deg = decimal(f[7], 0);
		}
		out.hdop = decimal(f[8], -1);
		if (f[9].length == 6) {
			out.utc_known = true;
			out.utc_hour = uint8_t((f[9].text[0] - '0') * 10 + (f[9].text[1] - '0'));
			out.utc_minute = uint8_t((f[9].text[2] - '0') * 10 + (f[9].text[3] - '0'));
			out.utc_second = uint8_t((f[9].text[4] - '0') * 10 + (f[9].text[5] - '0'));
		}
		if (!f[10].empty()) {
			char buffer[24];
			copy(f[10], buffer, sizeof buffer);
			out.epoch_ms = std::strtoull(buffer, nullptr, 10);
		}
		out.antenna = int8_t(integer(f[11], -1));
		out.antenna_power = int8_t(integer(f[12], -1));
		out.jamming = int8_t(integer(f[13], -1));
		int16_t* counts[] = {
			&out.max_cno, &out.seen, &out.no_signal, &out.searching, &out.acquired,
			&out.unusable, &out.locked, &out.ephemeris, &out.used, &out.healthy,
			&out.unhealthy
		};
		for (size_t i = 0; i < 11; i++) *counts[i] = int16_t(integer(f[14 + i], -1));
		copy(f[25], out.sbas_mode, sizeof out.sbas_mode);
		copy(f[26], out.sbas_system, sizeof out.sbas_system);
		out.sbas_satellites = int16_t(integer(f[27], -1));
		out.sbas_corrections = !f[28].empty() && !f[29].empty();
		out.prc_cm = int32_t(integer(f[28], 0));
		out.ic_cm = int32_t(integer(f[29], 0));
		copy(f[30], out.module, sizeof out.module);
		copy(f[31], out.software, sizeof out.software);
		copy(f[32], out.hardware, sizeof out.hardware);
		out.bbr = f[33].length == 2 && std::memcmp(f[33].text, "OK", 2) == 0;
		return true;
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
