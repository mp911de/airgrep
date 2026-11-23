#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "detection.hpp"
#include "traffic/traffic_table.hpp"

namespace display::mapping {
struct ViewInputs;
}

namespace conflicts {
struct GuideInfo {
	conflicts::Guide guide;
	const char* label;
	const char* units;
	const char* task;
};
inline constexpr GuideInfo kGuides[] = {
	{conflicts::Guide::altitude, "Altitude", "ft", "conflict altitude"},
	{conflicts::Guide::track, "Track", "deg", "conflict track"},
	{conflicts::Guide::ground_speed, "Speed", "kt", "conflict speed"}};
inline constexpr size_t kGuided = std::size(kGuides);

inline constexpr size_t band_capacity = 32;

class Bands {
   public:
	std::optional<uint32_t> calculation_us;

	std::span<const conflicts::Band> intervals() const {
		return std::span(stretches).first(count);
	}

   private:
	friend class Monitor;
	std::array<conflicts::Band, band_capacity> stretches{};
	uint8_t count = 0;
};

struct StateData {
	std::array<conflicts::AlertingResult, traffic::kListed> results{};
	uint8_t result_count = 0;
	bool indicator = false;

	uint32_t alert_duration_us = 0, input_read_us = 0;
	int64_t alert_finished_us = 0;

	std::array<Bands, kGuided> bands{};

	uint32_t update_count = 0, band_updates_skipped = 0;
	// Steps the engine failed and began anew from: they said no conflict.
	uint32_t engine_failures = 0;

	uint32_t minimum_free_stack_bytes = 0;
};

// The display borrows the band storage in state.
void show(const StateData& state, display::mapping::ViewInputs& sources);

enum class BandSchedule { skip, schedule };

class Monitor {
   public:
	BandSchedule alert(const conflicts::ConflictInput& sources, bool indicator,
					   StateData& state);
	bool calculate_bands(size_t guide, StateData& state);

	void release_pending_band_task();

   private:
	conflicts::Detector conflicts_;
	uint8_t pending_band_tasks_ = 0;
	bool banding_ = false;
};
}
