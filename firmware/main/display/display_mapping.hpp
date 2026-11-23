#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "battery/max17048_i2c.hpp"
#include "charger/bq25798_i2c.hpp"
#include "daidalus/detection.hpp"
#include "display/view_state.hpp"
#include "i2c_expander/tca9534_i2c.hpp"
#include "system/power_mode.h"
#include "wifi/wifi_protocol.hpp"

namespace display::mapping {
struct Preferences {
	const char* language = "en";
	bool dark_theme = false;
	bool track_history = true;
	bool simple_symbols = false;
	bool conflict_indicator = false;
	uint8_t radar_range_nm = 4;
	uint16_t altitude_filter_ft = 3000;
	uint32_t own_address = display::NO_ADDRESS;
	const char* own_callsign = "";
	std::span<const conflicts::Id> muted;
};

struct Clock {
	uint8_t hour = 0, minute = 0;
	const char* time_zone = "";
};

// As ViewState states them.
struct Zones {
	bool automatic = true;
	const char* in_use = "";
	const char* detected = "";
	std::span<const display::Zone> listed;
};

struct ViewInputs {
	std::span<const conflicts::AlertingResult> conflicts;
	// The Bands the conflict engine found for these ViewInputs, as it says
	// them: degrees true, feet in the datum it was given the own altitude in,
	// knots.
	std::span<const conflicts::Band> track_bands = {}, altitude_bands = {},
									 speed_bands = {};
	const adsbee::StateData& adsbee;
	std::span<const traffic::Tracked> traffic;

	std::span<const conflicts::Id> previous_inside;
	const barometer::StateData& barometer;
	const battery::StateData& battery;
	const charger::StateData& charger;
	const expander::StateData& expander;
	const wifi::StateData& wifi;

	int64_t monotonic_now_us = 0;
	uint16_t health = 0;
	std::optional<uint8_t> busiest_core_percent;
	power_mode_t mode = POWER_MODE_ON;
	bool starting = false;
	const char* device_name = "";
	const char* version = "";
	const char* device_id = "";
	uint32_t greeting = 0;
	Clock clock;
	Zones zones;
	Preferences preferences;
	std::span<const display::License> licenses;

	conflicts::ConflictInput conflict_input() const {
		return {adsbee, traffic, barometer, monotonic_now_us,
				preferences.own_address};
	}
};

inline constexpr int16_t fair_cno_dbhz = 30, good_cno_dbhz = 40;

inline constexpr int16_t minimum_climb_or_descent_fpm = 100;
display::Kind kind(const traffic::Target& target);
display::Battery battery(const battery::StateData& gauge,
						 const charger::StateData& charger);
display::Antenna antenna(const adsbee::Gnss& gnss);
uint8_t signal_level(const adsbee::Gnss& gnss);

inline constexpr float kRingBandNm = 0.2f;

bool inside_ring(float distance_nm, uint8_t range_nm, bool was_inside);

// Unknown altitude passes the altitude filter.
bool passes_altitude_filter(std::optional<int32_t> above_ft,
							const Preferences& preferences,
							conflicts::Level level = conflicts::Level::other);

// For a target the conflict engine cannot judge, as its speed, its course or
// its vertical rate is not known: true when the own ship could come too
// close to it within the horizon, were it to stand still. Too close is as
// DO-365B says, an unknown altitude counts as the own one.
bool within_reach(float distance_nm, float own_kt,
				  std::optional<int32_t> above_ft);

enum class ExclusionReason : uint8_t {
	no_report,
	own_ship,
	no_position,
	altitude_filter,
	list_full
};

struct RelativePosition {
	float distance_nm, true_bearing_deg;
};

struct TargetDetails {
	std::optional<ExclusionReason> exclusion_reason{};
	RadarPosition radar_position = RadarPosition::unknown;
	bool show_beyond_ring_marker = false;

	std::optional<RelativePosition> position{};

	std::optional<int32_t> above_ft{};
	std::optional<conflicts::Conflict> conflict{};
};

TargetDetails target_details(const traffic::Tracked& tracked,
							 const ViewInputs& sources);

size_t sort_and_count_inside(std::span<display::Target> targets,
							 bool has_own_fix);

size_t inside_ids(const display::ViewState& model,
				  std::span<conflicts::Id> ids);

// The returned model borrows its sources and output buffers.
display::ViewState build_view_state(const ViewInputs& sources,
									std::span<display::Target> targets,
									std::span<conflicts::Band> bands = {},
									std::span<TargetDetails> details = {});
}
