#include <cstring>
#include <initializer_list>

#include "adsbee/adsbee_time.hpp"
#include "unity.h"

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
	TEST_ASSERT_TRUE(
		adsbee::decode_time_ping(no_epoch, std::strlen(no_epoch), ping));
	TEST_ASSERT_FALSE(ping.epoch_known);
	TEST_ASSERT_EQUAL_UINT32(0, ping.pps);
	TEST_ASSERT_FALSE(adsbee::decode_time_ping("TIME_PING=7,1", 13, ping));
	TEST_ASSERT_FALSE(adsbee::decode_time_ping("OK", 2, ping));
}

CASE(estimate, "Estimate takes the shortest answered round trip with PPS") {
	adsbee::TimeSample samples[4] = {
		{true, 1000, 9000, 1000000, 5},
		{true, 2000, 5000, 1000001, 5},
		{true, 3000, 4000, 1000002, 0},
		{false, 4000, 0, 0, 0},
	};
	const adsbee::TimeEstimate e = adsbee::estimate_time(samples, 4);
	TEST_ASSERT_TRUE(e.valid);
	TEST_ASSERT_EQUAL_INT64(3000, e.rtt_us);
	TEST_ASSERT_EQUAL_INT64(5000, e.reply_received_us);
	TEST_ASSERT_EQUAL_INT64(1000001000LL + 1500, e.utc_at_reply_us);
	TEST_ASSERT_EQUAL_UINT32(5, e.pps);
	adsbee::TimeSample none[1] = {{true, 1000, 2000, 1000000, 0}};
	TEST_ASSERT_FALSE(adsbee::estimate_time(none, 1).valid);
	TEST_ASSERT_FALSE(adsbee::estimate_time(samples, 0).valid);
}

CASE(invalid_ping, "Time ping rejects partial, signed and overflowing fields") {
	adsbee::TimePing decoded;
	for (const char* line :
		 {"TIME_PING=1x,2,3", "TIME_PING=1,bad,3", "TIME_PING=1,-1,3",
		  "TIME_PING=1,9223372036854776,3", "TIME_PING=1,2,4294967296",
		  "TIME_PING=1,2,3,4"}) {
		TEST_ASSERT_FALSE(
			adsbee::decode_time_ping(line, std::strlen(line), decoded));
	}
}

CASE(epoch_limit,
	 "Round trip correction must fit before selecting a time sample") {
	const uint64_t last_ms = INT64_MAX / 1000;
	adsbee::TimeSample sample{true, 1000, 2616, last_ms, 1};
	TEST_ASSERT_FALSE(adsbee::estimate_time(&sample, 1).valid);
	sample.t4_us -= 2;
	auto estimate = adsbee::estimate_time(&sample, 1);
	TEST_ASSERT_TRUE(estimate.valid);
	TEST_ASSERT_EQUAL_INT64(INT64_MAX, estimate.utc_at_reply_us);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(ping);
	RUN_TEST(estimate);
	RUN_TEST(invalid_ping);
	RUN_TEST(epoch_limit);
	return UNITY_END();
}
#endif
