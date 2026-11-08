#include "adsbee/adsbee_protocol.hpp"
#include "unity.h"
#include <cstring>

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[adsbee]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(crc, "CSBEE CRC covers the trailing comma and accepts unpadded hex") {
	TEST_ASSERT_TRUE(adsbee::valid_csbee("#A:ABC,DEF4", 11));
	TEST_ASSERT_TRUE(adsbee::valid_csbee("#S:2,1,e252", 11));
	TEST_ASSERT_TRUE(adsbee::valid_csbee("#S:7,562", 8));
	TEST_ASSERT_FALSE(adsbee::valid_csbee("#A:ABD,DEF4", 11));
	TEST_ASSERT_FALSE(adsbee::valid_csbee("#A:ABC,F4DE", 11));
	TEST_ASSERT_FALSE(adsbee::valid_csbee("#A:ABC,", 7));
	TEST_ASSERT_FALSE(adsbee::valid_csbee("#A:ABC,DEF4x", 12));
	TEST_ASSERT_FALSE(adsbee::valid_csbee("OK", 2));
}

CASE(m10_crc, "AIRGREP M10 frames use the same checksum validator as CSBee") {
	const char* mon = "#M10:33:MON:00:01/01:0,0,1,2,1,0,50,4000,0,0,100,0,100,E6DB";
	const char* short_crc = "#M10:1:WARN:00:01/01:5,72B";
	const char* old_format = "#M10:33:MON:00:01/01:0,0,1,2,1,0,50,4000,0,0,100,0,100*BF08";
	TEST_ASSERT_TRUE(adsbee::valid_csbee(mon, std::strlen(mon)));
	TEST_ASSERT_TRUE(adsbee::valid_csbee(short_crc, std::strlen(short_crc)));
	TEST_ASSERT_FALSE(adsbee::valid_csbee(old_format, std::strlen(old_format)));
	TEST_ASSERT_FALSE(adsbee::valid_csbee("#M10:1:WARN:00:01/01:6,72B", std::strlen(short_crc)));
}

CASE(reply, "AT replies retain all lines and refuse overflow without partial append") {
	adsbee::Reply reply;
	TEST_ASSERT_TRUE(reply.append("Part Code: test", 15));
	TEST_ASSERT_TRUE(reply.append("ESP32 Disabled", 14));
	TEST_ASSERT_EQUAL_STRING("Part Code: test\nESP32 Disabled\n", reply.text);
	TEST_ASSERT_FALSE(reply.append("x", sizeof reply.text));
	TEST_ASSERT_EQUAL_STRING("Part Code: test\nESP32 Disabled\n", reply.text);
}

CASE(full_reply, "A full reply rejects further lines without overflowing") {
	adsbee::Reply reply;
	char full[2046];
	std::memset(full, 'x', sizeof full);
	TEST_ASSERT_TRUE(reply.append(full, sizeof full));
	TEST_ASSERT_EQUAL_UINT(2047, reply.length);
	TEST_ASSERT_FALSE(reply.append("x", 1));
	TEST_ASSERT_FALSE(reply.append("", 0));
	TEST_ASSERT_EQUAL_UINT(2047, reply.length);
}

CASE(command, "AT commands must be non-empty, fit the buffer and carry no line break") {
	TEST_ASSERT_TRUE(adsbee::acceptable_command("AT+DEVICE_INFO?", 16));
	TEST_ASSERT_FALSE(adsbee::acceptable_command("AT+DEVICE_INFO?", 15));
	TEST_ASSERT_FALSE(adsbee::acceptable_command("", 16));
	TEST_ASSERT_FALSE(adsbee::acceptable_command(nullptr, 16));
	TEST_ASSERT_FALSE(adsbee::acceptable_command("AT\n", 16));
	TEST_ASSERT_FALSE(adsbee::acceptable_command("AT\r", 16));
}

