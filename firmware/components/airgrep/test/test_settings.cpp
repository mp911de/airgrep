#include "settings/settings_model.hpp"
#include "unity.h"
#include <cstring>

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[settings]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(defaults, "Defaults carry the current version and UTC") {
	settings::Settings s = settings::defaults();
	TEST_ASSERT_EQUAL_UINT16(SETTINGS_VERSION, s.version);
	TEST_ASSERT_EQUAL_STRING("UTC0", s.tz_rule);
	TEST_ASSERT_EQUAL_UINT8(1, s.tz_detect);
	TEST_ASSERT_EQUAL_UINT16(5000, s.battery_mah);
}

CASE(number, "Number fields take decimal 0 to 65535 and nothing else") {
	settings::Settings s = settings::defaults();
	TEST_ASSERT_TRUE(settings::assign(s, "battery_mah", " 10000 "));
	TEST_ASSERT_EQUAL_UINT16(10000, s.battery_mah);
	TEST_ASSERT_TRUE(settings::assign(s, "battery_mah", "65535"));
	TEST_ASSERT_FALSE(settings::assign(s, "battery_mah", "65536"));
	TEST_ASSERT_FALSE(settings::assign(s, "battery_mah", "-1"));
	TEST_ASSERT_FALSE(settings::assign(s, "battery_mah", "5k"));
	TEST_ASSERT_FALSE(settings::assign(s, "battery_mah", ""));
	TEST_ASSERT_EQUAL_UINT16(65535, s.battery_mah);
	char text[8];
	TEST_ASSERT_EQUAL_UINT(5, settings::format(s, *settings::find("battery_mah"), text, sizeof text));
	TEST_ASSERT_EQUAL_STRING("65535", text);
	TEST_ASSERT_EQUAL_UINT(2, settings::format(s, *settings::find("battery_mah"), text, 3));
	TEST_ASSERT_EQUAL_STRING("65", text);
}

CASE(migrate_v1, "A version 1 blob keeps its zone fields and gains the default capacity") {
	settings::SettingsV1 v1{1, "CET-1CEST,M3.5.0,M10.5.0/3", 0, 0};
	settings::Settings out{};
	TEST_ASSERT_TRUE(settings::migrate(&v1, sizeof v1, out));
	TEST_ASSERT_EQUAL_UINT16(SETTINGS_VERSION, out.version);
	TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", out.tz_rule);
	TEST_ASSERT_EQUAL_UINT8(0, out.tz_detect);
	TEST_ASSERT_EQUAL_UINT16(5000, out.battery_mah);
	TEST_ASSERT_FALSE(settings::migrate(&v1, sizeof v1 + 1, out));
}

CASE(boolean, "Boolean fields accept on, off, true, false, 1, 0 and nothing else") {
	settings::Settings s = settings::defaults();
	TEST_ASSERT_TRUE(settings::assign(s, "tz_detect", " off "));
	TEST_ASSERT_EQUAL_UINT8(0, s.tz_detect);
	TEST_ASSERT_TRUE(settings::assign(s, "tz_detect", "true"));
	TEST_ASSERT_EQUAL_UINT8(1, s.tz_detect);
	TEST_ASSERT_TRUE(settings::assign(s, "tz_detect", "0"));
	TEST_ASSERT_FALSE(settings::assign(s, "tz_detect", "yes"));
	TEST_ASSERT_FALSE(settings::assign(s, "tz_detect", ""));
	char text[4];
	TEST_ASSERT_EQUAL_UINT(3, settings::format(s, *settings::find("tz_detect"), text, sizeof text));
	TEST_ASSERT_EQUAL_STRING("OFF", text);
}

CASE(migrate, "Migrate accepts the current layout and rejects unknown versions and sizes") {
	settings::Settings stored = settings::defaults();
	std::strcpy(stored.tz_rule, "CET-1CEST,M3.5.0,M10.5.0/3");
	settings::Settings out{};
	TEST_ASSERT_TRUE(settings::migrate(&stored, sizeof stored, out));
	TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", out.tz_rule);
	TEST_ASSERT_FALSE(settings::migrate(&stored, sizeof stored - 1, out));
	TEST_ASSERT_FALSE(settings::migrate(&stored, sizeof stored + 1, out));
	stored.version = 0;
	TEST_ASSERT_FALSE(settings::migrate(&stored, sizeof stored, out));
	stored.version = SETTINGS_VERSION + 1;
	TEST_ASSERT_FALSE(settings::migrate(&stored, sizeof stored, out));
	TEST_ASSERT_FALSE(settings::migrate(nullptr, sizeof stored, out));
	TEST_ASSERT_FALSE(settings::migrate(&stored, 1, out));
}

CASE(assign, "Assign trims text, enforces the field size and rejects unknown fields") {
	settings::Settings s = settings::defaults();
	TEST_ASSERT_TRUE(settings::assign(s, "tz_rule", "  CET-1CEST,M3.5.0,M10.5.0/3 \n"));
	TEST_ASSERT_EQUAL_STRING("CET-1CEST,M3.5.0,M10.5.0/3", s.tz_rule);
	TEST_ASSERT_TRUE(settings::assign(s, "tz_rule", ""));
	TEST_ASSERT_EQUAL_STRING("", s.tz_rule);
	char long_value[SETTINGS_TZ_RULE_SIZE + 1];
	std::memset(long_value, 'x', sizeof long_value - 1);
	long_value[sizeof long_value - 1] = 0;
	TEST_ASSERT_FALSE(settings::assign(s, "tz_rule", long_value));
	long_value[SETTINGS_TZ_RULE_SIZE - 1] = 0;
	TEST_ASSERT_TRUE(settings::assign(s, "tz_rule", long_value));
	TEST_ASSERT_EQUAL_UINT(SETTINGS_TZ_RULE_SIZE - 1, std::strlen(s.tz_rule));
	TEST_ASSERT_FALSE(settings::assign(s, "nope", "1"));
	TEST_ASSERT_FALSE(settings::assign(s, "tz_rule", nullptr));
}

CASE(table, "The field table lists every field once and formats it") {
	size_t count;
	const settings::Field* fields = settings::fields(count);
	TEST_ASSERT_EQUAL_UINT(3, count);
	TEST_ASSERT_EQUAL_STRING("tz_rule", fields[0].name);
	TEST_ASSERT_EQUAL_STRING("battery_mah", fields[2].name);
	TEST_ASSERT_EQUAL_PTR(fields, settings::find("tz_rule"));
	TEST_ASSERT_NULL(settings::find("TZ_RULE"));
	settings::Settings s = settings::defaults();
	char text[8];
	TEST_ASSERT_EQUAL_UINT(4, settings::format(s, fields[0], text, sizeof text));
	TEST_ASSERT_EQUAL_STRING("UTC0", text);
	TEST_ASSERT_EQUAL_UINT(2, settings::format(s, fields[0], text, 3));
	TEST_ASSERT_EQUAL_STRING("UT", text);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(defaults);
	RUN_TEST(boolean);
	RUN_TEST(number);
	RUN_TEST(migrate);
	RUN_TEST(migrate_v1);
	RUN_TEST(assign);
	RUN_TEST(table);
	return UNITY_END();
}
#endif
