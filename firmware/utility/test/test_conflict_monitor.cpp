#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <span>
#include <string_view>
#include <vector>

#include "benchmark.hpp"
#include "check.hpp"
#include "daidalus/monitor.hpp"
#include "display/display_mapping.hpp"
#include "navigation/units.h"
#include "scenario.hpp"
#include "usb/usb_layout.hpp"

namespace {
constexpr size_t kAircraft = 200;
constexpr int64_t kStart = 100'000'000;

struct Device {
	scenario::Traffic sky{kAircraft, benchmark::kSeed, benchmark::kLatitude,
						  benchmark::kLongitude, conflicts::kMaxBandTargets};
	std::vector<traffic::Target> slots =
		std::vector<traffic::Target>(usb::layout::kTargets);
	std::vector<traffic::Position> history = std::vector<traffic::Position>(
		usb::layout::kTargets * traffic::kHistorySeconds);
	traffic::Table table{slots.data(), slots.size(), history.data()};
	std::vector<traffic::Tracked> listed =
		std::vector<traffic::Tracked>(traffic::kListed);
	adsbee::StateData adsbee;
	barometer::StateData barometer;
	battery::StateData battery;
	charger::StateData charger;
	expander::StateData expander;
	wifi::StateData wifi;
	conflicts::Monitor monitor;
	conflicts::StateData state;
	int64_t now_us = kStart;
	bool fix = true;

	bool bands_due_after_next_second(bool indicator) {
		sky.step(1);
		now_us += 1'000'000;
		const scenario::Aircraft& own = sky.own();
		adsbee.gnss.fix = fix ? adsbee::Fix::fix_3d : adsbee::Fix::none;
		adsbee.gnss.latitude = static_cast<float>(own.latitude_deg);
		adsbee.gnss.longitude = static_cast<float>(own.longitude_deg);
		adsbee.gnss.altitude_ft =
			static_cast<int32_t>(std::lround(own.altitude_ft));
		adsbee.gnss.speed_kt = own.groundspeed_kt;
		adsbee.gnss.track_deg = own.true_track_deg;
		adsbee.last_gnss_report_us = now_us;
		barometer.status = Status::ok;
		barometer.altitude_m = own.altitude_ft / units::feet_per_metre;
		barometer.updated_us = now_us;
		for (size_t i = 0; i < sky.traffic().size(); i++) {
			const scenario::Aircraft& aircraft = sky.traffic()[i];
			traffic::Target target;
			target.address = static_cast<uint32_t>(i + 1);
			target.position_valid = target.baro_alt_valid = true;
			target.direction_valid = target.speed_valid = true;
			target.baro_rate_valid = true;
			target.latitude = static_cast<float>(aircraft.latitude_deg);
			target.longitude = static_cast<float>(aircraft.longitude_deg);
			target.baro_alt_ft =
				static_cast<int32_t>(std::lround(aircraft.altitude_ft));
			target.track_deg = aircraft.true_track_deg;
			target.speed_kt =
				static_cast<int16_t>(std::lround(aircraft.groundspeed_kt));
			target.baro_rate_fpm =
				static_cast<int16_t>(std::lround(aircraft.vertical_fpm));
			table.upsert(target, now_us);
		}
		table.sample(now_us);
		traffic::Reference reference{adsbee.gnss.latitude,
									 adsbee.gnss.longitude};
		const size_t count = table.nearest(fix ? &reference : nullptr,
										   listed.data(), listed.size());
		const conflicts::ConflictInput sources{
			.adsbee = adsbee,
			.traffic = std::span(listed).first(count),
			.barometer = barometer,

			.now_us = now_us};
		return monitor.alert(sources, indicator, state) ==
			   conflicts::BandSchedule::schedule;
	}

	void bands() {
		for (size_t guide = 0; guide < conflicts::kGuided; guide++) {
			monitor.calculate_bands(guide, state);
		}
	}

	display::ViewState model(bool indicator, std::span<conflicts::Band> bands) {
		display::mapping::ViewInputs sources{.adsbee = adsbee,
											 .traffic = {},
											 .barometer = barometer,
											 .battery = battery,
											 .charger = charger,
											 .expander = expander,
											 .wifi = wifi,
											 .monotonic_now_us = now_us};
		sources.preferences.conflict_indicator = indicator;
		conflicts::show(state, sources);
		return display::mapping::build_view_state(sources, {}, bands);
	}

