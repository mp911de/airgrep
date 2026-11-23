#pragma once

#include <cstddef>
#include <cstdint>

#include "device.h"

namespace wifi {
#define WIFI_AP_SET_COMMAND "AT+WIFI_AP="
#define WIFI_AP_QUERY_COMMAND "AT+WIFI_AP?"
// The help text says +WIFI_AP=, the ADSBee prints the bare command name.
#define WIFI_AP_REPLY "WIFI_AP="
#define WIFI_AP_SSID_SIZE 33
#define WIFI_AP_PASSWORD_SIZE 64

// Reply to AT+WIFI_AP?: WIFI_AP=<enabled>,<ssid>,<password>,<channel>.
struct Config {
	bool enabled = false;
	char ssid[WIFI_AP_SSID_SIZE]{};
	char password[WIFI_AP_PASSWORD_SIZE]{};
	uint8_t channel = 0;
};

bool operator==(const Config& a, const Config& b);
inline bool operator!=(const Config& a, const Config& b) { return !(a == b); }

bool decode_config(const char* text, size_t length, Config& out);
// Writes AT+WIFI_AP=<enabled>,<ssid>,<password>,<channel>. False when it does
// not fit.
bool format_set(const Config& config, char* out, size_t capacity);

struct StateData {
	Status status = Status::unknown;
	bool slider_sampled = false;
	bool slider_enabled = false;
	bool reported_known = false;
	bool set_pending = false;
	Config last_reported_config;
	int64_t sent_us = 0, reported_us = 0;
	uint32_t sent = 0, mismatches = 0;
	EspErrorCode last_error = 0;
	int64_t error_us = 0;
};
}
