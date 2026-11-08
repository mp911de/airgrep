#include "adsbee/adsbee_time.hpp"
#include "unity.h"
#include <cstring>

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[adsbee]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(ping, "Time ping reply decodes echo, epoch and PPS count") {
	adsbee::TimePing ping;
	const char* full = "TIME_PING=123456789012,1762588800123,1417";
	TEST_ASSERT_TRUE(adsbee::decode_time_ping(full, std::strlen(full), ping));
	TEST_ASSERT_EQUAL_UINT64(123456789012ULL, ping.echo);
	TEST_ASSERT_TRUE(ping.epoch_known);
	TEST_ASSERT_EQUAL_UINT64(1762588800123ULL, ping.epoch_ms);
	TEST_ASSERT_EQUAL_UINT32(1417, ping.pps);
	const char* no_epoch = "TIME_PING=7,,0";
	TEST_ASSERT_TRUE(adsbee::decode_time_ping(no_epoch, std::strlen(no_epoch), ping));
	TEST_ASSERT_FALSE(ping.epoch_known);
	TEST_ASSERT_EQUAL_UINT32(0, ping.pps);
	TEST_ASSERT_FALSE(adsbee::decode_time_ping("TIME_PING=7,1", 13, ping));
	TEST_ASSERT_FALSE(adsbee::decode_time_ping("OK", 2, ping));
}

CASE(estimate, "Estimate takes the shortest answered round trip with PPS") {
	adsbee::TimeSample samples[4] = {
		{true, 1000, 9000, 1000000, 5}, // 8 ms
		{true, 2000, 5000, 1000001, 5}, // 3 ms, best
		{true, 3000, 4000, 1000002, 0}, // 1 ms but no PPS
		{false, 4000, 0, 0, 0},
	};
	adsbee::TimeEstimate e = adsbee::estimate_time(samples, 4);
	TEST_ASSERT_TRUE(e.valid);
	TEST_ASSERT_EQUAL_INT64(3000, e.rtt_us);
	TEST_ASSERT_EQUAL_INT64(5000, e.t4_us);
	TEST_ASSERT_EQUAL_INT64(1000001000LL + 1500, e.utc_at_t4_us);
	TEST_ASSERT_EQUAL_UINT32(5, e.pps);
	adsbee::TimeSample none[1] = {{true, 1000, 2000, 1000000, 0}};
	TEST_ASSERT_FALSE(adsbee::estimate_time(none, 1).valid);
	TEST_ASSERT_FALSE(adsbee::estimate_time(samples, 0).valid);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(ping);
	RUN_TEST(estimate);
	return UNITY_END();
}
#endif
