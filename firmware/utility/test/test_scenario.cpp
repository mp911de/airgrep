#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

#include "benchmark.hpp"
#include "check.hpp"
#include "conflicts/conflicts.hpp"
#include "navigation/units.h"
#include "scenario.hpp"

namespace {
constexpr double kLatitude = 49.47, kLongitude = 8.51;

struct Offset {
	float north_nm, east_nm;
};

Offset offset(const scenario::Aircraft& aircraft) {
	const float east = std::cos(static_cast<float>(kLatitude) *
								units::radians_per_degree<float>);
	return {static_cast<float>(aircraft.latitude_deg - kLatitude) *
				units::nm_per_degree_latitude,
			static_cast<float>(aircraft.longitude_deg - kLongitude) *
				units::nm_per_degree_latitude * east};
}

void same_seed() {
	scenario::Traffic first(50, 7, kLatitude, kLongitude);
	scenario::Traffic second(50, 7, kLatitude, kLongitude);
	scenario::Traffic other(50, 8, kLatitude, kLongitude);
	for (int step = 0; step < 300; step++) {
		first.step(1);
		second.step(1);
		other.step(1);
	}
	bool same = true, differs = false;
	for (size_t i = 0; i < 50; i++) {
		same &= first.traffic()[i].latitude_deg ==
					second.traffic()[i].latitude_deg &&
				first.traffic()[i].true_track_deg ==
					second.traffic()[i].true_track_deg;
		differs |=
			first.traffic()[i].latitude_deg != other.traffic()[i].latitude_deg;
	}
	CHECK(same);
	CHECK(differs);
}

void stays_in_the_square() {
	// Half an hour: the fastest crosses the square more than three times.
	scenario::Traffic sky(400, 1, kLatitude, kLongitude);
	float furthest = 0;
	bool levels = true, tracks = true;
	for (int step = 0; step < 1800; step++) {
		sky.step(1);
		for (const scenario::Aircraft& aircraft : sky.traffic()) {
			const Offset at = offset(aircraft);
			furthest = std::max(
				{furthest, std::abs(at.north_nm), std::abs(at.east_nm)});
			levels &=
				aircraft.altitude_ft >= 1000 && aircraft.altitude_ft <= 9500;
			tracks &=
				aircraft.true_track_deg >= 0 && aircraft.true_track_deg < 360;
		}
	}
	CHECK(furthest <= scenario::kSquareSideNm / 2 + 0.00001f);
	CHECK(levels);
	CHECK(tracks);
	const Offset own = offset(sky.own());
	CHECK(std::abs(own.north_nm) <= scenario::kSquareSideNm / 2 + 0.00001f);
}

void some_fly_something() {
	scenario::Traffic sky(400, 1, kLatitude, kLongitude);
	std::vector<scenario::Aircraft> before(sky.traffic().begin(),
										   sky.traffic().end());
	size_t turned = 0, climbed = 0, unchanged = 0;
	sky.step(1);
	for (int step = 0; step < 20; step++) {
		sky.step(1);
	}
	for (size_t i = 0; i < before.size(); i++) {
		const scenario::Aircraft& now = sky.traffic()[i];
		const Offset at = offset(now);
		const bool inside =
			std::abs(at.north_nm) < 18 && std::abs(at.east_nm) < 18;
		turned += inside && now.true_track_deg != before[i].true_track_deg;
		unchanged += now.true_track_deg == before[i].true_track_deg &&
					 now.altitude_ft == before[i].altitude_ft;
		climbed += now.altitude_ft != before[i].altitude_ft;
	}
	CHECK(unchanged > before.size() / 2);
	CHECK(turned > 10 && turned < 100);
	CHECK(climbed > 5 && climbed < 80);
}

void engine_runs() { CHECK(benchmark::engine(100, 120).peak_alerted > 0); }

void crowded_sky_produces_alerts_and_bands() {
	benchmark::Cycles cycles(200);
	for (int second = 0; second < 5; second++) {
		const benchmark::Cycle cycle = cycles.measure_next_second();
		CHECK(cycle.alerting_aircraft <= conflicts::kMaxAlertTargets);
		CHECK(cycle.alerting_aircraft >= 10);
		CHECK(cycle.band_count > 0);
	}
}
}

int main(int argc, char* const* argv) {
	if (argc == 2 && std::string_view(argv[1]) == "--benchmark") {
		benchmark::Cycles cycles(200);
		for (int i = 1; i <= 5; i++) {
			const benchmark::Cycle took = cycles.measure_next_second();
			std::printf(
				"cycle %d  detect %u  altitude %u  track %u  "
				"speed %u  total %u us  alerting %u  bands "
				"%u\n",
				i, took.detect_us, took.altitude_us, took.track_us,
				took.speed_us, took.total_us(), took.alerting_aircraft,
				took.band_count);
		}
		for (const size_t aircraft : {10, 50, 100, 200, 400}) {
			const benchmark::Result result = benchmark::engine(aircraft, 20);
			std::printf(
				"engine %3zu aircraft  step least %u mean %u most %u us  "
				"alerting %u, most %u\n",
				aircraft, result.least_us, result.mean_us, result.most_us,
				result.alerted_last_step, result.peak_alerted);
		}
		return 0;
	}
	return check::run({
		TEST(same_seed),
		TEST(stays_in_the_square),
		TEST(some_fly_something),
		TEST(engine_runs),
		TEST(crowded_sky_produces_alerts_and_bands),
	});
}
