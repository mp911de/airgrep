#pragma once

#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <span>

#include "conflicts/conflicts.hpp"

namespace display {
enum class Kind : uint8_t {
	unknown,
	light,
	airliner,
	glider,
	helicopter,
	balloon
};

enum class Source : uint8_t { adsb, adsr, tisb, flarm, other, adsb_other };

struct TargetId {
	uint32_t address;
	Source source;
};

constexpr Source mute_source(Source source) {
	return source == Source::adsb_other ? Source::adsb : source;
}

// A place relative to the own ship as it is now: to the right of its track
// and ahead of it, in nautical miles. The radar is track up.
struct Offset {
	float right_nm, ahead_nm;
};

inline constexpr size_t kTailSamples = 60;

inline constexpr uint8_t RANGES_NM[] = {2, 4, 8, 16, 32};

constexpr std::optional<uint8_t> radar_range_index(uint8_t nm) {
	for (size_t i = 0; i < std::size(RANGES_NM); ++i) {
		if (RANGES_NM[i] == nm) {
			return static_cast<uint8_t>(i);
		}
	}
	return std::nullopt;
}

inline constexpr uint16_t ALTITUDE_FILTER_MAX_FT = 9999;

enum class VerticalTrend : int8_t { descending = -1, level = 0, climbing = 1 };

enum class RadarPosition : uint8_t { unknown, inside_ring, beyond_ring };

// One target as the display shows it, relative to the own ship. Bearing and
// track are relative to the own track, the radar is track up.
struct Target {
	char callsign[9]{};  // empty when unknown
	uint32_t address{};
	Source source{};
	Kind kind{};
	conflicts::Level level{};
	bool muted{};
	bool track_valid{};
	std::optional<int16_t> relative_altitude_ft;
	VerticalTrend vertical_trend{};
	uint16_t bearing_deg{};
	uint16_t track_deg{};
	float distance_nm{};
	bool own_ship_candidate{};
	// It moves and shows where it goes if it keeps its speed and its turn. To
	// the right is a positive turn.
	bool predicted = false;
	uint16_t speed_kt = 0;
	float turn_dps = 0;
	// Where it was, the newest second first. Empty for a target that shows no
	// track.
	uint8_t tail_count = 0;
	Offset tail[kTailSamples] = {};
	// A target at the ring boundary stays on its previous side.
	RadarPosition radar_position = RadarPosition::unknown;
	bool show_beyond_ring_marker = false;
	// Seconds until the conflict its level tells of begins, 0 once it has,
	// NO_CONFLICT without one.
	uint16_t conflict_in_s = conflicts::NO_CONFLICT;
};

enum class Health : uint8_t { ok, caution, failure };

enum class Cause : uint8_t {
	i2c,
	adsbee,
	gps_receiver,
	antenna_short,
	charger_overvoltage,
	charger_overheated,
	battery_overheated,
	antenna_open,
	backup_battery,
	barometer,
	battery_gauge,
	usb_c,
	battery_low,
	battery_cold,
	memory,
	cpu,
};
constexpr int CAUSES = 16;

constexpr uint16_t bit(Cause cause) {
	return static_cast<uint16_t>(1u << static_cast<uint8_t>(cause));
}

constexpr uint16_t bits(std::initializer_list<Cause> causes) {
	unsigned set = 0;
	for (const Cause cause : causes) {
		set |= bit(cause);
	}
	return static_cast<uint16_t>(set);
}

constexpr uint16_t FAILURES =
	bits({Cause::i2c, Cause::adsbee, Cause::gps_receiver, Cause::antenna_short,
		  Cause::charger_overvoltage, Cause::charger_overheated,
		  Cause::battery_overheated});

constexpr Health level(uint16_t causes) {
	return causes & FAILURES ? Health::failure
		   : causes          ? Health::caution
							 : Health::ok;
}

inline constexpr uint8_t kMaxSignalLevel = 3;

enum class Fix : uint8_t { none, fix_2d, fix_3d };

enum class Mode : uint8_t { starting, on, charger };

enum class Battery : uint8_t { absent, discharging, charging, full };

constexpr uint16_t NO_ESTIMATE = 0xFFFF;

// The GPS antenna. Passive is one without a supply, it can only be OK.
enum class Antenna : uint8_t {
	unknown,
	active,
	passive,
	shorted,
	disconnected
};

// A component in the shipped image. The strings live as long as the program.
struct License {
	const char* component;
	const char* spdx_id;
	const char* text;
};

struct Zone {
	const char* iana_name;
	// From UTC at this moment, daylight saving time included.
	int16_t offset_min;
};

constexpr uint32_t NO_ADDRESS = 0xFFFFFFFF;

struct RadarGenerations {
	uint8_t traffic{}, gnss{}, daidalus{};
	bool resync{};

