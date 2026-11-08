#include "wifi_protocol.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace wifi {
	bool operator==(const Config& a, const Config& b) {
		return a.enabled == b.enabled && a.channel == b.channel && std::strcmp(a.ssid, b.ssid) == 0 &&
			std::strcmp(a.password, b.password) == 0;
	}

	bool decode_config(const char* text, size_t length, Config& out) {
		constexpr size_t prefix = sizeof WIFI_AP_REPLY - 1;
		if (length < prefix || std::memcmp(text, WIFI_AP_REPLY, prefix) != 0) return false;
		char buffer[8 + WIFI_AP_SSID_SIZE + WIFI_AP_PASSWORD_SIZE];
		size_t n = length - prefix < sizeof buffer - 1 ? length - prefix : sizeof buffer - 1;
		std::memcpy(buffer, text + prefix, n);
		buffer[n] = 0;
		char* fields[4] = {buffer};
		for (int i = 1; i < 4; i++) {
			char* comma = std::strchr(fields[i - 1], ',');
			if (!comma) return false;
			*comma = 0;
			fields[i] = comma + 1;
		}
		if (!*fields[0] || !*fields[3]) return false;
		out = Config{};
		out.enabled = std::strtol(fields[0], nullptr, 10) != 0;
		std::strncpy(out.ssid, fields[1], sizeof out.ssid - 1);
		std::strncpy(out.password, fields[2], sizeof out.password - 1);
		out.channel = uint8_t(std::strtoul(fields[3], nullptr, 10));
		return true;
	}

	bool format_set(const Config& config, char* out, size_t capacity) {
		int n = std::snprintf(out, capacity, WIFI_AP_SET_COMMAND "%d,%s,%s,%u", config.enabled ? 1 : 0,
							config.ssid, config.password, unsigned(config.channel));
		return n > 0 && size_t(n) < capacity;
	}
}