	uint32_t alerting() const {
		return static_cast<uint32_t>(std::count_if(
			state.results.begin(), state.results.begin() + state.result_count,
			[](const conflicts::AlertingResult& result) {
				return result.conflict.level != conflicts::Level::other;
			}));
	}

	size_t band_count() const {
		size_t count = 0;
		for (const conflicts::Bands& bands : state.bands) {
			count += bands.intervals().size();
		}
		return count;
	}
};

void alerts_without_the_switch() {
	Device device;
	for (int second = 0; second < 3; second++) {
		CHECK(!device.bands_due_after_next_second(false));
	}
	CHECK_EQUAL(3u, device.state.update_count);
	CHECK(!device.state.indicator);
	CHECK(device.state.result_count > 0);
	CHECK(device.alerting() >= conflicts::kMaxBandTargets);
	CHECK_EQUAL(0u, device.band_count());
	CHECK_EQUAL(0u, device.state.band_updates_skipped);
}

void bands_follow_with_the_switch() {
	Device device;
	for (int second = 0; second < 3; second++) {
		CHECK(device.bands_due_after_next_second(true));
		device.bands();
		for (const conflicts::Bands& bands : device.state.bands) {
			CHECK(!bands.intervals().empty());
		}
	}
	CHECK(device.state.indicator);
	CHECK_EQUAL(0u, device.state.band_updates_skipped);
}

void bands_are_relative_to_own_ship() {
	Device device;
	CHECK(device.bands_due_after_next_second(true));
	device.bands();
	std::array<conflicts::Band, conflicts::kGuided * conflicts::band_capacity>
		room;
	display::ViewState shown = device.model(true, room);
	const scenario::Aircraft& own = device.sky.own();
	const struct {
		std::span<const conflicts::Band> shown;
		conflicts::Guide guide;
		float own;
	} guides[] = {
		{shown.altitude_bands, conflicts::Guide::altitude, own.altitude_ft},
		{shown.track_bands, conflicts::Guide::track, own.true_track_deg},
		{shown.speed_bands, conflicts::Guide::ground_speed, own.groundspeed_kt},
	};
	for (size_t guide = 0; guide < conflicts::kGuided; guide++) {
		const conflicts::Bands& found = device.state.bands[guide];
		CHECK(conflicts::kGuides[guide].guide == guides[guide].guide);
		CHECK_EQUAL(found.intervals().size(), guides[guide].shown.size());
		for (size_t i = 0; i < guides[guide].shown.size(); i++) {
			const conflicts::Band& band = guides[guide].shown[i];
			CHECK(std::abs(band.from + guides[guide].own -
						   found.intervals()[i].from) < 0.5f);
			CHECK(std::abs(band.to + guides[guide].own -
						   found.intervals()[i].to) < 0.5f);
			CHECK(band.region == found.intervals()[i].region);
		}
	}
	shown = device.model(false, room);
	CHECK_EQUAL(0u, shown.track_bands.size());
	CHECK_EQUAL(0u, shown.altitude_bands.size());
	CHECK_EQUAL(0u, shown.speed_bands.size());
}

void a_round_that_still_runs_skips_the_next() {
	Device device;
	CHECK(device.bands_due_after_next_second(true));
	device.monitor.calculate_bands(0, device.state);
	CHECK(!device.bands_due_after_next_second(true));
	CHECK_EQUAL(1u, device.state.band_updates_skipped);
	device.monitor.calculate_bands(1, device.state);
	CHECK(!device.bands_due_after_next_second(true));
	CHECK_EQUAL(2u, device.state.band_updates_skipped);
	device.monitor.calculate_bands(2, device.state);
	CHECK(device.bands_due_after_next_second(true));
	CHECK_EQUAL(2u, device.state.band_updates_skipped);
	CHECK_EQUAL(4u, device.state.update_count);
}

void band_results_belong_to_their_round() {
	Device device;
	CHECK(device.bands_due_after_next_second(true));
	device.bands();
	for (const auto& bands : device.state.bands) {
		CHECK(!bands.intervals().empty());
	}
	CHECK(device.bands_due_after_next_second(true));
	device.monitor.calculate_bands(0, device.state);
	CHECK(!device.state.bands[0].intervals().empty());
	CHECK_EQUAL(0u, device.state.bands[1].intervals().size());
	CHECK_EQUAL(0u, device.state.bands[2].intervals().size());
	std::array<conflicts::Band, conflicts::band_capacity> room;
	const auto shown = device.model(true, room);
	CHECK(shown.track_bands.empty());
	CHECK(shown.speed_bands.empty());
}

void a_band_that_never_runs_holds_nothing_up() {
	Device device;
	CHECK(device.bands_due_after_next_second(true));
	device.monitor.calculate_bands(0, device.state);
	device.monitor.release_pending_band_task();
	device.monitor.release_pending_band_task();
	CHECK(device.bands_due_after_next_second(true));
	CHECK_EQUAL(0u, device.state.band_updates_skipped);
}

void the_switch_goes_off_within_a_round() {
	Device device;
	CHECK(device.bands_due_after_next_second(true));
	device.bands();
	CHECK(device.band_count() > 0);
	CHECK(device.bands_due_after_next_second(true));
	device.monitor.calculate_bands(0, device.state);
	CHECK(!device.bands_due_after_next_second(false));
	CHECK_EQUAL(0u, device.band_count());
	device.monitor.calculate_bands(1, device.state);
	device.monitor.calculate_bands(2, device.state);
	CHECK_EQUAL(0u, device.band_count());
	CHECK_EQUAL(0u, device.state.band_updates_skipped);
	CHECK(device.bands_due_after_next_second(true));
}

void no_bands_without_a_fix() {
	Device device;
	CHECK(device.bands_due_after_next_second(true));
	device.bands();
	device.fix = false;
	CHECK(!device.bands_due_after_next_second(true));
	CHECK_EQUAL(0u, device.state.result_count);
	CHECK_EQUAL(0u, device.band_count());
	CHECK_EQUAL(0u, device.state.band_updates_skipped);
	device.fix = true;
	CHECK(device.bands_due_after_next_second(true));
}

// The benchmark hands the engine every aircraft as the scenario flies it.
// The device hands it the nearest kListed as Targets report them: whole
// knots, feet and feet per minute, places in float.
void agrees_with_the_benchmark(bool print) {
	benchmark::Cycles cycles(kAircraft);
	Device device;
	device.bands_due_after_next_second(true);
	device.bands();
	for (int second = 1; second <= 30; second++) {
		const benchmark::Cycle cycle = cycles.measure_next_second();
		const bool follows = device.bands_due_after_next_second(true);
		device.bands();
		if (print) {
			std::printf(
				"second %2d  benchmark alerting %2u bands %2u  device "
				"evaluated "
				"%2u alerting %2u bands %2u (%u %u %u)\n",
				second, static_cast<unsigned>(cycle.alerting_aircraft),
				static_cast<unsigned>(cycle.band_count),
				static_cast<unsigned>(device.state.result_count),
				static_cast<unsigned>(device.alerting()),
				static_cast<unsigned>(device.band_count()),
				static_cast<unsigned>(device.state.bands[0].intervals().size()),
				static_cast<unsigned>(device.state.bands[1].intervals().size()),
				static_cast<unsigned>(
					device.state.bands[2].intervals().size()));
		}
		CHECK(follows);
		CHECK_EQUAL(cycle.alerting_aircraft, device.alerting());
		CHECK_EQUAL(cycle.band_count, device.band_count());
	}
}

void agrees_with_the_benchmark() { agrees_with_the_benchmark(false); }
}

int main(int argc, char* const* argv) {
	if (argc == 2 && std::string_view(argv[1]) == "--print") {
		agrees_with_the_benchmark(true);
		return check::failures ? 1 : 0;
	}
	if (argc != 1) {
		(void)std::fprintf(stderr, "usage: %s [--print]\n", argv[0]);
		return 2;
	}
	return check::run(
		{TEST(alerts_without_the_switch), TEST(bands_follow_with_the_switch),
		 TEST(bands_are_relative_to_own_ship),
		 TEST(a_round_that_still_runs_skips_the_next),
		 TEST(band_results_belong_to_their_round),
		 TEST(a_band_that_never_runs_holds_nothing_up),
		 TEST(the_switch_goes_off_within_a_round), TEST(no_bands_without_a_fix),
		 TEST(agrees_with_the_benchmark)});
}
