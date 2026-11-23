#include "display_mapping.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <tuple>

#include "device.h"
#include "navigation/great_circle.hpp"
#include "navigation/units.h"
#include "system/health.hpp"
#include "traffic/traffic_table.hpp"

namespace display::mapping {
namespace {
using conflicts::Level;

// ADSBee emitter categories: 1..7 are A1..A7, 9..15 are B1..B7.
enum : uint8_t {
	kCategoryLight = 1,
	kCategorySmall = 2,
	kCategoryLarge = 3,
	kCategoryHighVortex = 4,
	kCategoryHeavy = 5,
	kCategoryHighPerformance = 6,
	kCategoryRotorcraft = 7,
	kCategoryGlider = 9,
	kCategoryLighterThanAir = 10,
	kCategoryUltralight = 12,
};
enum : uint8_t {
	kFlarmGlider = 1,
	kFlarmTowPlane = 2,
	kFlarmHelicopter = 3,
	kFlarmDropPlane = 5,
	kFlarmHangGlider = 6,
	kFlarmParaglider = 7,
	kFlarmPiston = 8,
	kFlarmJet = 9,
	kFlarmGyrocopter = 10,
	kFlarmBalloon = 11,
	kFlarmAirship = 12,
};

int rank(const display::Target& target) {
	return target.muted || target.level < Level::advisory ? 2
		   : target.level == Level::alert                 ? 0
														  : 1;
}

bool before(const display::Target& a, const display::Target& b) {
	const auto key = [](const display::Target& target) {
		return std::tuple(target.radar_position != RadarPosition::inside_ring,
						  rank(target), target.distance_nm, target.address,
						  target.source);
	};
	return key(a) < key(b);
}

uint16_t minutes(battery::OptionalFloat hours) {
	if (hours.valid != 1 || !(hours.value >= 0)) {
		return display::NO_ESTIMATE;
	}
	return static_cast<uint16_t>(
		std::min(hours.value * 60.0f, display::NO_ESTIMATE - 1.0f));
}

uint16_t degrees(float value) {
	const float turned = std::fmod(value, 360.0f);
	return static_cast<uint16_t>(
			   std::lround(turned < 0 ? turned + 360.0f : turned)) %
		   360;
}

display::VerticalTrend trend(const traffic::Target& target) {
	const int16_t rate = target.baro_rate_valid   ? target.baro_rate_fpm
						 : target.gnss_rate_valid ? target.gnss_rate_fpm
												  : int16_t{0};
	using enum display::VerticalTrend;
	return rate >= minimum_climb_or_descent_fpm    ? climbing
		   : rate <= -minimum_climb_or_descent_fpm ? descending
												   : level;
}

bool holds(std::span<const conflicts::Id> ids, uint32_t address,
		   display::Source from) {
	return std::ranges::any_of(ids, [&](const conflicts::Id& entry) {
		return entry.address == address && entry.source == from;
	});
}

display::Fix fix_of(adsbee::Fix fix) {
	switch (fix) {
		case adsbee::Fix::fix_3d:
			return display::Fix::fix_3d;
		case adsbee::Fix::fix_2d:
			return display::Fix::fix_2d;
		default:
			return display::Fix::none;
	}
}

display::Offset offset(const adsbee::Gnss& own, float latitude,
					   float longitude) {
	auto [distance, bearing] = navigation::great_circle(
		own.latitude, own.longitude, latitude, longitude);
	const float turned =
		(bearing - own.track_deg) * units::radians_per_degree<float>;
	return {distance * std::sin(turned), distance * std::cos(turned)};
}

void track(const traffic::Tracked& tracked, const adsbee::Gnss& own,
		   display::Target& out) {
	static_assert(display::kTailSamples >= traffic::kHistorySeconds);
	const traffic::Target& report = tracked.target;
	for (const traffic::Position& was : tracked.past) {
		const display::Offset place = offset(own, was.latitude, was.longitude);
		if (was.sample == traffic::Sample::none ||
			!std::isfinite(place.right_nm) || !std::isfinite(place.ahead_nm)) {
			break;
		}
		out.tail[out.tail_count++] = place;
	}
	out.predicted = report.direction_valid && report.speed_valid &&
					report.speed_kt >= conflicts::minimum_track_speed_kt &&
					std::isfinite(report.track_deg);
	if (out.predicted) {
		out.speed_kt = static_cast<uint16_t>(report.speed_kt);
		out.turn_dps = traffic::turn_rate_dps(tracked);
	}
}

display::Target build_display_target(const traffic::Tracked& tracked,
									 const ViewInputs& sources,
									 const TargetDetails& at) {
	const traffic::Target& report = tracked.target;
	const adsbee::Gnss& own = sources.adsbee.gnss_at(sources.monotonic_now_us);
	const Preferences& preferences = sources.preferences;
	display::Source from = conflicts::source(report.source);
	const bool inside = at.radar_position == RadarPosition::inside_ring;

	std::optional<int32_t> above = at.above_ft;
	display::Target out{};
	out.radar_position = at.radar_position;
	out.show_beyond_ring_marker = at.show_beyond_ring_marker;
	std::memcpy(out.callsign, report.callsign,
				std::min(sizeof out.callsign, sizeof report.callsign));
	out.callsign[sizeof out.callsign - 1] = '\0';
	out.address = report.address;
	out.source = from;
	out.kind = kind(report);
	out.muted =
		std::ranges::any_of(preferences.muted, [&](const conflicts::Id& id) {
			return id.address == out.address &&
				   mute_source(id.source) == mute_source(from);
		});

	out.relative_altitude_ft = above;
	out.vertical_trend = trend(report);
	if (at.position) {
		auto [distance, bearing] = *at.position;
		out.distance_nm = distance;
		out.bearing_deg = degrees(bearing - own.track_deg);
		out.track_valid = report.direction_valid;
		out.track_deg = degrees(report.track_deg - own.track_deg);
		out.own_ship_candidate =
			above && conflicts::near_own_ship(distance, *above);
		if (at.conflict) {
			out.level = at.conflict->level;
			out.conflict_in_s = at.conflict->within_s;
		}
		if (preferences.track_history && !out.muted && inside) {
			track(tracked, own, out);
		}
	}
	return out;
}
}

display::Kind kind(const traffic::Target& target) {
	using display::Kind;
	if (target.source == traffic::Source::flarm) {
		switch (target.flarm_aircraft_type) {
			case kFlarmGlider:
			case kFlarmHangGlider:
			case kFlarmParaglider:
				return Kind::glider;
			case kFlarmTowPlane:
			case kFlarmDropPlane:
			case kFlarmPiston:
				return Kind::light;
			case kFlarmHelicopter:
			case kFlarmGyrocopter:
				return Kind::helicopter;
			case kFlarmJet:
				return Kind::airliner;
			case kFlarmBalloon:
			case kFlarmAirship:
				return Kind::balloon;
			default:
				return Kind::unknown;
		}
	}
	switch (target.mode_s_category) {
		case kCategoryLight:
		case kCategorySmall:
		case kCategoryUltralight:
			return Kind::light;
		case kCategoryLarge:
		case kCategoryHighVortex:
		case kCategoryHeavy:
		case kCategoryHighPerformance:
			return Kind::airliner;
		case kCategoryRotorcraft:
			return Kind::helicopter;
		case kCategoryGlider:
			return Kind::glider;
		case kCategoryLighterThanAir:
			return Kind::balloon;
		default:
			return Kind::unknown;
	}
}

display::Battery battery(const battery::StateData& gauge,
						 const charger::StateData& charger) {
	if (charger.battery() == BatteryDetected::absent ||
		gauge.battery() == BatteryDetected::absent) {
		return display::Battery::absent;
	}
	const uint8_t phase = charger.readings.status1.charge;
	return phase == charger::kChargeDone   ? display::Battery::full
		   : phase != charger::kChargeNone ? display::Battery::charging
										   : display::Battery::discharging;
}

display::Antenna antenna(const adsbee::Gnss& gnss) {
	using display::Antenna;
	switch (gnss.antenna) {
		case adsbee::AntennaStatus::ok:
			return gnss.antenna_power == adsbee::AntennaPower::off
					   ? Antenna::passive
					   : Antenna::active;
		case adsbee::AntennaStatus::shorted:
			return Antenna::shorted;
		case adsbee::AntennaStatus::open:
			return Antenna::disconnected;
		default:
			return Antenna::unknown;
	}
}

using display::bit;
using display::Cause;
static_assert(health::kRequiredI2cDeviceMissing == bit(Cause::i2c));
static_assert(health::kAdsbee == bit(Cause::adsbee));
static_assert(health::kGnss == bit(Cause::gps_receiver));
static_assert(health::kAntennaShort == bit(Cause::antenna_short));
static_assert(health::kChargerOvervoltage == bit(Cause::charger_overvoltage));
static_assert(health::kChargerOverheated == bit(Cause::charger_overheated));
static_assert(health::kBatteryOverheated == bit(Cause::battery_overheated));
static_assert(health::kAntennaOpen == bit(Cause::antenna_open));
static_assert(health::kBackupBattery == bit(Cause::backup_battery));
static_assert(health::kBarometer == bit(Cause::barometer));
static_assert(health::kGauge == bit(Cause::battery_gauge));
static_assert(health::kUsbC == bit(Cause::usb_c));
static_assert(health::kBatteryLow == bit(Cause::battery_low));
static_assert(health::kBatteryCold == bit(Cause::battery_cold));
static_assert(health::kMemory == bit(Cause::memory));
static_assert(health::kCpu == bit(Cause::cpu));
static_assert(health::kFailures == display::FAILURES);

uint8_t signal_level(const adsbee::Gnss& gnss) {
	return gnss.max_cno_dbhz >= good_cno_dbhz   ? display::kMaxSignalLevel
		   : gnss.max_cno_dbhz >= fair_cno_dbhz ? 2
		   : gnss.max_cno_dbhz > 0              ? 1
												: 0;
}

bool inside_ring(float distance_nm, uint8_t range_nm, bool was_inside) {
	const float edge = was_inside ? kRingBandNm / 2 : -kRingBandNm / 2;
	return distance_nm <= static_cast<float>(range_nm) + edge;
}

bool passes_altitude_filter(std::optional<int32_t> above_ft,
							const Preferences& preferences,
							conflicts::Level level) {
	const uint16_t filter_ft = preferences.altitude_filter_ft;
	return conflicts::in_conflict(level) ||
		   !(filter_ft && above_ft && std::abs(*above_ft) > filter_ft);
}

bool within_reach(float distance_nm, float own_kt,
				  std::optional<int32_t> above_ft) {
	const float flown_nm =
		std::isfinite(own_kt)
			? std::max(own_kt, 0.0f) * conflicts::kLookAheadS / 3600
			: 0;
	return distance_nm <= flown_nm + conflicts::kWellClearNm &&
		   (!above_ft || std::abs(*above_ft) <= conflicts::kWellClearFt);
}

TargetDetails target_details(const traffic::Tracked& tracked,
							 const ViewInputs& sources) {
	const traffic::Target& report = tracked.target;
	const adsbee::Gnss& own = sources.adsbee.gnss_at(sources.monotonic_now_us);
	const Preferences& preferences = sources.preferences;
	const bool fix = own.fix != adsbee::Fix::none;
	TargetDetails at;
	if (!report.last_seen_us) {
		at.exclusion_reason = ExclusionReason::no_report;
		return at;
	}
	at.above_ft = sources.conflict_input().relative_altitude_ft(report, fix);
	if (report.address == preferences.own_address) {
		at.exclusion_reason = ExclusionReason::own_ship;
		return at;
	}
	if (fix) {
		RelativePosition position{};
		if (!report.position_valid ||
			!traffic::relative_position(report, own.latitude, own.longitude,
										position.distance_nm,
										position.true_bearing_deg) ||
			!std::isfinite(position.distance_nm) ||
			!std::isfinite(position.true_bearing_deg)) {
			at.exclusion_reason = ExclusionReason::no_position;
			return at;
		}
		at.position = position;
		at.radar_position =
			inside_ring(position.distance_nm, preferences.radar_range_nm,
						holds(sources.previous_inside, report.address,
							  conflicts::source(report.source)))
				? RadarPosition::inside_ring
				: RadarPosition::beyond_ring;
	}
	for (const conflicts::AlertingResult& result : sources.conflicts) {
		if (result.target.address == report.address &&
			result.target.source == conflicts::source(report.source)) {
			at.conflict = result.conflict;
			break;
		}
	}
	const bool conflict = at.conflict && at.conflict->level != Level::other;
	const bool marked =
		at.conflict ? conflict || at.conflict->enters_corrective_volume
					: (at.position && within_reach(at.position->distance_nm,
												   own.speed_kt, at.above_ft));
	if (!passes_altitude_filter(
			at.above_ft, preferences,
			at.conflict ? at.conflict->level : Level::other)) {
		at.exclusion_reason = ExclusionReason::altitude_filter;
	}
	at.show_beyond_ring_marker =
		at.radar_position == RadarPosition::beyond_ring && marked;
	return at;
}

size_t sort_and_count_inside(std::span<display::Target> targets,
							 bool has_own_fix) {
	if (has_own_fix) {
		std::ranges::sort(targets, before);
	}
	return static_cast<size_t>(std::ranges::count(
		targets, RadarPosition::inside_ring, &display::Target::radar_position));
}

size_t inside_ids(const display::ViewState& model,
				  std::span<conflicts::Id> ids) {
	const size_t count = std::min(model.traffic_inside, ids.size());
	for (size_t i = 0; i < count; i++) {
		ids[i] = {model.traffic[i].address, model.traffic[i].source};
	}
	return count;
}

display::ViewState build_view_state(const ViewInputs& sources,
									std::span<display::Target> targets,
									std::span<conflicts::Band> bands,
									std::span<TargetDetails> details) {
	const adsbee::Gnss& gnss = sources.adsbee.gnss_at(sources.monotonic_now_us);
	const Preferences& preferences = sources.preferences;
	const bool fix = gnss.fix != adsbee::Fix::none;
	size_t count = 0;
	for (size_t i = 0; i < sources.traffic.size(); ++i) {
		const auto& tracked = sources.traffic[i];
		const auto at = target_details(tracked, sources);
		if (i < details.size()) {
			details[i] = at;
		}
		if (at.exclusion_reason) {
			continue;
		}
		const auto target = build_display_target(tracked, sources, at);
		if (count < targets.size()) {
			targets[count++] = target;
			continue;
		}

		if (!fix) {
			continue;
		}
		const auto lowest_priority = std::ranges::max_element(targets, before);
		if (lowest_priority != targets.end() &&
			before(target, *lowest_priority)) {
			*lowest_priority = target;
		}
	}
	if (count == targets.size() && count < sources.traffic.size()) {
		for (size_t i = 0; i < std::min(details.size(), sources.traffic.size());
			 ++i) {
			const auto& report = sources.traffic[i].target;
			if (!details[i].exclusion_reason &&
				std::ranges::none_of(
					targets, [&](const display::Target& target) {
						return target.address == report.address &&
							   target.source ==
								   conflicts::source(report.source);
					})) {
				details[i].exclusion_reason = ExclusionReason::list_full;
			}
		}
	}
	const size_t inside = sort_and_count_inside(targets.first(count), fix);

	const bool charger_mode = sources.mode == POWER_MODE_CHARGER;
	const bool charging = sources.battery.rate > 0;
	const uint16_t duration = minutes(sources.battery.charge_duration_hours);
	display::ViewState result;
	result.language = preferences.language;
	result.mode = charger_mode       ? display::Mode::charger
				  : sources.starting ? display::Mode::starting
									 : display::Mode::on;
	result.device_name = sources.device_name;
	result.version = sources.version;
	result.greeting_seed = sources.greeting;
	result.device_id = sources.device_id;
	result.hour = sources.clock.hour;
	result.minute = sources.clock.minute;
	result.zone_abbreviation = sources.clock.time_zone;
	result.time_zone_automatic = sources.zones.automatic;
	result.zone = sources.zones.in_use;
	result.zone_detected = sources.zones.detected;
	result.zones = sources.zones.listed;
	result.battery = battery(sources.battery, sources.charger);
	result.battery_percent = static_cast<uint8_t>(
		std::clamp(std::lround(sources.battery.soc), 0L, 100L));
	result.causes = sources.health;
	result.fix = fix_of(gnss.fix);
	result.busiest_core_percent = sources.busiest_core_percent;
	// The charger screen is dark whatever the theme says, it lights a room
	// at night.
	result.dark_theme = preferences.dark_theme || charger_mode;
	result.track_history = preferences.track_history;
	result.simple_symbols = preferences.simple_symbols;
	result.conflict_indicator = preferences.conflict_indicator;
	result.radar_range_nm = preferences.radar_range_nm;
	result.altitude_filter_ft = preferences.altitude_filter_ft;
	result.satellites =
		static_cast<uint8_t>(std::max<int8_t>(gnss.satellites_used, 0));
	result.strongest_satellite_signal_level = signal_level(gnss);
	result.antenna = antenna(gnss);
	result.alert = std::ranges::any_of(
		targets.first(count), [](const display::Target& target) {
			return target.level == Level::alert && !target.muted;
		});
	result.wifi_on = sources.wifi.slider_enabled;
	result.muted = sources.expander.input.mute;
	result.wifi_network = sources.wifi.reported_known
							  ? sources.wifi.last_reported_config.ssid
							  : sources.device_id;
	result.wifi_password = sources.wifi.reported_known
							   ? sources.wifi.last_reported_config.password
							   : "";
	result.battery_remaining_min = charging ? display::NO_ESTIMATE : duration;
	result.battery_to_full_min = charging ? duration : display::NO_ESTIMATE;
	result.own_address = preferences.own_address;
	result.own_callsign = preferences.own_callsign;
	result.traffic = targets.first(count);
	result.traffic_inside = inside;

	if (preferences.conflict_indicator && fix) {
		size_t banded = 0;
		const auto against = [&](std::span<const conflicts::Band> found,
								 float own) {
			const size_t first = banded;
			for (conflicts::Band band :
				 found.first(std::min(found.size(), bands.size() - banded))) {
				band.from -= own;
				band.to -= own;
				bands[banded++] = band;
			}
			return std::span<const conflicts::Band>(bands).subspan(
				first, banded - first);
		};
		result.track_bands = against(sources.track_bands, gnss.track_deg);
		result.altitude_bands =
			against(sources.altitude_bands,
					sources.conflict_input().own_altitude_ft(gnss));
		result.speed_bands = against(sources.speed_bands, gnss.speed_kt);
	}
	result.licenses = sources.licenses;
	return result;
}
}
