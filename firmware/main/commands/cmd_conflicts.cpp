#include <cstdio>

#include "console.h"
#include "esp_console.h"

#if AIRGREP_HAS_DISPLAY
#include "daidalus/scheduler.hpp"
#include "esp_timer.h"
#include "sdkconfig.h"

namespace {
constexpr const char* kLevels[] = {"none", "proximate", "advisory", "alert"};
constexpr const char* kRegions[] = {"proximate", "advisory", "alert",
									"recovery"};

int command(int, char**) {
	static conflicts::State state;
	conflicts::State::update(
		[](const conflicts::State& published) { state = published; });
	if (!state.alert_finished_us) {
		std::printf("No conflict update yet\n");
		return 0;
	}
	const int64_t now = esp_timer_get_time();
	std::printf("%-20s %s\n", "Conflict Indicator",
				state.indicator ? "on" : "off");
	std::printf("%-20s %u targets, %.1f s ago\n", "Targets evaluated",
				(unsigned)state.result_count,
				static_cast<double>(now - state.alert_finished_us) / 1e6);
	for (size_t i = 0; i < state.result_count; i++) {
		const conflicts::AlertingResult& result = state.results[i];
		if (result.conflict.level != conflicts::Level::other) {
			std::printf("  %06X %-9s in %u s\n",
						(unsigned)result.target.address,
						kLevels[static_cast<size_t>(result.conflict.level)],
						(unsigned)result.conflict.within_s);
		}
	}
	for (size_t guide = 0; guide < conflicts::kGuided; guide++) {
		const auto intervals = state.bands[guide].intervals();
		std::printf("%-20s %zu", conflicts::kGuides[guide].label,
					intervals.size());
		for (const auto& band : intervals) {
			std::printf("  %.0f to %.0f %s %s", band.from, band.to,
						conflicts::kGuides[guide].units,
						kRegions[static_cast<size_t>(band.region)]);
		}
		std::printf("\n");
	}
	std::printf("%-20s alerting %.1f (sources %.1f)", "Tasks, ms",
				state.alert_duration_us / 1e3, state.input_read_us / 1e3);
	for (size_t guide = 0; guide < conflicts::kGuided; guide++) {
		const conflicts::Bands& bands = state.bands[guide];

		if (bands.calculation_us) {
			std::printf("  %s %.1f", conflicts::kGuides[guide].label,
						*bands.calculation_us / 1e3);
		} else {
			std::printf("  %s -", conflicts::kGuides[guide].label);
		}
	}
	std::printf("\n%-20s %u, %u of them skipped their bands\n", "Updates",
				(unsigned)state.update_count,
				(unsigned)state.band_updates_skipped);
	std::printf("%-20s %u\n", "Engine failures",
				(unsigned)state.engine_failures);
	std::printf("%-20s %u of %u B\n", "Loop stack left",
				(unsigned)state.minimum_free_stack_bytes,
				(unsigned)CONFIG_AIRGREP_LOOP_STACK);
	return 0;
}
}

extern "C" esp_err_t console_register_conflicts(void) {
	const esp_console_cmd_t entry = {
		.command = "conflicts",
		.help =
			"Show what the conflict engine found, its bands and how long "
			"its tasks took.",
		.hint = nullptr,
		.func = command,
		.argtable = nullptr,
		.func_w_context = nullptr,
		.context = nullptr,
	};
	return console_register(&entry);
}
#else
extern "C" esp_err_t console_register_conflicts(void) { return ESP_OK; }
#endif
