#include "adsbee_protocol.hpp"

#include <cstring>
#include <iterator>

#include "csbee_fields.hpp"

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

namespace {
uint16_t crc16(const char* text, size_t comma) {
	uint16_t crc = 0xffff;
	for (size_t i = 0; i < comma; i++) {
		crc ^= static_cast<uint16_t>(static_cast<uint8_t>(text[i])) << 8;
		for (unsigned bit = 0; bit < 8; bit++) {
			crc = crc & 0x8000 ? (crc << 1) ^ 0x1021 : crc << 1;
		}
	}
	return crc;
}
}

bool valid_csbee(const char* text, size_t length) {
	if (length < 5 || text[0] != '#') {
		return false;
	}
	size_t comma = length;
	while (comma && text[comma - 1] != ',') {
		--comma;
	}
	if (!comma || length == comma || length - comma > 4) {
		return false;
	}
	uint16_t expected = 0;
	for (size_t i = comma; i < length; i++) {
		const char c = text[i];
		unsigned digit = 0;
		if (c >= '0' && c <= '9') {
			digit = c - '0';
		} else if (c >= 'A' && c <= 'F') {
			digit = c - 'A' + 10;
		} else if (c >= 'a' && c <= 'f') {
			digit = c - 'a' + 10;
		} else {
			return false;
		}
		expected = (expected << 4) | digit;
	}
	const uint16_t crc = crc16(text, comma);
	return expected == static_cast<uint16_t>((crc >> 8) | (crc << 8));
}

namespace {
constexpr char kGnssPrefix[] = "#GNSS:";
constexpr size_t kGnssFields = 34;
using csbee::copy;
using csbee::decimal;
using csbee::Field;
using csbee::integer;
using csbee::optional_integer;
using csbee::split;
}

bool decode_gnss(const char* text, size_t length, Gnss& out) {
	if (length < sizeof kGnssPrefix - 1 ||
		std::memcmp(text, kGnssPrefix, sizeof kGnssPrefix - 1) != 0) {
		return false;
	}
	Field f[kGnssFields];
	if (!split(text, length, sizeof kGnssPrefix - 1, kGnssFields, f)) {
		return false;
	}
	out = Gnss{};
	// The frame says fix or none, not which kind.
	uint8_t fix = 0;
	if (!integer(f[0], fix) || fix > 1 ||
		!optional_integer(f[1], out.quality) ||
		!optional_integer(f[2], out.satellites_used)) {
		return false;
	}
	if (fix) {
		if (!csbee::position(f[3], f[4], out.latitude, out.longitude) ||
			!integer(f[5], out.altitude_ft) || !decimal(f[6], out.speed_kt) ||
			!decimal(f[7], out.track_deg)) {
			return false;
		}
		out.fix = Fix::fix_3d;
	}
	if (!f[8].empty() && !decimal(f[8], out.hdop)) {
		return false;
	}
	if (!f[9].empty()) {
		const char* utc = f[9].text;
		if (f[9].length != 6 || !integer({utc, 2}, out.utc_hour) ||
			!integer({utc + 2, 2}, out.utc_minute) ||
			!integer({utc + 4, 2}, out.utc_second) || out.utc_hour > 23 ||
			out.utc_minute > 59 || out.utc_second > 60) {
			return false;
		}
		out.utc_known = true;
	}
	if (!optional_integer(f[10], out.epoch_ms) ||
		!optional_integer(f[11], out.antenna) ||
		!optional_integer(f[12], out.antenna_power) ||
		!optional_integer(f[13], out.jamming)) {
		return false;
	}
	int16_t* const counts[] = {
		&out.max_cno_dbhz, &out.seen,     &out.no_signal, &out.searching,
		&out.acquired,     &out.unusable, &out.locked,    &out.ephemeris,
		&out.used,         &out.healthy,  &out.unhealthy};
	for (size_t i = 0; i < std::size(counts); i++) {
		if (!optional_integer(f[14 + i], *counts[i])) {
			return false;
		}
	}
	copy(f[25], out.sbas_mode, sizeof out.sbas_mode);
	copy(f[26], out.sbas_system, sizeof out.sbas_system);
	if (!optional_integer(f[27], out.sbas_satellites) ||
		!optional_integer(f[28], out.prc_cm) ||
		!optional_integer(f[29], out.ic_cm)) {
		return false;
	}
	out.sbas_corrections = !f[28].empty() && !f[29].empty();
	copy(f[30], out.module, sizeof out.module);
	copy(f[31], out.software, sizeof out.software);
	copy(f[32], out.hardware, sizeof out.hardware);
	out.bbr = f[33].length == 2 && std::memcmp(f[33].text, "OK", 2) == 0;
	return true;
}

void Link::set_powered(bool powered, int64_t now_us) {
	if (!powered) {
		powered_since_us_ = received_us_ = 0;
	} else if (!powered_since_us_) {
		powered_since_us_ = now_us;
	}
}

void Link::received(int64_t received_us) {
	if (powered_since_us_ && received_us >= powered_since_us_ &&
		received_us > received_us_) {
		received_us_ = received_us;
	}
}

bool Link::permits_tx(int64_t now_us) const {
	return powered_since_us_ > 0 && received_us_ > 0 &&
		   now_us >= received_us_ && now_us - received_us_ <= kRxTimeoutUs;
}
}
