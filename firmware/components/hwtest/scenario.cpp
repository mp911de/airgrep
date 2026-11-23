#include "scenario.hpp"

#include <algorithm>
#include <cmath>

#include "navigation/units.h"

namespace scenario {
namespace {
constexpr float kStandardTurnDps = 3;
constexpr float kHoldLegS = 60;
constexpr float kHalfCircleS = 180 / kStandardTurnDps;
constexpr float kClimbFpm = 500;
constexpr float kLowestFt = 1000, kLevelStepFt = 500;
constexpr uint32_t kLevels = 18;
constexpr float kSlowestKt = 60, kFastestKt = 250;
// High and fast enough that the bands of the engine reach all the way to
// either side.
constexpr float kOwnFt = 5000, kOwnKt = 100;
constexpr size_t kEncounterSpacing = 16;
constexpr float kEarliestEncounterS = 20, kLatestEncounterS = 240,
				kNearFt = 300;
constexpr float kEarliestCrowdedEncounterS = 25, kLatestCrowdedEncounterS = 40;

class Xorshift32 {
   public:
	explicit Xorshift32(uint32_t seed) : state_(seed ? seed : 1) {}
	uint32_t next() {
		state_ ^= state_ << 13;
		state_ ^= state_ >> 17;
		state_ ^= state_ << 5;
		return state_;
	}
	// From 0 up to, not including, 1.
	float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }
	float between(float low, float high) { return low + unit() * (high - low); }

   private:
	uint32_t state_;
};

void place_on_intercept_course(Aircraft& aircraft,
							   const Aircraft& northbound_ownship,
							   float meets_s, float cos_centre_latitude) {
	const float own_deg = northbound_ownship.groundspeed_kt * meets_s / 3600 /
						  units::nm_per_degree_latitude;
	const float flown_deg = aircraft.groundspeed_kt * meets_s / 3600 /
							units::nm_per_degree_latitude;
	const float track =
		aircraft.true_track_deg * units::radians_per_degree<float>;
	aircraft.latitude_deg =
		northbound_ownship.latitude_deg + own_deg - flown_deg * std::cos(track);
	aircraft.longitude_deg = northbound_ownship.longitude_deg -
							 flown_deg * std::sin(track) / cos_centre_latitude;
}

float level(Xorshift32& random) {
	return kLowestFt +
		   kLevelStepFt * static_cast<float>(random.next() % kLevels);
}
}

Traffic::Traffic(size_t count, uint32_t seed, double latitude_deg,
				 double longitude_deg, size_t early_encounters)
	: latitude_(latitude_deg),
	  longitude_(longitude_deg),
	  cos_centre_latitude_(std::cos(static_cast<float>(latitude_deg) *
									units::radians_per_degree<float>)),
	  aircraft_(count + 1),
	  plans_(count + 1) {
	Xorshift32 random(seed);
	aircraft_[0] = {latitude_deg, longitude_deg, kOwnFt, 0, kOwnKt, 0};
	plans_[0].target_altitude_ft = kOwnFt;
	const float half_deg = kSquareSideNm / 2 / units::nm_per_degree_latitude;
	for (size_t i = 1; i < aircraft_.size(); i++) {
		Aircraft& aircraft = aircraft_[i];
		Plan& plan = plans_[i];
		aircraft.latitude_deg =
			latitude_deg + random.between(-half_deg, half_deg);
		aircraft.longitude_deg =
			longitude_deg +
			random.between(-half_deg, half_deg) / cos_centre_latitude_;
		aircraft.altitude_ft = level(random);
		aircraft.true_track_deg = random.between(0, 360);
		aircraft.groundspeed_kt = random.between(kSlowestKt, kFastestKt);
		plan.target_altitude_ft = aircraft.altitude_ft;
		if (i <= early_encounters || i % kEncounterSpacing == 1) {
			const float meets_s =
				i <= early_encounters
					? random.between(kEarliestCrowdedEncounterS,
									 kLatestCrowdedEncounterS)
					: random.between(kEarliestEncounterS, kLatestEncounterS);
			place_on_intercept_course(aircraft, aircraft_[0], meets_s,
									  cos_centre_latitude_);
			aircraft.altitude_ft = kOwnFt + random.between(-kNearFt, kNearFt);
			plan.target_altitude_ft = aircraft.altitude_ft;
			continue;
		}
		switch (random.next() % 15) {
			case 0:
				plan.turn_dps =
					random.next() % 2 ? kStandardTurnDps : -kStandardTurnDps;
				break;
			case 1:
				plan.turn_dps = kStandardTurnDps;
				plan.holds = true;
				break;
			case 2:
				plan.target_altitude_ft = level(random);
				break;
			default:
				break;
		}
	}
}

void Traffic::fly(Aircraft& aircraft, Plan& plan, float seconds) const {
	if (plan.holds) {
		plan.hold_phase_elapsed_s += seconds;
		const bool turning = plan.hold_phase_elapsed_s >= kHoldLegS;
		if (plan.hold_phase_elapsed_s >= kHoldLegS + kHalfCircleS) {
			plan.hold_phase_elapsed_s -= kHoldLegS + kHalfCircleS;
		}
		aircraft.true_track_deg += turning ? plan.turn_dps * seconds : 0;
	} else {
		aircraft.true_track_deg += plan.turn_dps * seconds;
	}

	const float behind_ft = plan.target_altitude_ft - aircraft.altitude_ft;
	const float climbed_ft = std::clamp(behind_ft, -kClimbFpm * seconds / 60,
										kClimbFpm * seconds / 60);
	aircraft.vertical_fpm = seconds > 0 ? climbed_ft * 60 / seconds : 0;
	aircraft.altitude_ft += climbed_ft;

	const float flown_deg = aircraft.groundspeed_kt * seconds / 3600 /
							units::nm_per_degree_latitude;
	const float track =
		aircraft.true_track_deg * units::radians_per_degree<float>;
	aircraft.latitude_deg += flown_deg * std::cos(track);
	aircraft.longitude_deg +=
		flown_deg * std::sin(track) / cos_centre_latitude_;

	reflect_at_boundary(aircraft);
}

void Traffic::reflect_at_boundary(Aircraft& aircraft) const {
	const double half_lat =
		kSquareSideNm / (2.0 * units::nm_per_degree_latitude);
	const auto reflect = [](double& position, double centre,
							double half_width) {
		if (std::abs(position - centre) <= half_width) {
			return false;
		}
		double phase =
			std::fmod(position - centre + half_width, 4 * half_width);
		if (phase < 0) {
			phase += 4 * half_width;
		}
		const bool reversed = phase > 2 * half_width;
		position =
			centre - half_width + (reversed ? 4 * half_width - phase : phase);
		return reversed;
	};
	if (reflect(aircraft.latitude_deg, latitude_, half_lat)) {
		aircraft.true_track_deg = 180 - aircraft.true_track_deg;
	}
	if (reflect(aircraft.longitude_deg, longitude_,
				half_lat / cos_centre_latitude_)) {
		aircraft.true_track_deg = -aircraft.true_track_deg;
	}
	aircraft.true_track_deg = std::fmod(aircraft.true_track_deg, 360.0f);
	if (aircraft.true_track_deg < 0) {
		aircraft.true_track_deg += 360;
	}
}

void Traffic::step(float seconds) {
	for (size_t i = 0; i < aircraft_.size(); i++) {
		fly(aircraft_[i], plans_[i], seconds);
	}
}
}
