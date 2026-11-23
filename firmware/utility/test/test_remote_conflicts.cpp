#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <stdexcept>
#include <vector>

#include "check.hpp"
#include "device/emulator.hpp"
#include "device/inspect.hpp"
#include "device/maneuvers.hpp"
#include "display/display_mapping.hpp"
#include "remote/conflicts.hpp"
#include "remote/conversation.hpp"

namespace {
using namespace std::chrono_literals;

constexpr double kEncounterStartS =
	device::maneuvers::kMeetingFromLegStartS - 30;

struct Flight {
	device::EmulatedDevice device = device::make_booted_service_device();
	double seconds = kEncounterStartS;

	device::DeviceState next_picture() {
		device::fly(device, seconds++);
		device::Emulator emulator(device);
		remote::Conversation conversation(emulator);
		remote::Conversation::Step step =
			conversation.step({}, {.interval = 1ms});
		if (!step.picture) {
			throw std::runtime_error("The device gave no picture");
		}
		return std::move(*step.picture);
	}
};

// The Sources a Remote Display makes of a picture, which has to outlive them.
display::mapping::ViewInputs sources(const device::DeviceState& picture,
									 bool indicator) {
	display::mapping::ViewInputs sources{
		.adsbee = picture.states.adsbee,
		.traffic = picture.traffic_listing.nearest_targets,
		.barometer = picture.states.barometer,
		.battery = picture.states.battery,
		.charger = picture.states.charger,
		.expander = picture.states.expander,
		.wifi = picture.states.wifi,
		.monotonic_now_us = device::end_of_uptime_second_us(picture.hello)};
	sources.preferences.conflict_indicator = indicator;
	return sources;
}

void await_update(remote::Conflicts& engine, conflicts::StateData& state,
				  unsigned expected_updates, bool expect_bands) {
	if (!engine.loop().wait_idle(2s) || !engine.take_latest(state)) {
		throw std::runtime_error(
			"Conflict update did not publish before the deadline");
	}
	CHECK_EQUAL(expected_updates, state.update_count);
	for (const auto& guide : state.bands) {
		CHECK((!guide.intervals().empty()) == expect_bands);
	}
}

struct Direct {
	conflicts::Monitor rounds;
	conflicts::StateData state;

	bool alert(const device::DeviceState& picture, bool indicator) {
		return rounds.alert(sources(picture, indicator).conflict_input(),
							indicator,
							state) == conflicts::BandSchedule::schedule;
	}
	void bands() {
		for (size_t guide = 0; guide < conflicts::kGuided; guide++) {
			rounds.calculate_bands(guide, state);
		}
	}
};

bool same(const conflicts::Band& a, const conflicts::Band& b) {
	return a.from == b.from && a.to == b.to && a.region == b.region;
}

size_t band_count(const conflicts::StateData& state) {
	size_t count = 0;
	for (const conflicts::Bands& bands : state.bands) {
		count += bands.intervals().size();
	}
	return count;
}

void check_same(const conflicts::StateData& expected,
				const conflicts::StateData& actual) {
	CHECK_EQUAL(expected.update_count, actual.update_count);
	CHECK_EQUAL(expected.band_updates_skipped, actual.band_updates_skipped);
	CHECK_EQUAL(expected.indicator, actual.indicator);
	CHECK_EQUAL(expected.result_count, actual.result_count);
	for (size_t i = 0; i < expected.result_count && i < actual.result_count;
		 i++) {
		const conflicts::AlertingResult &a = expected.results[i],
										&b = actual.results[i];
		CHECK(a.target.address == b.target.address &&
			  a.target.source == b.target.source &&
			  a.conflict.level == b.conflict.level &&
			  a.conflict.within_s == b.conflict.within_s);
	}
	for (size_t guide = 0; guide < conflicts::kGuided; guide++) {
		const conflicts::Bands &a = expected.bands[guide],
							   &b = actual.bands[guide];
		CHECK_EQUAL(a.intervals().size(), b.intervals().size());
		for (size_t i = 0; i < a.intervals().size() && i < b.intervals().size();
			 i++) {
			CHECK(same(a.intervals()[i], b.intervals()[i]));
		}
	}
}

struct MappedDisplay {
	std::array<conflicts::Band, conflicts::kGuided * conflicts::band_capacity>
		room;
	display::ViewState model;

