#include <cstring>

#include "settings/settings_model.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[settings]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

namespace {
const char* shown(const settings::Settings& s, const char* name) {
	static char text[SETTINGS_ZONE_SIZE];
	settings::format(s, *settings::find(name), text, sizeof text);
	return text;
}
}

CASE(defaults, "Defaults carry the current version and choose nothing") {
	settings::Settings s = settings::defaults();
	TEST_ASSERT_EQUAL_UINT16(SETTINGS_VERSION, s.version);
	TEST_ASSERT_EQUAL_STRING("", s.zone);
	TEST_ASSERT_EQUAL_UINT8(1, s.tz_detect);
	TEST_ASSERT_EQUAL_UINT16(5000, s.battery_mah);
	TEST_ASSERT_EQUAL_HEX32(SETTINGS_NO_ADDRESS, s.own_address);
	TEST_ASSERT_EQUAL_STRING("", s.own_callsign);
	TEST_ASSERT_EQUAL_UINT16(3000, s.altitude_filter_ft);
	TEST_ASSERT_TRUE(settings::valid(s));
}

CASE(limit, "A number field with a limit takes no more than that") {
	settings::Settings s = settings::defaults();
	TEST_ASSERT_TRUE(settings::assign(s, "altitude_filter_ft", "9999"));
	TEST_ASSERT_FALSE(settings::assign(s, "altitude_filter_ft", "10000"));
	TEST_ASSERT_EQUAL_UINT16(9999, s.altitude_filter_ft);
	TEST_ASSERT_FALSE(settings::assign(s, "look_ahead_min", "2"));
	TEST_ASSERT_TRUE(settings::assign(s, "altitude_filter_ft", "0"));
	TEST_ASSERT_EQUAL_STRING("0", shown(s, "altitude_filter_ft"));
}

CASE(number, "A number field takes what its size holds and nothing else") {
	settings::Settings s = settings::defaults();
	TEST_ASSERT_TRUE(settings::assign(s, "battery_mah", " 10000 "));
	TEST_ASSERT_EQUAL_UINT16(10000, s.battery_mah);
	TEST_ASSERT_TRUE(settings::assign(s, "battery_mah", "65535"));
	TEST_ASSERT_FALSE(settings::assign(s, "battery_mah", "65536"));
	TEST_ASSERT_FALSE(settings::assign(s, "battery_mah", "-1"));
	TEST_ASSERT_FALSE(settings::assign(s, "battery_mah", "5k"));
	TEST_ASSERT_FALSE(settings::assign(s, "battery_mah", ""));
	TEST_ASSERT_EQUAL_UINT16(65535, s.battery_mah);
	TEST_ASSERT_EQUAL_STRING("65535", shown(s, "battery_mah"));
	char text[3];
	TEST_ASSERT_EQUAL_UINT(
		2, settings::format(s, *settings::find("battery_mah"), text, 3));
	TEST_ASSERT_EQUAL_STRING("65", text);
}

CASE(address, "An address field takes up to six hex digits or NONE") {
	settings::Settings s = settings::defaults();
	TEST_ASSERT_EQUAL_STRING("NONE", shown(s, "own_address"));
	TEST_ASSERT_TRUE(settings::assign(s, "own_address", " 3d2a7f "));
	TEST_ASSERT_EQUAL_HEX32(0x3D2A7F, s.own_address);
	TEST_ASSERT_EQUAL_STRING("3D2A7F", shown(s, "own_address"));
	TEST_ASSERT_TRUE(settings::assign(s, "own_address", "1"));
	TEST_ASSERT_EQUAL_STRING("000001", shown(s, "own_address"));
	TEST_ASSERT_FALSE(settings::assign(s, "own_address", "1000000"));
	TEST_ASSERT_FALSE(settings::assign(s, "own_address", "3D2A7G"));
	TEST_ASSERT_FALSE(settings::assign(s, "own_address", ""));
	TEST_ASSERT_EQUAL_HEX32(1, s.own_address);
	TEST_ASSERT_TRUE(settings::assign(s, "own_address", "NONE"));
	TEST_ASSERT_EQUAL_HEX32(SETTINGS_NO_ADDRESS, s.own_address);
}

