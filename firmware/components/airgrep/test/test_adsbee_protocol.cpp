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

CASE(gnss_full, "GNSS frame decodes every field in order") {
	const char* frame =
		"#GNSS:1,2,11,50.500000,-8.250000,1234,87,271.5,0.90,070509,1762588800123,2,1,1,47,24,5,4,3,2,10,12,11,13,1,ENABLED,EGNOS,2,-312,45,MAX-M10S,ROM SPG 5.10 (7b202e),00000A00,OK,ABCD";
	adsbee::Gnss g;
	TEST_ASSERT_TRUE(adsbee::decode_gnss(frame, std::strlen(frame), g));
	TEST_ASSERT_TRUE(g.fix);
	TEST_ASSERT_EQUAL_INT(2, g.quality);
	TEST_ASSERT_EQUAL_INT(11, g.satellites);
	TEST_ASSERT_FLOAT_WITHIN(0.000001f, 50.5f, g.latitude);
	TEST_ASSERT_FLOAT_WITHIN(0.000001f, -8.25f, g.longitude);
	TEST_ASSERT_EQUAL_INT32(1234, g.altitude_ft);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 87, g.speed_kt);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 271.5f, g.track_deg);
	TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9f, g.hdop);
	TEST_ASSERT_TRUE(g.utc_known);
	TEST_ASSERT_EQUAL_UINT(7, g.utc_hour);
	TEST_ASSERT_EQUAL_UINT(5, g.utc_minute);
	TEST_ASSERT_EQUAL_UINT(9, g.utc_second);
	TEST_ASSERT_EQUAL_UINT64(1762588800123ULL, g.epoch_ms);
	TEST_ASSERT_EQUAL_INT(2, g.antenna);
	TEST_ASSERT_EQUAL_INT(1, g.antenna_power);
	TEST_ASSERT_EQUAL_INT(1, g.jamming);
	TEST_ASSERT_EQUAL_INT(47, g.max_cno);
	TEST_ASSERT_EQUAL_INT(24, g.seen);
	TEST_ASSERT_EQUAL_INT(1, g.unhealthy);
	TEST_ASSERT_EQUAL_STRING("ENABLED", g.sbas_mode);
	TEST_ASSERT_EQUAL_STRING("EGNOS", g.sbas_system);
	TEST_ASSERT_EQUAL_INT(2, g.sbas_satellites);
	TEST_ASSERT_TRUE(g.sbas_corrections);
	TEST_ASSERT_EQUAL_INT32(-312, g.prc_cm);
	TEST_ASSERT_EQUAL_INT32(45, g.ic_cm);
	TEST_ASSERT_EQUAL_STRING("MAX-M10S", g.module);
	TEST_ASSERT_EQUAL_STRING("ROM SPG 5.10 (7b202e)", g.software);
	TEST_ASSERT_EQUAL_STRING("00000A00", g.hardware);
	TEST_ASSERT_TRUE(g.bbr);
}

CASE(gnss_empty,
	"GNSS frame with empty fields reports unknowns and rejects wrong shapes") {
	const char* frame = "#GNSS:0,0,0,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,2695";
	adsbee::Gnss g;
	TEST_ASSERT_TRUE(adsbee::decode_gnss(frame, std::strlen(frame), g));
	TEST_ASSERT_FALSE(g.fix);
	TEST_ASSERT_EQUAL_INT(0, g.quality);
	TEST_ASSERT_FLOAT_WITHIN(0.001f, -1, g.hdop);
	TEST_ASSERT_FALSE(g.utc_known);
	TEST_ASSERT_EQUAL_UINT64(0, g.epoch_ms);
	TEST_ASSERT_EQUAL_INT(-1, g.antenna);
	TEST_ASSERT_EQUAL_INT(-1, g.seen);
	TEST_ASSERT_EQUAL_STRING("", g.sbas_mode);
	TEST_ASSERT_FALSE(g.sbas_corrections);
	TEST_ASSERT_FALSE(g.bbr);
	TEST_ASSERT_FALSE(adsbee::decode_gnss("#GNSS:0,0,2695", 14, g));
	TEST_ASSERT_FALSE(adsbee::decode_gnss("#M10:1:WARN:00:01/01:5,72B", 26, g));
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
	RUN_TEST(gnss_full);
	RUN_TEST(gnss_empty);
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
