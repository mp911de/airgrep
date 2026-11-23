#include <cstring>

#include "unity.h"
#include "wifi/wifi_protocol.hpp"
#include "wifi/wifi_requests.hpp"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[wifi]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(decode, "WIFI_AP reply decodes enabled, ssid, password and channel") {
	wifi::Config config;
	const char* on = "WIFI_AP=1,airgrep-7cdfa1b2c3d4,airgrep-wifi,11";
	TEST_ASSERT_TRUE(wifi::decode_config(on, std::strlen(on), config));
	TEST_ASSERT_TRUE(config.enabled);
	TEST_ASSERT_EQUAL_STRING("airgrep-7cdfa1b2c3d4", config.ssid);
	TEST_ASSERT_EQUAL_STRING("airgrep-wifi", config.password);
	TEST_ASSERT_EQUAL_UINT8(11, config.channel);
	const char* off = "WIFI_AP=0,ADSBee,,1";
	TEST_ASSERT_TRUE(wifi::decode_config(off, std::strlen(off), config));
	TEST_ASSERT_FALSE(config.enabled);
	TEST_ASSERT_EQUAL_STRING("", config.password);
	TEST_ASSERT_FALSE(wifi::decode_config("WIFI_AP=1,x,y", 13, config));
	TEST_ASSERT_FALSE(wifi::decode_config("OK", 2, config));
}

CASE(compare_and_format, "Config equality and the set command round trip") {
	wifi::Config wanted;
	wanted.enabled = true;
	std::strcpy(wanted.ssid, "airgrep-7cdfa1b2c3d4");
	std::strcpy(wanted.password, "airgrep-wifi");
	wanted.channel = 11;
	char command[128];
	TEST_ASSERT_TRUE(wifi::format_set(wanted, command, sizeof command));
	TEST_ASSERT_EQUAL_STRING(
		"AT+WIFI_AP=1,airgrep-7cdfa1b2c3d4,airgrep-wifi,11", command);
	wifi::Config reported;
	const char* reply = "WIFI_AP=1,airgrep-7cdfa1b2c3d4,airgrep-wifi,11";
	TEST_ASSERT_TRUE(wifi::decode_config(reply, std::strlen(reply), reported));
	TEST_ASSERT_TRUE(reported == wanted);
	reported.channel = 6;
	TEST_ASSERT_TRUE(reported != wanted);
	TEST_ASSERT_FALSE(wifi::format_set(wanted, command, 20));
}

CASE(rejected_write, "A rejected write retries the pending set") {
	wifi::Requests requests;
	requests.apply();
	TEST_ASSERT_TRUE(requests.submit() == wifi::Command::set);
	requests.finished();
	TEST_ASSERT_TRUE(requests.set_pending());
	TEST_ASSERT_TRUE(requests.submit() == wifi::Command::set);
}

CASE(edit_during_write, "An edit during a queued write survives completion") {
	wifi::Requests requests;
	requests.apply();
	TEST_ASSERT_TRUE(requests.submit() == wifi::Command::set);
	requests.apply();
	requests.written(false);
	TEST_ASSERT_TRUE(requests.set_pending());
	TEST_ASSERT_TRUE(requests.submit() == wifi::Command::none);
	requests.finished();
	TEST_ASSERT_TRUE(requests.submit() == wifi::Command::set);
}

CASE(verify_after_write, "A completed write schedules a verification query") {
	wifi::Requests requests;
	requests.apply();
	TEST_ASSERT_TRUE(requests.submit() == wifi::Command::set);
	requests.written(true);
	TEST_ASSERT_FALSE(requests.set_pending());
	TEST_ASSERT_TRUE(requests.submit() == wifi::Command::none);
	requests.finished();
	TEST_ASSERT_TRUE(requests.submit() == wifi::Command::query);
}

CASE(edit_during_query, "An edit during a query schedules a set") {
	wifi::Requests requests;
	requests.poll();
	TEST_ASSERT_TRUE(requests.submit() == wifi::Command::query);
	requests.apply();
	requests.written(true);
	requests.finished();
	TEST_ASSERT_TRUE(requests.submit() == wifi::Command::set);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(decode);
	RUN_TEST(compare_and_format);
	RUN_TEST(rejected_write);
	RUN_TEST(edit_during_write);
	RUN_TEST(verify_after_write);
	RUN_TEST(edit_during_query);
	return UNITY_END();
}
#endif