CASE(boolean,
	 "Boolean fields accept on, off, true, false, 1, 0 and nothing else") {
	settings::Settings s = settings::defaults();
	TEST_ASSERT_TRUE(settings::assign(s, "tz_detect", " off "));
	TEST_ASSERT_EQUAL_UINT8(0, s.tz_detect);
	TEST_ASSERT_TRUE(settings::assign(s, "tz_detect", "true"));
	TEST_ASSERT_EQUAL_UINT8(1, s.tz_detect);
	TEST_ASSERT_TRUE(settings::assign(s, "tz_detect", "0"));
	TEST_ASSERT_FALSE(settings::assign(s, "tz_detect", "yes"));
	TEST_ASSERT_FALSE(settings::assign(s, "tz_detect", ""));
	TEST_ASSERT_EQUAL_STRING("OFF", shown(s, "tz_detect"));
	TEST_ASSERT_FALSE(settings::assign(s, "dark_theme", "on"));
	TEST_ASSERT_FALSE(settings::assign(s, "track_display", "off"));
	TEST_ASSERT_FALSE(settings::assign(s, "radar_range_nm", "8"));
}

CASE(migrate,
	 "Migrate reads the V1 bytes and rejects unknown versions and sizes") {
	uint8_t stored[88] = {1, 0};
	std::memcpy(stored + 2, "Europe/Berlin", sizeof "Europe/Berlin");
	// V1 bytes 66 through 87, including a callsign missing its terminator.
	// Bytes 67 through 69 held the theme, the track display and the range.
	const uint8_t tail[] = {0,    1,   1,   16,  0x70, 0x17, 0x56, 0x34,
							0x12, 0,   'D', 'E', 'A',  'B',  'C',  '1',
							'2',  '3', 'X', 5,   0xd0, 0x07};
	std::memcpy(stored + 66, tail, sizeof tail);
	settings::Settings out{};
	TEST_ASSERT_TRUE(settings::migrate(stored, sizeof stored, out));
	TEST_ASSERT_EQUAL_UINT16(SETTINGS_VERSION, out.version);
	TEST_ASSERT_EQUAL_STRING("Europe/Berlin", out.zone);
	TEST_ASSERT_EQUAL_UINT8(0, out.tz_detect);
	TEST_ASSERT_EQUAL_UINT16(6000, out.battery_mah);
	TEST_ASSERT_EQUAL_HEX32(0x123456, out.own_address);
	TEST_ASSERT_EQUAL_STRING("DEABC123", out.own_callsign);
	TEST_ASSERT_EQUAL_UINT8(5, out.reserved);
	TEST_ASSERT_EQUAL_UINT16(2000, out.altitude_filter_ft);
	TEST_ASSERT_FALSE(settings::migrate(stored, sizeof stored - 1, out));
	uint8_t oversized[89] = {1};
	TEST_ASSERT_FALSE(settings::migrate(oversized, sizeof oversized, out));
	stored[0] = 0;
	TEST_ASSERT_FALSE(settings::migrate(stored, sizeof stored, out));
	stored[0] = SETTINGS_VERSION + 1;
	TEST_ASSERT_FALSE(settings::migrate(stored, sizeof stored, out));
	TEST_ASSERT_FALSE(settings::migrate(nullptr, sizeof stored, out));
	TEST_ASSERT_FALSE(settings::migrate(stored, 1, out));
}

CASE(before_the_release,
	 "A blob of a layout from before the first release is not taken") {
	// 68 and 70 bytes, as development firmware stored them under the
	// versions 1 and 2.
	uint8_t old[70]{};
	old[0] = 1;
	settings::Settings out{};
	TEST_ASSERT_FALSE(settings::migrate(old, 68, out));
	old[0] = 2;
	TEST_ASSERT_FALSE(settings::migrate(old, 70, out));
}

