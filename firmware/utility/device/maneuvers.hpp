#pragma once

#include <cstddef>
#include <span>

// Positions are relative to the center of the racetrack.
namespace device::maneuvers {

// One way to meet the own ship. Times count from the meeting, in seconds,
// and are negative before it.
struct Maneuver {
	const char* callsign;
	unsigned char adsbee_emitter_category;
	// Before its turn, against the track of the own ship at the meeting.
	double track_deg;
	double speed_kt;
	// The turn: its rate, right is positive, and when it begins and ends.
	double turn_dps, turn_from_s, turn_to_s;
	// The course the target holds at this time leads through the meeting
	// point. Before the turn for one that turns away, after it for one that
	// turns in.
	double aimed_s;
	double height_above_own_at_meeting_ft, rate_fpm;
};

inline constexpr double kOwnKt = 90, kLegS = 120, kTurnS = 60,
						kEncounterS = kLegS + kTurnS;
inline constexpr double kMeetingFromLegStartS = 90;

std::span<const Maneuver> all();
// The place in all() of what the encounter of this number flies. Every round
// of all() flies each one once, in an order that a fixed seed shuffles, and
// no encounter repeats the one before.
size_t flown(long encounter);

struct State {
	double east_nm, north_nm, track_deg, speed_kt, turn_dps;
	double relative_altitude_ft, rate_fpm;
};

State own(double seconds);
State target(double seconds);
}