	// Keep a wrap visible until this producer has delivered a snapshot to its
	// display.
	void bump(uint8_t& counter) noexcept {
		if (++counter == 0) {
			resync = true;
		}
	}
	void acknowledge() noexcept { resync = false; }
	bool operator==(const RadarGenerations&) const = default;
};
static_assert(sizeof(RadarGenerations) == 4);

struct ViewState {
	// Producers without revisions request a complete radar update.
	std::optional<RadarGenerations> radar_generations;
	const char* language = "en";
	Mode mode = Mode::on;
	const char* device_name = "";
	const char* version = "";
	uint32_t greeting_seed = 0;
	const char* device_id = "";
	uint8_t hour{};
	uint8_t minute{};
	const char* zone_abbreviation = "";
	// Automatic takes the zone from the position. Both are IANA names: the
	// zone in use, empty for UTC, and the zone the position lies in, empty
	// while none is detected.
	bool time_zone_automatic = true;
	const char* zone = "";
	const char* zone_detected = "";
	// Sorted by name. The display keeps what it is given until the next
	// ViewState.
	std::span<const Zone> zones;
	Battery battery{};
	uint8_t battery_percent{};
	uint16_t causes = 0;
	std::optional<uint8_t> busiest_core_percent;
	bool dark_theme{};
	bool track_history{};
	bool simple_symbols = false;
	bool conflict_indicator = false;
	uint8_t radar_range_nm = 4;
	// Traffic further above or below is not in the ViewState, unless it is in
	// conflict. 0 to ALTITUDE_FILTER_MAX_FT, 0 filters nothing.
	uint16_t altitude_filter_ft = 3000;
	Fix fix{};
	uint8_t satellites{};
	uint8_t strongest_satellite_signal_level{};
	Antenna antenna{};
	bool alert{};
	// The sliders on the device own these two, the display only shows them.
	bool wifi_on{};
	bool muted{};
	const char* wifi_network{};
	const char* wifi_password{};
	// While it discharges, or NO_ESTIMATE.
	uint16_t battery_remaining_min{};
	// While it charges, or NO_ESTIMATE.
	uint16_t battery_to_full_min = NO_ESTIMATE;
	// What tells the own aircraft from traffic. The callsign has up to eight
	// characters of A to Z and 0 to 9 and is empty when none is stored.
	uint32_t own_address = NO_ADDRESS;
	const char* own_callsign = "";
	// The targets inside the outer ring, then those beyond it. Each group is
	// sorted by level, then distance. Without a fix, ring positions are unknown
	// and targets stay in report order. Valid during display::update only.
	std::span<const Target> traffic;
	size_t traffic_inside = 0;
	// What to stay out of, against what the own ship flies now: tracks in
	// degrees clockwise from the own track, altitudes in feet above the own
	// one, ground speeds in knots above the own one. Empty with the Conflict
	// Indicator off and without a fix. Valid during display::update only.
	std::span<const conflicts::Band> track_bands, altitude_bands, speed_bands;
	// Lives as long as the program. The display reads it once.
	std::span<const License> licenses;
};

// What a display asks the logic to do when the user interacts with it. The
// display changes nothing itself: the answer is a new ViewState. A null member
// is an action nobody handles.
struct Actions {
	void (*set_dark_theme)(bool dark) = nullptr;
	void (*set_track_history)(bool on) = nullptr;
	void (*set_simple_symbols)(bool simple) = nullptr;
	void (*set_conflict_indicator)(bool on) = nullptr;
	void (*set_radar_range)(uint8_t range_nm) = nullptr;
	void (*set_altitude_filter)(uint16_t feet) = nullptr;
	void (*set_time_zone_automatic)(bool on) = nullptr;
	// An IANA name of ViewState::zones, or an empty one for UTC. Ends
	// automatic.
	void (*set_time_zone)(const char* zone) = nullptr;
	void (*set_muted)(TargetId target, bool muted) = nullptr;
	// NO_ADDRESS and an empty callsign remove what is stored.
	void (*set_own_ship)(uint32_t address) = nullptr;
	void (*set_own_callsign)(const char* callsign) = nullptr;
};
}