CASE(power, "ADSBee TX requires fresh successful power on and fails closed") {
	TEST_ASSERT_FALSE(adsbee::Power{}.permits_tx(1000000));
	TEST_ASSERT_TRUE((adsbee::Power{true, 1000000, 0}).permits_tx(1500000));
	TEST_ASSERT_FALSE((adsbee::Power{true, 1000000, 0}).permits_tx(1500001));
	TEST_ASSERT_FALSE((adsbee::Power{false, 1000000, 0}).permits_tx(1000000));
	TEST_ASSERT_FALSE((adsbee::Power{true, 1000000, 1000001}).permits_tx(1000001));
	TEST_ASSERT_FALSE((adsbee::Power{true, 1000000, 1000000}).permits_tx(1000000));
	TEST_ASSERT_TRUE((adsbee::Power{true, 1250000, 1000001}).permits_tx(1250000));
	TEST_ASSERT_FALSE((adsbee::Power{true, 1000000, 0}).permits_tx(999999));
}

CASE(lifecycle, "ADSBee waits for new RX after charger mode and every power cycle") {
	adsbee::Link link;
	link.update_power({false, 1000000, 0}, 1000000);
	link.received(1100000);
	TEST_ASSERT_FALSE(link.permits_tx(1100000));
	link.update_power({true, 2000000, 0}, 2000000);
	TEST_ASSERT_FALSE(link.permits_tx(2000000));
	link.received(1900000);
	TEST_ASSERT_FALSE(link.permits_tx(2000000));
	link.received(2100000);
	TEST_ASSERT_TRUE(link.permits_tx(2100000));
	link.update_power({false, 2200000, 0}, 2200000);
	TEST_ASSERT_FALSE(link.permits_tx(2200000));
	link.update_power({true, 2300000, 0}, 2300000);
	TEST_ASSERT_FALSE(link.permits_tx(2300000));
	link.received(2100000);
	TEST_ASSERT_FALSE(link.permits_tx(2300000));
	link.received(2400000);
	TEST_ASSERT_TRUE(link.permits_tx(2400000));
}

CASE(rx_timeout, "ADSBee RX liveness expires and recovers without active probes") {
	adsbee::Link link;
	link.update_power({true, 1000000, 0}, 1000000);
	link.received(1100000);
	for (int64_t now = 1250000; now <= 4000000; now += 250000)
		link.update_power({true, now, 0}, now);
	TEST_ASSERT_TRUE(link.permits_tx(4100000));
	TEST_ASSERT_FALSE(link.permits_tx(4100001));
	link.received(4200000);
	TEST_ASSERT_TRUE(link.permits_tx(4200000));
}

CASE(power_loss, "Power errors and missed samples require new RX before TX resumes") {
	adsbee::Link link;
	link.update_power({true, 1000000, 0}, 1000000);
	link.received(1100000);
	link.poll(1500001);
	TEST_ASSERT_FALSE(link.permits_tx(1500001));
	link.update_power({true, 1600000, 0}, 1600000);
	TEST_ASSERT_FALSE(link.permits_tx(1600000));
	link.received(1700000);
	TEST_ASSERT_TRUE(link.permits_tx(1700000));
	link.update_power({true, 1600000, 1800000}, 1800000);
	TEST_ASSERT_FALSE(link.permits_tx(1800000));
	link.update_power({true, 1900000, 1800000}, 1900000);
	TEST_ASSERT_FALSE(link.permits_tx(1900000));
	link.received(2000000);
	TEST_ASSERT_TRUE(link.permits_tx(2000000));
	link.update_power({true, 2600000, 1800000}, 2600000);
	TEST_ASSERT_FALSE(link.permits_tx(2600000));
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(command);
	RUN_TEST(power);
	RUN_TEST(crc);
	RUN_TEST(m10_crc);
	RUN_TEST(reply);
	RUN_TEST(full_reply);
	RUN_TEST(lifecycle);
	RUN_TEST(rx_timeout);
	RUN_TEST(power_loss);
	return UNITY_END();
}
#endif
