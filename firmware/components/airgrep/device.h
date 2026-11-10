#pragma once
#include <stdint.h>

enum class Status : uint8_t { unknown, ok, off };

// Battery presence as seen by one source. yes and no come from the battery test, vbat from
// a live cell voltage without a test.
enum class BatteryDetected : uint8_t { unknown, yes, no, vbat };

inline const char* battery_detected_name(BatteryDetected detected) {
	static const char* names[] = {"UNKNOWN", "YES", "NO", "VBAT_DETECTED"};
	return names[uint8_t(detected)];
}
