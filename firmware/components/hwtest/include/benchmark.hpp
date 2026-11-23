#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace benchmark {
inline constexpr uint32_t kSeed = 20261121;
inline constexpr double kLatitude = 49.47, kLongitude = 8.51;
struct Result {
	uint32_t least_us = 0, mean_us = 0, most_us = 0;
	uint32_t alerted_last_step = 0, peak_alerted = 0;
};

// The warm-up step introduces the traffic and is excluded from the timings.
// rest is called after every step, outside the timing: a device lets its
// other tasks run there, or its watchdog ends the run.
Result engine(size_t aircraft, uint32_t steps, void (*rest)() = nullptr);

struct Cache {
	uint32_t instruction_accesses = 0, instruction_misses = 0;
	uint32_t data_accesses = 0, data_flash_misses = 0, data_ram_misses = 0;
};

struct Cycle {
	uint32_t detect_us = 0;
	uint32_t altitude_us = 0, track_us = 0, speed_us = 0;
	Cache detect_cache, altitude_cache, track_cache, speed_cache;

	uint32_t alerting_aircraft = 0, band_count = 0;
	uint32_t total_us() const {
		return detect_us + altitude_us + track_us + speed_us;
	}
};

// The scenario with this many aircraft and an engine that has met them. As
// many as the engine computes its Bands against head for the own ship.
class Cycles {
   public:
	explicit Cycles(size_t aircraft, Cache (*probe)() = nullptr);
	~Cycles();
	Cycles(const Cycles&) = delete;
	Cycles& operator=(const Cycles&) = delete;
	Cycles(Cycles&&) = delete;
	Cycles& operator=(Cycles&&) = delete;
	Cycle measure_next_second();

   private:
	struct Sky;
	std::unique_ptr<Sky> sky_;
};
}
