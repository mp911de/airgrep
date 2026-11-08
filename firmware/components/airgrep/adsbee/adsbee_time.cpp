#include "adsbee_time.hpp"
#include <cstdlib>
#include <cstring>

namespace adsbee {
	bool decode_time_ping(const char* text, size_t length, TimePing& out) {
		constexpr size_t prefix = sizeof ADSBEE_TIME_PING_REPLY - 1;
		if (length < prefix || std::memcmp(text, ADSBEE_TIME_PING_REPLY, prefix) != 0) return false;
		char buffer[64];
		size_t n = length - prefix < sizeof buffer - 1 ? length - prefix : sizeof buffer - 1;
		std::memcpy(buffer, text + prefix, n);
		buffer[n] = 0;
		char* first = std::strchr(buffer, ',');
		if (!first) return false;
		char* second = std::strchr(first + 1, ',');
		if (!second) return false;
		*first = *second = 0;
		if (!*buffer || !*(second + 1)) return false;
		out = TimePing{};
		out.echo = std::strtoull(buffer, nullptr, 10);
		out.epoch_known = *(first + 1) != 0;
		if (out.epoch_known) out.epoch_ms = std::strtoull(first + 1, nullptr, 10);
		out.pps = uint32_t(std::strtoul(second + 1, nullptr, 10));
		return true;
	}

	TimeEstimate estimate_time(const TimeSample* samples, size_t count) {
		TimeEstimate best;
		for (size_t i = 0; i < count; i++) {
			const TimeSample& s = samples[i];
			if (!s.answered || !s.pps || s.t4_us < s.t1_us) continue;
			int64_t rtt = s.t4_us - s.t1_us;
			if (best.valid && rtt >= best.rtt_us) continue;
			best.valid = true;
			best.rtt_us = rtt;
			best.t4_us = s.t4_us;
			best.utc_at_t4_us = int64_t(s.t2_ms) * 1000 + rtt / 2;
			best.pps = s.pps;
		}
		return best;
	}
}
