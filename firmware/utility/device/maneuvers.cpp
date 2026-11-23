#include "maneuvers.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>
#include <utility>

#include "navigation/units.h"

namespace device::maneuvers {
namespace {
constexpr uint64_t kSeed = 0x61697267726571;

constexpr Maneuver kManeuvers[] = {
	{.callsign = "HEADON",
	 .adsbee_emitter_category = 1,
	 .track_deg = 180,
	 .speed_kt = 100},
	{.callsign = "CROSSING",
	 .adsbee_emitter_category = 1,
	 .track_deg = 270,
	 .speed_kt = 100},
	{.callsign = "OVERTAKE", .adsbee_emitter_category = 1, .speed_kt = 160},
	{.callsign = "TURNIN",
	 .adsbee_emitter_category = 1,
	 .track_deg = 180,
	 .speed_kt = 100,
	 .turn_dps = 3,
	 .turn_from_s = -60,
	 .turn_to_s = -30},
	{.callsign = "TURNAWAY",
	 .adsbee_emitter_category = 1,
	 .track_deg = 180,
	 .speed_kt = 100,
	 .turn_dps = 3,
	 .turn_from_s = -35,
	 .turn_to_s = -5,
	 .aimed_s = -kMeetingFromLegStartS},
	{.callsign = "CLIMB",
	 .adsbee_emitter_category = 1,
	 .track_deg = 180,
	 .speed_kt = 90,
	 .rate_fpm = 1000},
	{.callsign = "DESCEND",
	 .adsbee_emitter_category = 1,
	 .track_deg = 90,
	 .speed_kt = 110,
	 .rate_fpm = -800},
	{.callsign = "HOVER",
	 .adsbee_emitter_category = 7,
	 .height_above_own_at_meeting_ft = -100},
};
constexpr size_t kCount = std::size(kManeuvers);
constexpr size_t kOpeningHeadOn = 0;
static_assert(std::string_view(kManeuvers[kOpeningHeadOn].callsign) ==
			  "HEADON");
constexpr double kOwnTurnRadiusNm = kOwnKt / 3600 * kTurnS / std::numbers::pi;
constexpr double kHalfLeg = kOwnKt / 3600 * kLegS / 2;

uint64_t mixed(uint64_t value) {
	value += 0x9e3779b97f4a7c15;
	value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9;
	value = (value ^ (value >> 27U)) * 0x94d049bb133111eb;
	return value ^ (value >> 31U);
}

std::array<size_t, kCount> shuffled(long round) {
	std::array<size_t, kCount> order{};
	for (size_t i = 0; i < kCount; i++) {
		order[i] = i;
	}
	uint64_t random = mixed(kSeed + static_cast<uint64_t>(round));
	for (size_t i = kCount - 1; i > 0; i--) {
		random = mixed(random);
		std::swap(order[i], order[random % (i + 1)]);
	}
	if (round == 0) {
		std::iter_swap(order.begin(), std::ranges::find(order, kOpeningHeadOn));
	}
	return order;
}

struct Place {
	double east, north;
};

Place along(double track_deg, double distance) {
	return {distance * std::sin(track_deg * units::radians_per_degree<double>),
			distance * std::cos(track_deg * units::radians_per_degree<double>)};
}

// The track of a maneuver at a time, against the own one at the meeting.
double track(const Maneuver& maneuver, double time) {
	return maneuver.track_deg +
		   maneuver.turn_dps *
			   (std::clamp(time, maneuver.turn_from_s, maneuver.turn_to_s) -
				maneuver.turn_from_s);
}

// Where a maneuver is at a time, from where its turn begins.
Place flown_to(const Maneuver& maneuver, double time) {
	const double mile_per_s = maneuver.speed_kt / 3600;
	const double from = maneuver.turn_from_s;
	const double to = maneuver.turn_to_s;
	if (time <= from) {
		return along(maneuver.track_deg, mile_per_s * (time - from));
	}
	const double turned = std::min(time, to);
	const double begin = maneuver.track_deg * units::radians_per_degree<double>;
	const double end =
		track(maneuver, turned) * units::radians_per_degree<double>;
	const double rate = maneuver.turn_dps * units::radians_per_degree<double>;
	const Place arc =
		rate != 0 ? Place{mile_per_s * (std::cos(begin) - std::cos(end)) / rate,
						  mile_per_s * (std::sin(end) - std::sin(begin)) / rate}
				  : along(maneuver.track_deg, mile_per_s * (turned - from));
	const Place straight =
		along(track(maneuver, to), mile_per_s * (time - turned));
	return {arc.east + straight.east, arc.north + straight.north};
}
}

std::span<const Maneuver> all() { return kManeuvers; }

size_t flown(long encounter) {
	const long round = encounter / static_cast<long>(kCount);
	std::array<size_t, kCount> order = shuffled(0);
	for (long earlier = 1; earlier <= round; earlier++) {
		const size_t last = order.back();
		order = shuffled(earlier);
		if (order.front() == last) {
			std::swap(order.front(), order.back());
		}
	}
	return order[encounter % kCount];
}

State own(double seconds) {
	const double lap = 2 * kEncounterS;
	const double phase = std::fmod(seconds, lap);
	// The second half of the lap is the first one, turned around.
	const double side = phase < kEncounterS ? 1 : -1;
	const double within = std::fmod(phase, kEncounterS);
	const double eastbound = side > 0 ? 90 : 270;
	if (within < kLegS) {
		return {side * (kOwnKt / 3600 * within - kHalfLeg),
				side * kOwnTurnRadiusNm,
				eastbound,
				kOwnKt,
				0,
				0,
				0};
	}
	const double turned = 180 * (within - kLegS) / kTurnS;
	return {side * (kHalfLeg +
					kOwnTurnRadiusNm *
						std::sin(turned * units::radians_per_degree<double>)),
			side * kOwnTurnRadiusNm *
				std::cos(turned * units::radians_per_degree<double>),
			std::fmod(eastbound + turned, 360.0),
			kOwnKt,
			180 / kTurnS,
			0,
			0};
}

State target(double seconds) {
	const long encounter = static_cast<long>(std::floor(seconds / kEncounterS));
	const Maneuver& maneuver = kManeuvers[flown(encounter)];
	const double begins = static_cast<double>(encounter) * kEncounterS;
	const double time = seconds - begins - kMeetingFromLegStartS;
	const State meeting = own(begins + kMeetingFromLegStartS);
	const Place at = flown_to(maneuver, time);
	const Place aimed = flown_to(maneuver, maneuver.aimed_s);
	const Place onward = along(track(maneuver, maneuver.aimed_s),
							   maneuver.speed_kt / 3600 * -maneuver.aimed_s);
	const Place own_track_frame = {at.east - aimed.east - onward.east,
								   at.north - aimed.north - onward.north};
	const double own_track =
		meeting.track_deg * units::radians_per_degree<double>;
	const bool turning =
		time > maneuver.turn_from_s && time < maneuver.turn_to_s;
	return {
		meeting.east_nm + own_track_frame.east * std::cos(own_track) +
			own_track_frame.north * std::sin(own_track),
		meeting.north_nm - own_track_frame.east * std::sin(own_track) +
			own_track_frame.north * std::cos(own_track),
		std::fmod(meeting.track_deg + track(maneuver, time) + 720, 360.0),
		maneuver.speed_kt,
		turning ? maneuver.turn_dps : 0,
		maneuver.height_above_own_at_meeting_ft + maneuver.rate_fpm * time / 60,
		maneuver.rate_fpm};
}
}
