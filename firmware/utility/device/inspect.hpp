#pragma once

#include <string>

#include "device_state.hpp"

namespace device {
struct Inspection {
	std::string text;
	int fields = 0, problems = 0;
	std::string first_invalid_field;
};

inline int64_t end_of_uptime_second_us(const Hello& hello) {
	return static_cast<int64_t>(hello.uptime_seconds) * 1'000'000 + 999'999;
}

Inspection inspect(const DeviceState& picture, int64_t latest_timestamp_us);
Inspection inspect(const TrafficListing& listing);
void require_valid(const Inspection& inspection);
Inspection inspect(const Hello& hello);
}