	MappedDisplay(const device::DeviceState& picture,
				  const conflicts::StateData& state, bool indicator) {
		display::mapping::ViewInputs made = sources(picture, indicator);
		conflicts::show(state, made);
		model = display::mapping::build_view_state(made, {}, room);
	}
	size_t bands() const {
		return model.track_bands.size() + model.altitude_bands.size() +
			   model.speed_bands.size();
	}
};

void tasks_run_in_the_order_submitted() {
	std::vector<int> ran;
	remote::Loop loop;
	for (int task = 0; task < 100; task++) {
		loop.submit([&ran, task] { ran.push_back(task); });
	}
	if (!loop.wait_idle(2s)) {
		throw std::runtime_error("FIFO loop did not drain");
	}
	CHECK_EQUAL(100u, ran.size());
	CHECK(std::ranges::is_sorted(ran));
}

void a_loop_that_ends_runs_no_task_that_waits() {
	std::atomic<int> ran = 0;
	std::promise<void> running, open;
	{
		remote::Loop loop;
		loop.submit([&] {
			running.set_value();
			open.get_future().wait();
			ran++;
		});
		loop.submit([&] { ran++; });
		running.get_future().wait();
		loop.request_stop();
		open.set_value();
		CHECK(loop.wait_idle(2s));
	}
	CHECK_EQUAL(1, ran.load());
}

void bands_with_the_switch_as_the_rounds_give_them() {
	Flight flight;
	remote::Conflicts engine;
	Direct direct;
	conflicts::StateData found;
	CHECK(!engine.take_latest(found));
	for (int second = 0; second < 5; second++) {
		const device::DeviceState picture = flight.next_picture();
		CHECK(direct.alert(picture, true));
		direct.bands();
		engine.alert(sources(picture, true).conflict_input(), true);
		await_update(engine, found, static_cast<unsigned>(second) + 1, true);
		CHECK(!engine.take_latest(found));
		check_same(direct.state, found);
		CHECK(found.result_count > 0);
		for (const conflicts::Bands& bands : found.bands) {
			CHECK(!bands.intervals().empty());
		}
		const MappedDisplay shown(picture, found, true);
		CHECK(shown.model.conflict_indicator);
		CHECK_EQUAL(band_count(found), shown.bands());
		CHECK_EQUAL(found.bands[0].intervals().size(),
					shown.model.altitude_bands.size());
		CHECK_EQUAL(found.bands[1].intervals().size(),
					shown.model.track_bands.size());
		CHECK_EQUAL(found.bands[2].intervals().size(),
					shown.model.speed_bands.size());
	}
	CHECK_EQUAL(0u, found.band_updates_skipped);
}

void duplicate_polls_do_not_advance_conflict_rounds() {
	Flight flight;
	remote::Conflicts engine;
	auto picture = flight.next_picture();
	const auto sample = sources(picture, true).conflict_input();
	for (int i = 0; i < 5; ++i) {
		engine.alert(sample, true);
	}
	conflicts::StateData found;
	await_update(engine, found, 1, true);
	picture = flight.next_picture();
	engine.alert(sources(picture, true).conflict_input(), true);
	await_update(engine, found, 2, true);
	CHECK_EQUAL(0u, found.band_updates_skipped);
}

void no_bands_without_the_switch() {
	Flight flight;
	remote::Conflicts engine;
	Direct direct;
	conflicts::StateData found;
	for (int second = 0; second < 3; second++) {
		const device::DeviceState picture = flight.next_picture();
		CHECK(!direct.alert(picture, false));
		engine.alert(sources(picture, false).conflict_input(), false);
		await_update(engine, found, static_cast<unsigned>(second) + 1, false);
		check_same(direct.state, found);
		CHECK(found.result_count > 0);
		CHECK_EQUAL(0u, band_count(found));
		CHECK_EQUAL(0u, MappedDisplay(picture, found, false).bands());
	}
}

void disabling_indicator_clears_previous_round() {
	Flight flight;
	remote::Conflicts engine;
	conflicts::StateData found;
	device::DeviceState picture = flight.next_picture();
	engine.alert(sources(picture, true).conflict_input(), true);
	await_update(engine, found, 1, true);
	CHECK(band_count(found) > 0);
	CHECK(MappedDisplay(picture, found, true).bands() > 0);
	CHECK_EQUAL(0u, MappedDisplay(picture, found, false).bands());
	picture = flight.next_picture();
	engine.alert(sources(picture, false).conflict_input(), false);
	await_update(engine, found, 2, false);
	CHECK_EQUAL(0u, band_count(found));
}

void a_round_that_still_runs_skips_the_next() {
	Flight flight;
	remote::Conflicts engine;
	Direct direct;
	const device::DeviceState first = flight.next_picture();
	const device::DeviceState second = flight.next_picture();
	const device::DeviceState third = flight.next_picture();
	std::promise<void> open;
	engine.loop().submit([&] { open.get_future().wait(); });
	engine.alert(sources(first, true).conflict_input(), true);
	engine.alert(sources(second, true).conflict_input(), true);
	open.set_value();
	CHECK(direct.alert(first, true));
	CHECK(!direct.alert(second, true));
	direct.bands();
	conflicts::StateData found;
	await_update(engine, found, 2, true);
	CHECK_EQUAL(2u, found.update_count);
	CHECK_EQUAL(1u, found.band_updates_skipped);
	CHECK(band_count(found) > 0);
	check_same(direct.state, found);
	engine.alert(sources(third, true).conflict_input(), true);
	CHECK(direct.alert(third, true));
	direct.bands();
	await_update(engine, found, 3, true);
	CHECK_EQUAL(1u, found.band_updates_skipped);
	check_same(direct.state, found);
}

int warning_sectors(const conflicts::Bands& track) {
	int sectors = 0;
	bool previous = false;
	bool starts_at_north = false;
	bool ends_at_north = false;
	float previous_end = -1;
	for (size_t i = 0; i < track.intervals().size(); ++i) {
		const auto& band = track.intervals()[i];
		const bool warning = band.region == conflicts::BandRegion::warning;
		if (warning && (!previous || band.from != previous_end)) {
			++sectors;
		}
		if (i == 0) {
			starts_at_north = warning && band.from == 0;
		}
		previous = warning;
		previous_end = band.to;
		ends_at_north = warning && band.to == 360;
	}
	return sectors - (sectors > 1 && starts_at_north && ends_at_north);
}

void head_on_recovery_at_second_66() {
	Flight flight;
	flight.seconds = 40;
	Direct direct;
	for (int second = 40; second <= 105; ++second) {
		const device::DeviceState picture = flight.next_picture();
		CHECK(direct.alert(picture, true));
		direct.bands();
		const auto& track = direct.state.bands[1];
		const int sectors = warning_sectors(track);
		CHECK(sectors <= 1);
		if (second == 66) {
			CHECK_EQUAL(1, sectors);
			bool collision_course_warns = false, reciprocal_recovers = false;
			for (const auto& band : track.intervals()) {
				const float course = picture.states.adsbee.gnss.track_deg;
				const float reciprocal = std::fmod(course + 180.f, 360.f);
				collision_course_warns |=
					band.region == conflicts::BandRegion::warning &&
					band.from <= course && course <= band.to;
				reciprocal_recovers |=
					band.region == conflicts::BandRegion::recovery &&
					band.from <= reciprocal && reciprocal <= band.to;
			}
			CHECK(collision_course_warns);
			CHECK(reciprocal_recovers);
		}
	}
}

void shutdown_discards_pending_conflict_tasks() {
	Flight flight;
	const device::DeviceState picture = flight.next_picture();
	for (const bool alert_published : {false, true}) {
		std::promise<void> start, blocked, release;
		bool sentinel_ran = false;
		{
			remote::Conflicts engine;
			engine.loop().submit([&] { start.get_future().wait(); });
			if (alert_published) {
				engine.alert(sources(picture, true).conflict_input(), true);
			}
			engine.loop().submit([&] {
				blocked.set_value();
				release.get_future().wait();
			});
			if (!alert_published) {
				engine.alert(sources(picture, true).conflict_input(), true);
			}
			engine.loop().submit([&] { sentinel_ran = true; });
			start.set_value();
			blocked.get_future().wait();

			conflicts::StateData state;
			CHECK(engine.take_latest(state) == alert_published);
			if (alert_published) {
				CHECK(state.result_count > 0);
				CHECK_EQUAL(0u, band_count(state));
			}
			engine.loop().request_stop();
			release.set_value();
		}
		CHECK(!sentinel_ran);
	}
}

void print_encounter_by_second() {
	Flight flight;
	flight.seconds = 0;
	Direct direct;
	for (int second = 0; second < 180; second++) {
		const device::DeviceState picture = flight.next_picture();
		direct.alert(picture, true);
		direct.bands();
		std::printf(
			"second %3d  evaluated %2u  bands %u %u %u\n", second,
			static_cast<unsigned>(direct.state.result_count),
			static_cast<unsigned>(direct.state.bands[0].intervals().size()),
			static_cast<unsigned>(direct.state.bands[1].intervals().size()),
			static_cast<unsigned>(direct.state.bands[2].intervals().size()));
	}
}
}

int main(int argc, char**) {
	if (argc > 1) {
		print_encounter_by_second();
		return 0;
	}
	return check::run({TEST(tasks_run_in_the_order_submitted),
					   TEST(a_loop_that_ends_runs_no_task_that_waits),
					   TEST(bands_with_the_switch_as_the_rounds_give_them),
					   TEST(duplicate_polls_do_not_advance_conflict_rounds),
					   TEST(no_bands_without_the_switch),
					   TEST(disabling_indicator_clears_previous_round),
					   TEST(a_round_that_still_runs_skips_the_next),
					   TEST(head_on_recovery_at_second_66),
					   TEST(shutdown_discards_pending_conflict_tasks)});
}
