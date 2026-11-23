#pragma once
#include <stdint.h>

using EspErrorCode = int;

enum class Status : uint8_t { unknown, ok, off };

// One rule for every driver State: usable while the last successful Poll is
// at most 5 s old, so a single failed Poll changes nothing.
constexpr int64_t kStateMaxAgeUs = 5000000;

inline bool state_usable(int64_t updated_us, int64_t now_us) {
	return updated_us && now_us - updated_us <= kStateMaxAgeUs;
}

// Ready: detected, configured and read at least once. A driver that fails to
// get there stops, the system watchdog starts it again.
template <typename State>
bool is_ready(const State& state) {
	return state.identity_known && state.configured && state.updated_us;
}

enum class BatteryDetected : uint8_t {
	unknown,
	test_present,
	absent,
	voltage_present
};

inline const char* battery_detected_name(BatteryDetected detected) {
	static const char* const names[] = {"UNKNOWN", "YES", "NO",
										"VBAT_DETECTED"};
	return names[static_cast<uint8_t>(detected)];
}
