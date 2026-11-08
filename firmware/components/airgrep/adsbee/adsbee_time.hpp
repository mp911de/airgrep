#pragma once

#include <cstddef>
#include <cstdint>

// NTP-style time transfer over the ADSBee AT link. Hardware free, so it has a host test.
namespace adsbee {
#define ADSBEE_TIME_PING_COMMAND "AT+TIME_PING="
#define ADSBEE_TIME_PING_REPLY "TIME_PING="
#define ADSBEE_TIME_PING_COUNT 5
#define ADSBEE_TIME_PING_SPACING_MS 200
#define ADSBEE_TIME_PING_WINDOW_MS 150
#define ADSBEE_TIME_SYNC_PERIOD_MS 300000
#define ADSBEE_TIME_SYNC_DELAY_MS 3000
#define ADSBEE_TIME_SYNC_JITTER_MS 5000
	// Corrections above this step the clock, smaller ones slew so time never runs backwards.
#define ADSBEE_TIME_STEP_THRESHOLD_US 1000000

	// Reply to AT+TIME_PING=<echo>: TIME_PING=<echo>,<epoch_ms>,<pps_count>.
	struct TimePing {
		uint64_t echo = 0;
		bool epoch_known = false;
		uint64_t epoch_ms = 0;
		uint32_t pps = 0;
	};

	bool decode_time_ping(const char* text, size_t length, TimePing& out);

	// One exchange: t1 our send time, t4 our receive time, both monotonic microseconds,
	// t2 the ADSBee's UTC epoch at reply time. The ADSBee answers in one step, so t2 = t3.
	struct TimeSample {
		bool answered = false;
		int64_t t1_us = 0, t4_us = 0;
		uint64_t t2_ms = 0;
		uint32_t pps = 0;
	};

	struct TimeEstimate {
		bool valid = false;
		int64_t rtt_us = 0;
		int64_t t4_us = 0; // monotonic arrival of the chosen reply
		int64_t utc_at_t4_us = 0; // UTC in microseconds at that arrival
		uint32_t pps = 0;
	};

	// Picks the answered sample with the smallest round trip among those whose receiver had
	// PPS, the NTP clock filter. Only PPS-disciplined epochs are precise enough to apply.
	TimeEstimate estimate_time(const TimeSample* samples, size_t count);
}
