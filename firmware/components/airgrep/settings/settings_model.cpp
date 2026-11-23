#include "settings_model.hpp"

#include <cstdio>
#include <cstring>

namespace settings {
namespace {
constexpr uint32_t kAddressMax = 0xFFFFFF;
}

Settings defaults() {
	Settings result{};
	result.version = SETTINGS_VERSION;
	result.tz_detect = SETTINGS_DEFAULT_TZ_DETECT;
	result.battery_mah = SETTINGS_DEFAULT_BATTERY_MAH;
	result.own_address = SETTINGS_NO_ADDRESS;
	result.altitude_filter_ft = SETTINGS_DEFAULT_ALTITUDE_FILTER_FT;
	return result;
}

bool valid(const Settings& settings) {
	return settings.version == SETTINGS_VERSION &&
		   std::memchr(settings.zone, 0, sizeof settings.zone) &&
		   settings.tz_detect <= 1 &&
		   (settings.own_address == SETTINGS_NO_ADDRESS ||
			settings.own_address <= kAddressMax) &&
		   std::memchr(settings.own_callsign, 0,
					   sizeof settings.own_callsign) &&
		   settings.altitude_filter_ft <= SETTINGS_MAX_ALTITUDE_FILTER_FT;
}

bool migrate(const void* blob, size_t size, Settings& out) {
	if (!blob || size < sizeof(uint16_t)) {
		return false;
	}
	uint16_t version = 0;
	std::memcpy(&version, blob, sizeof version);
	Settings result = defaults();
	if (version != 1 || size != sizeof(Settings)) {
		return false;
	}
	std::memcpy(&result, blob, sizeof result);
	result.zone[sizeof result.zone - 1] = 0;
	result.own_callsign[sizeof result.own_callsign - 1] = 0;
	out = result;
	return true;
}

namespace {
const Field table[] = {
	{"zone", Type::text, offsetof(Settings, zone), SETTINGS_ZONE_SIZE},
	{"tz_detect", Type::boolean, offsetof(Settings, tz_detect), 1},
	{"battery_mah", Type::number, offsetof(Settings, battery_mah),
	 sizeof(Settings::battery_mah), UINT16_MAX},
	{"own_address", Type::address, offsetof(Settings, own_address), 4},
	{"own_callsign", Type::text, offsetof(Settings, own_callsign),
	 SETTINGS_CALLSIGN_SIZE},
	{"altitude_filter_ft", Type::number, offsetof(Settings, altitude_filter_ft),
	 2, SETTINGS_MAX_ALTITUDE_FILTER_FT},
};

bool equals(const char* text, size_t length, const char* word) {
	return std::strlen(word) == length && std::strncmp(text, word, length) == 0;
}

bool blank(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

constexpr char kNone[] = "NONE";

bool parse(const char* text, size_t length, unsigned base, uint32_t limit,
		   uint32_t& out) {
	uint64_t parsed = 0;
	for (size_t i = 0; i < length; i++) {
		const char c = text[i];
		const unsigned digit =
			c >= '0' && c <= '9'   ? static_cast<unsigned>(c - '0')
			: c >= 'a' && c <= 'f' ? static_cast<unsigned>(c - 'a' + 10)
			: c >= 'A' && c <= 'F' ? static_cast<unsigned>(c - 'A' + 10)
								   : base;
		if (digit >= base) {
			return false;
		}
		parsed = parsed * base + digit;
		if (parsed > limit) {
			return false;
		}
	}
	out = static_cast<uint32_t>(parsed);
	return length > 0;
}

size_t put(const char* text, char* out, size_t capacity) {
	size_t length = std::strlen(text);
	if (length >= capacity) {
		length = capacity - 1;
	}
	std::memcpy(out, text, length);
	out[length] = 0;
	return length;
}
}

const Field* fields(size_t& count) {
	count = sizeof table / sizeof table[0];
	return table;
}

const Field* find(const char* name) {
	for (const Field& field : table) {
		if (std::strcmp(field.name, name) == 0) {
			return &field;
		}
	}
	return nullptr;
}

bool assign(Settings& settings, const char* name, const char* value) {
	const Field* field = find(name);
	if (!field || !value) {
		return false;
	}
	while (blank(*value)) {
		value++;
	}
	size_t length = std::strlen(value);
	while (length && blank(value[length - 1])) {
		length--;
	}
	switch (field->type) {
		case Type::text:
			if (length >= field->size) {
				return false;
			}
			std::memcpy(reinterpret_cast<char*>(&settings) + field->offset,
						value, length);
			reinterpret_cast<char*>(&settings)[field->offset + length] = 0;
			return true;
		case Type::boolean: {
			const bool on = equals(value, length, "on") ||
							equals(value, length, "true") ||
							equals(value, length, "1");
			const bool off = equals(value, length, "off") ||
							 equals(value, length, "false") ||
							 equals(value, length, "0");
			if (!on && !off) {
				return false;
			}
			reinterpret_cast<uint8_t*>(&settings)[field->offset] = on ? 1 : 0;
			return true;
		}
		case Type::number: {
			uint32_t number = 0;
			if (!parse(value, length, 10, field->largest, number)) {
				return false;
			}
			const uint16_t narrowed = static_cast<uint16_t>(number);
			std::memcpy(reinterpret_cast<char*>(&settings) + field->offset,
						&narrowed, sizeof narrowed);
			return true;
		}
		case Type::address: {
			uint32_t address = SETTINGS_NO_ADDRESS;
			if (!equals(value, length, kNone) &&
				!parse(value, length, 16, kAddressMax, address)) {
				return false;
			}
			std::memcpy(reinterpret_cast<char*>(&settings) + field->offset,
						&address, sizeof address);
			return true;
		}
	}
	return false;
}

size_t format(const Settings& settings, const Field& field, char* out,
			  size_t capacity) {
	if (!capacity) {
		return 0;
	}
	switch (field.type) {
		case Type::text:
			return put(reinterpret_cast<const char*>(&settings) + field.offset,
					   out, capacity);
		case Type::boolean:
			return put(reinterpret_cast<const uint8_t*>(&settings)[field.offset]
						   ? "ON"
						   : "OFF",
					   out, capacity);
		case Type::number: {
			uint16_t number = 0;
			std::memcpy(&number,
						reinterpret_cast<const char*>(&settings) + field.offset,
						sizeof number);
			char text[12];
			(void)std::snprintf(text, sizeof text, "%u",
								static_cast<unsigned>(number));
			return put(text, out, capacity);
		}
		case Type::address: {
			uint32_t number = 0;
			std::memcpy(&number,
						reinterpret_cast<const char*>(&settings) + field.offset,
						sizeof number);
			char text[12];
			if (number == SETTINGS_NO_ADDRESS) {
				(void)std::snprintf(text, sizeof text, "%s", kNone);
			} else {
				(void)std::snprintf(text, sizeof text, "%06lX",
									static_cast<unsigned long>(number));
			}
			return put(text, out, capacity);
		}
	}
	out[0] = 0;
	return 0;
}
}
