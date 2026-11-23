#include "monitor.hpp"

#include "display/display_mapping.hpp"

namespace conflicts {
void show(const StateData& state, display::mapping::ViewInputs& sources) {
	for (size_t guide = 0; guide < kGuided; guide++) {
		const auto found = state.bands[guide].intervals();
		switch (kGuides[guide].guide) {
			case conflicts::Guide::track:
				sources.track_bands = found;
				break;
			case conflicts::Guide::altitude:
				sources.altitude_bands = found;
				break;
			case conflicts::Guide::ground_speed:
				sources.speed_bands = found;
				break;
			case conflicts::Guide::vertical_speed:
				break;
		}
	}
}

BandSchedule Monitor::alert(const conflicts::ConflictInput& sources,
							bool indicator, StateData& state) {
	const auto result_count = conflicts_.detect(sources, state.results);
	state.result_count = static_cast<uint8_t>(result_count.value_or(0));
	state.update_count++;
	state.engine_failures = conflicts_.engine_failures();
	state.indicator = indicator;

	banding_ = indicator && result_count.has_value();
	for (Bands& bands : state.bands) {
		bands.count = 0;
		bands.calculation_us.reset();
	}
	if (!banding_) {
		return BandSchedule::skip;
	}
	if (pending_band_tasks_) {
		state.band_updates_skipped++;
		return BandSchedule::skip;
	}
	pending_band_tasks_ = kGuided;
	return BandSchedule::schedule;
}

void Monitor::release_pending_band_task() {
	if (pending_band_tasks_) {
		pending_band_tasks_--;
	}
}

bool Monitor::calculate_bands(size_t guide, StateData& state) {
	release_pending_band_task();
	if (!banding_) {
		return false;
	}
	Bands& bands = state.bands[guide];
	bands.count = static_cast<uint8_t>(
		conflicts_.bands(kGuides[guide].guide, bands.stretches));
	state.engine_failures = conflicts_.engine_failures();
	return true;
}
}
