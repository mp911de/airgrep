#pragma once

#include "conflicts/conflicts.hpp"

namespace conflicts {
// How finely the engine tries each Guide, and how far. Each value it tries is
// a flight path checked against all traffic, so these set what Bands cost.
inline constexpr float kTrackStepDeg = 20;
inline constexpr float kSpeedStepKt = 10;
inline constexpr float kMinSpeedKt = 0, kMaxSpeedKt = 500;
inline constexpr float kVerticalStepFpm = 100, kMaxVerticalSpeedFpm = 2000;
inline constexpr float kAltitudeStepFt = 250;
// DAIDALUS takes no altitude below zero.
inline constexpr float kMinAltitudeFt = 0;
// Once well clear is lost a Band is a way out, and one counts only when it
// holds this long. DAIDALUS asks for 3 s, and then the ways out come in
// slices that change from second to second.
inline constexpr float kRecoveryStableS = 10;
}
