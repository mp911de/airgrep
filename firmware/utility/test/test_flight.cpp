#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "check.hpp"
#include "device/emulator.hpp"
#include "device/maneuvers.hpp"
#include "display/display_mapping.hpp"
#include "navigation/units.h"
#include "traffic/traffic_table.hpp"

namespace {
namespace maneuvers = device::maneuvers;

const traffic::Target& encounter_target_at(const device::EmulatedDevice& device,
										   double seconds) {
	const long encounter = static_cast<long>(seconds / maneuvers::kEncounterS);
	const std::string callsign =
		maneuvers::all()[maneuvers::flown(encounter)].callsign;
	for (const traffic::Target& target : device.states.traffic) {
		if (callsign == target.callsign) {
			return target;
		}
	}
	throw std::runtime_error("no target flies " + callsign);
}

struct Step {
	double nm, track_deg;
};

// From one place to another, close enough to be flat.
Step between(double latitude, double longitude, double to_latitude,
			 double to_longitude) {
	const double north =
		(to_latitude - latitude) * units::nm_per_degree_latitude;
	const double east = (to_longitude - longitude) *
						units::nm_per_degree_latitude *
						std::cos(latitude * units::radians_per_degree<double>);
	return {
		std::hypot(east, north),
		std::fmod(
			std::atan2(east, north) / units::radians_per_degree<double> + 360,
			360.0)};
}

double apart_deg(double a, double b) {
	return std::fabs(std::remainder(a - b, 360.0));
}

void same_every_time() {
	device::EmulatedDevice first = device::make_booted_service_device(),
						   second = device::make_booted_service_device();
	device::fly(first, 1234.5);
	device::fly(second, 77);
	device::fly(second, 1234.5);
	CHECK(std::memcmp(first.states.traffic.data(), second.states.traffic.data(),
					  first.states.traffic.size() * sizeof(traffic::Target)) ==
		  0);
	CHECK_EQUAL(first.states.adsbee.gnss.latitude,
				second.states.adsbee.gnss.latitude);
	CHECK_EQUAL(first.states.adsbee.gnss.longitude,
				second.states.adsbee.gnss.longitude);
	CHECK_EQUAL(first.states.adsbee.gnss.track_deg,
				second.states.adsbee.gnss.track_deg);
}

void every_maneuver_in_a_round() {
	const size_t count = maneuvers::all().size();
	for (long round = 0; round < 8; round++) {
		std::set<size_t> flown;
		for (size_t i = 0; i < count; i++) {
			flown.insert(maneuvers::flown(round * static_cast<long>(count) +
										  static_cast<long>(i)));
		}
		CHECK_EQUAL(count, flown.size());
	}
	for (long encounter = 1; encounter < 8 * static_cast<long>(count);
		 encounter++) {
		CHECK(maneuvers::flown(encounter) != maneuvers::flown(encounter - 1));
	}
	bool in_order = true;
	for (size_t i = 0; i < count; i++) {
		in_order &= maneuvers::flown(static_cast<long>(i)) == i;
	}
	CHECK(!in_order);
}

// Positions are floats of a degree, a few metres at this latitude, so a step
// spans several seconds.
constexpr int kStepS = 5;

void own_ship_flies_what_it_reports() {
	device::EmulatedDevice device = device::make_booted_service_device();
	for (int second = 0; second < 2 * maneuvers::kEncounterS;
		 second += kStepS) {
		device::fly(device, second);
		const adsbee::Gnss before = device.states.adsbee.gnss;
		device::fly(device, second + kStepS);
		const adsbee::Gnss& after = device.states.adsbee.gnss;
		const Step step = between(before.latitude, before.longitude,
								  after.latitude, after.longitude);
		CHECK(std::fabs(step.nm - before.speed_kt * kStepS / 3600) < 0.01);

		const double turned =
			std::remainder(after.track_deg - before.track_deg, 360.0);
		const double midpoint_track_deg = before.track_deg + turned / 2;
		CHECK(apart_deg(step.track_deg, midpoint_track_deg) < 2);
	}
}

void targets_fly_what_they_report() {
	device::EmulatedDevice device = device::make_booted_service_device();
	const size_t count = maneuvers::all().size();
	for (size_t encounter = 0; encounter < count; encounter++) {
		const double begins =
			static_cast<double>(encounter) * maneuvers::kEncounterS;
		for (int second = 0; second + kStepS < maneuvers::kEncounterS;
			 second += kStepS) {
			device::fly(device, begins + second);
			const traffic::Target before =
				encounter_target_at(device, begins + second);
			device::fly(device, begins + second + kStepS);
			const traffic::Target& after =
				encounter_target_at(device, begins + second);
			const Step step = between(before.latitude, before.longitude,
									  after.latitude, after.longitude);
			// A chord of a turn is a little shorter than its arc.
			CHECK(std::fabs(step.nm - before.speed_kt * kStepS / 3600.0) <
				  0.01);
			const double turned =
				std::remainder(after.track_deg - before.track_deg, 360.0);
			if (before.speed_kt > 0) {
				CHECK(apart_deg(step.track_deg, before.track_deg + turned / 2) <
					  2);
			}
			CHECK_EQUAL(0.0f, after.flarm_turn_rate_dps);
			const auto initial_turn =
				maneuvers::target(begins + second).turn_dps;
			const auto final_turn =
				maneuvers::target(begins + second + kStepS).turn_dps;
			if (initial_turn == final_turn) {
				CHECK(std::fabs(turned - initial_turn * kStepS) < 0.5);
			}
			CHECK(std::abs(after.baro_alt_ft - before.baro_alt_ft -
						   before.baro_rate_fpm * kStepS / 60) <= 1);
		}
	}
}

void meets_the_own_ship() {
	device::EmulatedDevice device = device::make_booted_service_device();
	const size_t count = maneuvers::all().size();
	for (size_t encounter = 0; encounter < count; encounter++) {
		const maneuvers::Maneuver& maneuver =
			maneuvers::all()[maneuvers::flown(static_cast<long>(encounter))];
		const double at =
			static_cast<double>(encounter) * maneuvers::kEncounterS +
			maneuvers::kMeetingFromLegStartS;
		device::fly(device, at);
		const traffic::Target& target = encounter_target_at(device, at);
		const adsbee::Gnss& own = device.states.adsbee.gnss;
		const Step step = between(own.latitude, own.longitude, target.latitude,
								  target.longitude);
		if (maneuver.aimed_s == 0) {
			CHECK(step.nm < 0.02);
			CHECK_EQUAL(
				own.altitude_ft + static_cast<int32_t>(
									  maneuver.height_above_own_at_meeting_ft),
				target.baro_alt_ft);
		} else {
			CHECK(step.nm > 0.5);
		}
	}
}

void print_alert_onsets_by_maneuver() {
	constexpr const char* kNames[] = {"other", "proximate", "advisory",
									  "alert"};
	device::EmulatedDevice device = device::make_booted_service_device();
	conflicts::Detector conflicts;
	std::vector<traffic::Tracked> listed(traffic::kListed);
	std::array<conflicts::AlertingResult, traffic::kListed> results;
	const size_t count = maneuvers::all().size();
	std::printf("maneuver   seconds to the meeting at which a level begins\n");
	for (size_t encounter = 0; encounter < count; encounter++) {
		const maneuvers::Maneuver& maneuver =
			maneuvers::all()[maneuvers::flown(static_cast<long>(encounter))];
		int began[4] = {0, 0, 0, 0};
		bool reached[4] = {};
		for (int second = 0; second < maneuvers::kEncounterS; second++) {
			const double now =
				static_cast<double>(encounter) * maneuvers::kEncounterS +
				second;
			device::fly(device, now);
			const auto table = device.traffic_table();
			const adsbee::Gnss& gnss = device.states.adsbee.gnss;
			const traffic::Reference own{gnss.latitude, gnss.longitude};
			const size_t heard =
				table.nearest(&own, listed.data(), listed.size());
			const conflicts::ConflictInput sources{
				.adsbee = device.states.adsbee,
				.traffic = std::span(listed).first(heard),
				.barometer = device.states.barometer,

				.now_us = device.states.adsbee.last_gnss_report_us,
			};
			const size_t result_count =
				conflicts.detect(sources, results).value_or(0);
			const uint32_t address = encounter_target_at(device, now).address;
			for (size_t i = 0; i < result_count; i++) {
				const auto level =
					static_cast<size_t>(results[i].conflict.level);
				if (results[i].target.address == address && !reached[level]) {
					reached[level] = true;
					began[level] =
						second -
						static_cast<int>(maneuvers::kMeetingFromLegStartS);
				}
			}
		}
		std::printf("%-9s", maneuver.callsign);
		for (size_t level = 1; level < 4; level++) {
			if (reached[level]) {
				std::printf("  %s %+d", kNames[level], began[level]);
			}
		}
		std::printf("%s\n", reached[1] || reached[2] || reached[3]
								? ""
								: "  other only");
	}
}
}

int main(int argc, char**) {
	if (argc > 1) {
		print_alert_onsets_by_maneuver();
		return 0;
	}
	return check::run({
		TEST(same_every_time),
		TEST(every_maneuver_in_a_round),
		TEST(own_ship_flies_what_it_reports),
		TEST(targets_fly_what_they_report),
		TEST(meets_the_own_ship),
	});
}
