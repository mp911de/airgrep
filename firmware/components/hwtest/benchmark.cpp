#include "benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <vector>

#include "conflicts/conflicts.hpp"
#include "scenario.hpp"

namespace benchmark {
namespace {

conflicts::Flight flight(const scenario::Aircraft& aircraft, uint64_t id,
						 double time_s) {
	return {.id = id,
			.latitude_deg = aircraft.latitude_deg,
			.longitude_deg = aircraft.longitude_deg,
			.altitude_ft = aircraft.altitude_ft,
			.true_track_deg = aircraft.true_track_deg,
			.groundspeed_kt = aircraft.groundspeed_kt,
			.vertical_fpm = aircraft.vertical_fpm,
			.time_s = time_s};
}
}

Result engine(size_t aircraft, uint32_t steps, void (*rest)()) {
	scenario::Traffic sky(aircraft, kSeed, kLatitude, kLongitude);
	conflicts::ConflictEngine detector;
	std::vector<conflicts::Flight> flights(aircraft);
	std::vector<conflicts::Conflict> found(aircraft);
	Result result{.least_us = UINT32_MAX};
	uint64_t total_us = 0;
	for (uint32_t step = 0; step <= steps; step++) {
		sky.step(1);
		for (size_t i = 0; i < aircraft; i++) {
			flights[i] = flight(sky.traffic()[i], i + 1, step);
		}
		const auto began = std::chrono::steady_clock::now();
		detector.detect(flight(sky.own(), 0, step), flights, found);
		const auto took = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now() - began);
		result.alerted_last_step = static_cast<uint32_t>(std::ranges::count_if(
			found, [](const conflicts::Conflict& conflict) {
				return conflict.level != conflicts::Level::other;
			}));
		result.peak_alerted =
			std::max(result.peak_alerted, result.alerted_last_step);
		if (rest) {
			rest();
		}
		if (step == 0) {
			continue;
		}
		const uint32_t us = static_cast<uint32_t>(took.count());
		result.least_us = std::min(result.least_us, us);
		result.most_us = std::max(result.most_us, us);
		total_us += us;
	}
	result.mean_us = steps ? static_cast<uint32_t>(total_us / steps) : 0;
	result.least_us = steps ? result.least_us : 0;
	return result;
}

struct Cycles::Sky {
	explicit Sky(size_t aircraft)
		: traffic(aircraft, kSeed, kLatitude, kLongitude,
				  conflicts::kMaxBandTargets),
		  flights(aircraft),
		  found(aircraft) {}
	scenario::Traffic traffic;
	conflicts::ConflictEngine engine;
	std::vector<conflicts::Flight> flights;
	std::vector<conflicts::Conflict> found;
	uint32_t step = 0;
	Cache (*probe)() = nullptr;
};

Cycles::Cycles(size_t aircraft, Cache (*probe)())
	: sky_(std::make_unique<Sky>(aircraft)) {
	sky_->probe = probe;
	measure_next_second();
}
Cycles::~Cycles() = default;

Cycle Cycles::measure_next_second() {
	using Clock = std::chrono::steady_clock;
	const auto us = [](Clock::time_point from, Clock::time_point to) {
		return static_cast<uint32_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(to - from)
				.count());
	};
	Sky& sky = *sky_;
	const auto cache = [&] { return sky.probe ? sky.probe() : Cache{}; };
	const auto since = [](Cache from, Cache to) {
		return Cache{to.instruction_accesses - from.instruction_accesses,
					 to.instruction_misses - from.instruction_misses,
					 to.data_accesses - from.data_accesses,
					 to.data_flash_misses - from.data_flash_misses,
					 to.data_ram_misses - from.data_ram_misses};
	};
	sky.traffic.step(1);
	sky.step++;
	for (size_t i = 0; i < sky.flights.size(); i++) {
		sky.flights[i] = flight(sky.traffic.traffic()[i], i + 1, sky.step);
	}
	const conflicts::Flight own = flight(sky.traffic.own(), 0, sky.step);

	conflicts::Band stretches[32];
	Cycle cycle;

	const Cache before = cache();
	const Clock::time_point began = Clock::now();
	sky.engine.detect(own, sky.flights, sky.found);
	const Clock::time_point alerted = Clock::now();
	const Cache after_alerting = cache();
	size_t bands = sky.engine.bands(conflicts::Guide::altitude, stretches);
	const Clock::time_point altitude = Clock::now();
	const Cache after_altitude = cache();
	bands += sky.engine.bands(conflicts::Guide::track, stretches);
	const Clock::time_point track = Clock::now();
	const Cache after_track = cache();
	bands += sky.engine.bands(conflicts::Guide::ground_speed, stretches);
	const Clock::time_point done = Clock::now();
	cycle.detect_cache = since(before, after_alerting);
	cycle.altitude_cache = since(after_alerting, after_altitude);
	cycle.track_cache = since(after_altitude, after_track);
	cycle.speed_cache = since(after_track, cache());

	cycle.detect_us = us(began, alerted);
	cycle.altitude_us = us(alerted, altitude);
	cycle.track_us = us(altitude, track);
	cycle.speed_us = us(track, done);
	cycle.alerting_aircraft = static_cast<uint32_t>(std::ranges::count_if(
		sky.found, [](const conflicts::Conflict& conflict) {
			return conflict.level != conflicts::Level::other;
		}));
	cycle.band_count = static_cast<uint32_t>(bands);
	return cycle;
}
}