CASE(assign,
	 "Assign trims text, enforces the field size and rejects unknown fields") {
	settings::Settings s = settings::defaults();
	TEST_ASSERT_TRUE(settings::assign(s, "zone", "  Europe/Berlin \n"));
	TEST_ASSERT_EQUAL_STRING("Europe/Berlin", s.zone);
	TEST_ASSERT_TRUE(settings::assign(s, "zone", ""));
	TEST_ASSERT_EQUAL_STRING("", s.zone);
	char long_value[SETTINGS_ZONE_SIZE + 1];
	std::memset(long_value, 'x', sizeof long_value - 1);
	long_value[sizeof long_value - 1] = 0;
	TEST_ASSERT_FALSE(settings::assign(s, "zone", long_value));
	long_value[SETTINGS_ZONE_SIZE - 1] = 0;
	TEST_ASSERT_TRUE(settings::assign(s, "zone", long_value));
	TEST_ASSERT_EQUAL_UINT(SETTINGS_ZONE_SIZE - 1, std::strlen(s.zone));
	TEST_ASSERT_TRUE(settings::assign(s, "own_callsign", "DEABC"));
	TEST_ASSERT_FALSE(settings::assign(s, "own_callsign", "TOOLONG99"));
	TEST_ASSERT_EQUAL_STRING("DEABC", s.own_callsign);
	TEST_ASSERT_FALSE(settings::assign(s, "nope", "1"));
	TEST_ASSERT_FALSE(settings::assign(s, "zone", nullptr));
}

CASE(table, "The field table lists every field once and formats it") {
	size_t count = 0;
	const settings::Field* fields = settings::fields(count);
	TEST_ASSERT_EQUAL_UINT(6, count);
	const auto* zone = settings::find("zone");
	TEST_ASSERT_NOT_NULL(zone);
	TEST_ASSERT_NOT_NULL(settings::find("altitude_filter_ft"));
	TEST_ASSERT_NULL(settings::find("ZONE"));
	for (size_t i = 0; i < count; i++) {
		TEST_ASSERT_TRUE(fields[i].offset + fields[i].size <=
						 sizeof(settings::Settings));
		for (size_t j = 0; j < i; j++) {
			TEST_ASSERT_TRUE(std::strcmp(fields[i].name, fields[j].name) != 0);
		}
	}
	settings::Settings s = settings::defaults();
	std::strcpy(s.zone, "Etc/UTC");
	char text[8];
	TEST_ASSERT_EQUAL_UINT(7, settings::format(s, *zone, text, sizeof text));
	TEST_ASSERT_EQUAL_STRING("Etc/UTC", text);
	TEST_ASSERT_EQUAL_UINT(2, settings::format(s, *zone, text, 3));
	TEST_ASSERT_EQUAL_STRING("Et", text);
}

CASE(valid, "Valid settings hold what each field takes") {
	auto with = [](auto change) {
		settings::Settings s = settings::defaults();
		change(s);
		return settings::valid(s);
	};
	TEST_ASSERT_TRUE(with([](settings::Settings& s) {
		s.own_address = 0xFFFFFF;
		s.altitude_filter_ft = SETTINGS_MAX_ALTITUDE_FILTER_FT;
		s.tz_detect = 0;
	}));
	TEST_ASSERT_FALSE(with([](settings::Settings& s) { s.version = 2; }));
	TEST_ASSERT_FALSE(with([](settings::Settings& s) { s.tz_detect = 2; }));
	TEST_ASSERT_FALSE(
		with([](settings::Settings& s) { s.own_address = 0x1000000; }));
	TEST_ASSERT_FALSE(
		with([](settings::Settings& s) { s.altitude_filter_ft = 10000; }));
	TEST_ASSERT_FALSE(with([](settings::Settings& s) {
		std::memset(s.zone, 'x', sizeof s.zone);
	}));
	TEST_ASSERT_FALSE(with([](settings::Settings& s) {
		std::memset(s.own_callsign, 'X', sizeof s.own_callsign);
	}));
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(defaults);
	RUN_TEST(valid);
	RUN_TEST(boolean);
	RUN_TEST(number);
	RUN_TEST(limit);
	RUN_TEST(address);
	RUN_TEST(migrate);
	RUN_TEST(before_the_release);
	RUN_TEST(assign);
	RUN_TEST(table);
	return UNITY_END();
}
#endif
