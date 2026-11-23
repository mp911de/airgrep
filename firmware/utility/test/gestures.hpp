#pragma once

#include <algorithm>
#include <charconv>
#include <string_view>

#include "app/display.h"
#include "device/fixtures.hpp"

namespace gestures {
// A finger on the panel in display time. A hold is a drag that keeps the
// finger down, a wait no finger at all for its milliseconds. gnss is no
// finger either: the device is handed the GNSS data of a fixture, as its
// receiver would report it next. device lets the device run on for its
// milliseconds: a fixture stays at its scene until then.
struct Gesture {
	enum Kind : uint8_t { tap, drag, hold, wait, gnss, device } kind{};
	int32_t from_x{}, from_y{}, to_x{}, to_y{};
	uint32_t wait_ms{};
	adsbee::Gnss reported{};
};
constexpr uint32_t kGestureMs = 900, kDownMs = 100, kTapUpMs = 200,
				   kLockMs = 200, kArrivedMs = 500, kDragUpMs = 600;
// The first move alone passes the scroll lock of LVGL.
constexpr int32_t kLockStep = 17;

inline bool parse(std::string_view text, Gesture& gesture) {
	constexpr std::string_view kinds[] = {
		"tap:", "drag:", "hold:", "wait:", "gnss:", "device:"};
	const auto* kind = std::ranges::find_if(
		kinds, [&](std::string_view name) { return text.starts_with(name); });
	if (kind == std::end(kinds)) {
		return false;
	}
	gesture.kind = static_cast<Gesture::Kind>(kind - kinds);
	text.remove_prefix(kind->size());
	if (gesture.kind == Gesture::gnss) {
		try {
			gesture.reported = device::fixtures::gnss_of(text);
		} catch (const device::Error&) {
			return false;
		}
		return true;
	}
	int32_t values[4]{};
	const bool lasts =
		gesture.kind == Gesture::wait || gesture.kind == Gesture::device;
	const size_t count = lasts ? 1 : gesture.kind == Gesture::tap ? 2 : 4;
	for (size_t i = 0; i < count; i++) {
		const auto [end, error] =
			std::from_chars(text.data(), text.data() + text.size(), values[i]);
		const bool last = i == count - 1;
		if (error != std::errc{} ||
			(last ? end != text.data() + text.size()
				  : end == text.data() + text.size() || *end != ',')) {
			return false;
		}
		text.remove_prefix(static_cast<size_t>(end - text.data()) + !last);
	}
	if (lasts) {
		gesture.wait_ms = static_cast<uint32_t>(std::max(values[0], 0));
		return true;
	}
	gesture.from_x = values[0];
	gesture.from_y = values[1];
	gesture.to_x = gesture.kind == Gesture::tap ? values[0] : values[2];
	gesture.to_y = gesture.kind == Gesture::tap ? values[1] : values[3];
	return true;
}

inline void touch(const airgrep_display_api& display, const Gesture& gesture,
				  uint32_t elapsed_ms) {
	if (gesture.kind >= Gesture::wait || elapsed_ms < kDownMs) {
		return;
	}
	const int32_t start_x =
		gesture.from_x +
		std::clamp(gesture.to_x - gesture.from_x, -kLockStep, kLockStep);
	const int32_t start_y =
		gesture.from_y +
		std::clamp(gesture.to_y - gesture.from_y, -kLockStep, kLockStep);
	const auto moved = static_cast<int32_t>(
		std::clamp(elapsed_ms, kLockMs, kArrivedMs) - kLockMs);
	const auto span = static_cast<int32_t>(kArrivedMs - kLockMs);
	const bool locked = elapsed_ms < kLockMs;
	const bool down =
		gesture.kind == Gesture::hold ||
		elapsed_ms < (gesture.kind == Gesture::tap ? kTapUpMs : kDragUpMs);
	display.touch(locked ? gesture.from_x
						 : start_x + (gesture.to_x - start_x) * moved / span,
				  locked ? gesture.from_y
						 : start_y + (gesture.to_y - start_y) * moved / span,
				  down);
}
}
