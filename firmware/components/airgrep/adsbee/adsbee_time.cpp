#include "adsbee_time.hpp"

#include <cstring>
#include <string_view>
#include <utility>

#include "csbee_fields.hpp"

namespace adsbee {
bool decode_time_ping(const char* text, size_t length, TimePing& out) {
	constexpr size_t prefix = sizeof ADSBEE_TIME_PING_REPLY - 1;
	if (length < prefix ||
		std::memcmp(text, ADSBEE_TIME_PING_REPLY, prefix) != 0) {
		return false;
	}
	std::string_view fields(text + prefix, length - prefix);
	const size_t first = fields.find(',');
	if (first == fields.npos ||
		!csbee::integer({fields.data(), first}, out.echo)) {
		return false;
	}
	fields.remove_prefix(first + 1);
	const size_t second = fields.find(',');
	if (second == fields.npos) {
		return false;
	}
	out.epoch_known = second != 0;
	out.epoch_ms = 0;
	if (out.epoch_known &&
		(!csbee::integer({fields.data(), second}, out.epoch_ms) ||
		 out.epoch_ms > INT64_MAX / 1000)) {
		return false;
	}
	fields.remove_prefix(second + 1);
	return csbee::integer({fields.data(), fields.size()}, out.pps);
}

TimeEstimate estimate_time(const TimeSample* samples, size_t count) {
	TimeEstimate best;
	for (size_t i = 0; i < count; i++) {
		const TimeSample& s = samples[i];
		if (!s.answered || !s.pps || s.t1_us < 0 || s.t4_us < s.t1_us) {
			continue;
		}
		const int64_t rtt = s.t4_us - s.t1_us;
		if ((best.valid && rtt >= best.rtt_us) ||
			std::cmp_greater(s.t2_ms, (INT64_MAX - rtt / 2) / 1000)) {
			continue;
		}
		best.valid = true;
		best.rtt_us = rtt;
		best.reply_received_us = s.t4_us;
		best.utc_at_reply_us = int64_t(s.t2_ms) * 1000 + rtt / 2;
		best.pps = s.pps;
	}
	return best;
}
}
