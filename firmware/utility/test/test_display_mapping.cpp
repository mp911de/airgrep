#include <array>
#include <cmath>
#include <cstring>
#include <string_view>
#include <utility>
#include <vector>

#include "check.hpp"
#include "display/display_mapping.hpp"
#include "navigation/units.h"
#include "system/health.hpp"

namespace {
using conflicts::Level;
using display::Battery;
using display::Kind;

constexpr int64_t kNow = 100'000'000;

constexpr float latitude_degrees_per_nm = 1.0f / units::nm_per_degree_latitude;

struct Device {
	adsbee::StateData adsbee;
	std::vector<traffic::Tracked> traffic;
	barometer::StateData barometer;
	battery::StateData battery;
	charger::StateData charger;
	expander::StateData expander;
	wifi::StateData wifi;
	display::mapping::ViewInputs sources{.adsbee = adsbee,
										 .traffic = traffic,
										 .barometer = barometer,
										 .battery = battery,
										 .charger = charger,
										 .expander = expander,
										 .wifi = wifi};
	std::array<display::Target, 4> targets;

	Device() {
		adsbee.gnss.fix = adsbee::Fix::fix_3d;
		adsbee.gnss.latitude = 50;
		adsbee.gnss.longitude = 8;
		adsbee.gnss.track_deg = 90;
		adsbee.gnss.altitude_ft = 3000;
		adsbee.last_gnss_report_us = kNow;
		barometer.status = Status::ok;
		barometer.updated_us = kNow;
		barometer.altitude_m = 3000 / units::feet_per_metre;
		sources.monotonic_now_us = kNow;
		sources.preferences.radar_range_nm = 32;
	}

	traffic::Target& add(const char* callsign, uint32_t address, float north_nm,
						 int32_t altitude_ft) {
		traffic::Target target;
		std::strncpy(target.callsign, callsign, sizeof target.callsign - 1);
		target.address = address;
		target.last_seen_us = kNow;
		target.position_valid = target.baro_alt_valid = true;
		target.latitude = 50 + north_nm * latitude_degrees_per_nm;
		target.longitude = 8;
		target.baro_alt_ft = altitude_ft;
		traffic.push_back({target, {}});
		return traffic.back().target;
	}

	display::ViewState map() {
		sources.traffic = traffic;
		sources.previous_inside = std::span(inside).first(inside_count);
		display::ViewState shown =
			display::mapping::build_view_state(sources, targets);
		inside_count = display::mapping::inside_ids(shown, inside);
		return shown;
	}

	std::array<conflicts::Id, 4> inside{};
	size_t inside_count = 0;
};

void battery_states() {
	Device device;
	device.battery.soc = 77.6f;
	device.battery.rate = -10;
	device.battery.charge_duration_hours = {.value = 4.5f, .valid = 1};
	display::ViewState shown = device.map();
	CHECK(shown.battery == Battery::discharging);
	CHECK_EQUAL(78, shown.battery_percent);
	CHECK_EQUAL(270, shown.battery_remaining_min);
	CHECK_EQUAL(display::NO_ESTIMATE, shown.battery_to_full_min);

	device.charger.readings.status1.charge = 3;
	device.battery.rate = 20;
	device.battery.charge_duration_hours = {.value = 1.25f, .valid = 1};
	shown = device.map();
	CHECK(shown.battery == Battery::charging);
	CHECK_EQUAL(75, shown.battery_to_full_min);
	CHECK_EQUAL(display::NO_ESTIMATE, shown.battery_remaining_min);

	device.battery.charge_duration_hours = {};
	CHECK_EQUAL(display::NO_ESTIMATE, device.map().battery_to_full_min);

	device.charger.readings.status1.charge = charger::kChargeDone;
	CHECK(device.map().battery == Battery::full);

	device.charger.battery_detected = BatteryDetected::absent;
	CHECK(device.map().battery == Battery::absent);
	device.charger.battery_detected = BatteryDetected::unknown;
	device.battery.status = Status::off;
	CHECK(device.map().battery == Battery::absent);

	device.battery.soc = 104;
	CHECK_EQUAL(100, device.map().battery_percent);
	device.battery.soc = -3;
	CHECK_EQUAL(0, device.map().battery_percent);
}

void gnss() {
	using display::Antenna;
	Device device;
	adsbee::Gnss& gnss = device.adsbee.gnss;
	gnss.satellites_used = 11;
	const display::ViewState shown = device.map();
	CHECK(shown.fix == display::Fix::fix_3d);
	CHECK_EQUAL(11, shown.satellites);
	CHECK(shown.antenna == Antenna::unknown);

	const struct {
		int8_t antenna, power;
		Antenna expected;
	} antennas[] = {{-1, -1, Antenna::unknown}, {0, 1, Antenna::unknown},
					{1, 1, Antenna::unknown},   {2, 1, Antenna::active},
					{2, 2, Antenna::active},    {2, 0, Antenna::passive},
					{3, 1, Antenna::shorted},   {4, 1, Antenna::disconnected}};
	for (const auto& [antenna, power, expected] : antennas) {
		gnss.antenna = static_cast<adsbee::AntennaStatus>(antenna);
		gnss.antenna_power = static_cast<adsbee::AntennaPower>(power);
		CHECK(device.map().antenna == expected);
	}

	const struct {
		int16_t cno;
		uint8_t level;
	} signals[] = {{-1, 0}, {0, 0},  {12, 1}, {29, 1},
				   {30, 2}, {39, 2}, {40, 3}};
	for (const auto& [cno, level] : signals) {
		gnss.max_cno_dbhz = cno;
		CHECK_EQUAL(level, device.map().strongest_satellite_signal_level);
	}

	gnss.satellites_used = -1;
	CHECK_EQUAL(0, device.map().satellites);
	device.adsbee.last_gnss_report_us = kNow - kStateMaxAgeUs - 1;
	gnss.satellites_used = 11;
	CHECK(device.map().fix == display::Fix::none);
	CHECK_EQUAL(0, device.map().satellites);
	CHECK_EQUAL(0, device.map().strongest_satellite_signal_level);
	CHECK(device.map().antenna == Antenna::unknown);
	device.adsbee.last_gnss_report_us = 0;
	CHECK(device.map().fix == display::Fix::none);
	device.adsbee.last_gnss_report_us = kNow;
	gnss.fix = adsbee::Fix::fix_2d;
	CHECK(device.map().fix == display::Fix::fix_2d);
	gnss.fix = adsbee::Fix::none;
	CHECK(device.map().fix == display::Fix::none);
}

void health_levels() {
	using display::Cause;
	using display::Health;
	using display::level;
	CHECK(level(0) == Health::ok);
	CHECK(level(bit(Cause::i2c)) == Health::failure);
	CHECK(level(bit(Cause::battery_overheated)) == Health::failure);
	CHECK(level(bit(Cause::antenna_open)) == Health::caution);
	CHECK(level(bit(Cause::cpu)) == Health::caution);
	CHECK(level(bit(Cause::cpu) | bit(Cause::adsbee)) == Health::failure);
	CHECK(level(bit(Cause::cpu) | bit(Cause::memory)) == Health::caution);

	Device device;
	device.sources.health = health::kAntennaOpen | health::kCpu;
	device.sources.busiest_core_percent = 87;
	const display::ViewState shown = device.map();
	CHECK_EQUAL(bit(Cause::antenna_open) | bit(Cause::cpu), shown.causes);
	CHECK(shown.busiest_core_percent == 87);
	device.sources.busiest_core_percent = 0;
	CHECK(device.map().busiest_core_percent == 0);
	device.sources.busiest_core_percent.reset();
	CHECK(!device.map().busiest_core_percent);
}

void device_facts() {
	Device device;
	device.sources.device_id = "airgrep-0123456789ab";
	device.sources.device_name = "S3 Plus";
	device.sources.version = "0.1.0";
	device.sources.clock = {14, 32, "UTC"};
	device.expander.input.mute = true;
	display::ViewState shown = device.map();
	CHECK(shown.mode == display::Mode::on);
	CHECK(shown.muted);
	CHECK(!shown.wifi_on);
	CHECK_EQUAL(std::string_view("airgrep-0123456789ab"), shown.wifi_network);
	CHECK_EQUAL(std::string_view(""), shown.wifi_password);
	CHECK_EQUAL(14, shown.hour);
	CHECK_EQUAL(32, shown.minute);
	CHECK_EQUAL(std::string_view("UTC"), shown.zone_abbreviation);
	CHECK_EQUAL(std::string_view("S3 Plus"), shown.device_name);
	CHECK(!shown.dark_theme);
	CHECK(!shown.simple_symbols);
	device.sources.preferences.simple_symbols = true;
	CHECK(device.map().simple_symbols);

	device.wifi.slider_enabled = device.wifi.reported_known = true;
	std::strcpy(device.wifi.last_reported_config.ssid, "airgrep-net");
	std::strcpy(device.wifi.last_reported_config.password, "secret");
	shown = device.map();
	CHECK(shown.wifi_on);
	CHECK_EQUAL(std::string_view("airgrep-net"), shown.wifi_network);
	CHECK_EQUAL(std::string_view("secret"), shown.wifi_password);

	device.sources.starting = true;
	CHECK(device.map().mode == display::Mode::starting);
	device.sources.mode = POWER_MODE_CHARGER;
	shown = device.map();
	CHECK(shown.mode == display::Mode::charger);
	CHECK(shown.dark_theme);
}

bool near(float expected, float actual) {
	return std::fabs(expected - actual) < 0.01f;
}

void placement() {
	Device device;
	traffic::Target& north = device.add("NORTH", 0x111111, 3, 3500);
	north.direction_valid = true;
	north.track_deg = 180;
	north.baro_rate_valid = true;
	north.baro_rate_fpm = -640;
	const display::ViewState shown = device.map();
	CHECK_EQUAL(1u, shown.traffic.size());
	const display::Target& target = shown.traffic[0];
	CHECK_EQUAL(std::string_view("NORTH"), target.callsign);
	CHECK(target.distance_nm > 2.99f && target.distance_nm < 3.01f);
	// Flying east, north is to the left and a southbound target crosses to
	// the right.
	CHECK_EQUAL(270, target.bearing_deg);
	CHECK(target.track_valid);
	CHECK_EQUAL(90, target.track_deg);
	CHECK(target.relative_altitude_ft == 500);
	CHECK(target.vertical_trend == display::VerticalTrend::descending);
	CHECK(target.level == Level::other);
	CHECK(!target.own_ship_candidate);
}
void relative_altitude_uses_two_gnss_altitudes() {
	Device device;
	auto& north = device.add("NORTH", 1, 3, 3500);
	device.barometer.altitude_m = 5500 / units::feet_per_metre;
	north.baro_alt_valid = false;
	north.gnss_alt_valid = true;
	north.gnss_alt_ft = 2600;
	north.baro_rate_valid = false;
	CHECK(device.map().traffic[0].relative_altitude_ft == -400);
	CHECK(device.map().traffic[0].vertical_trend ==
		  display::VerticalTrend::level);
}

void altitude_filter() {
	Device device;
	device.add("ABOVE", 1, 1, 6000);
	device.add("BELOW", 2, 2, 0);
	device.add("EDGE", 3, 3, 6001);
	device.add("UNKNOWN", 4, 4, 0).baro_alt_valid = false;
	display::ViewState shown = device.map();
	CHECK_EQUAL(3000, shown.altitude_filter_ft);
	const char* within[] = {"ABOVE", "BELOW", "UNKNOWN"};
	CHECK_EQUAL(3u, shown.traffic.size());
	for (size_t i = 0; i < shown.traffic.size(); i++) {
		CHECK_EQUAL(std::string_view(within[i]), shown.traffic[i].callsign);
	}

	device.sources.preferences.altitude_filter_ft = 1000;
	shown = device.map();
	CHECK_EQUAL(1u, shown.traffic.size());
	CHECK_EQUAL(std::string_view("UNKNOWN"), shown.traffic[0].callsign);

	device.sources.preferences.altitude_filter_ft = 0;
	CHECK_EQUAL(4u, device.map().traffic.size());
}

void reach() {
	// What the engine cannot judge counts as standing still. In the 180 s of
	// the horizon the own ship flies 6 NM at 120 kt, and 0.66 NM is too close.
	CHECK(display::mapping::within_reach(6.6f, 120, 0));
	CHECK(!display::mapping::within_reach(6.7f, 120, 0));
	CHECK(display::mapping::within_reach(0.6f, 0, 0));
	CHECK(!display::mapping::within_reach(0.7f, 0, 0));
	CHECK(display::mapping::within_reach(0.6f, NAN, 0));
	CHECK(display::mapping::within_reach(1, 120, 450));
	CHECK(!display::mapping::within_reach(1, 120, -451));
	CHECK(display::mapping::within_reach(1, 120, std::nullopt));
}

void unjudged_offscale_targets_use_reach() {
	Device device;
	device.sources.preferences.radar_range_nm = 4;
	device.adsbee.gnss.speed_kt = 120;
	device.add("NEAR", 2, 4.1f, 3000);
	device.add("INSIDE", 3, 3.5f, 3000);
	device.add("FAR", 1, 9.9f, 3000);
	const display::ViewState shown = device.map();
	CHECK_EQUAL(3u, shown.traffic.size());
	CHECK_EQUAL(1u, shown.traffic_inside);
	CHECK_EQUAL(std::string_view("INSIDE"), shown.traffic[0].callsign);
	CHECK_EQUAL(std::string_view("NEAR"), shown.traffic[1].callsign);
	CHECK(shown.traffic[1].show_beyond_ring_marker);
	CHECK_EQUAL(std::string_view("FAR"), shown.traffic[2].callsign);
	CHECK(!shown.traffic[2].show_beyond_ring_marker);
}
void engine_closing_overrides_reach_estimate() {
	Device device;
	device.sources.preferences.radar_range_nm = 4;
	device.adsbee.gnss.speed_kt = 120;
	device.add("NEAR", 2, 4.1f, 3000);
	device.add("FAR", 1, 9.9f, 3000);
	const conflicts::AlertingResult results[] = {
		{{1, display::Source::adsb}, {.enters_corrective_volume = true}},
		{{2, display::Source::adsb}, {}}};
	device.sources.conflicts = results;
	const auto shown = device.map();
	CHECK_EQUAL(2u, shown.traffic.size());
	CHECK_EQUAL(2u, shown.traffic[0].address);
	CHECK(!shown.traffic[0].show_beyond_ring_marker);
	CHECK_EQUAL(1u, shown.traffic[1].address);
	CHECK(shown.traffic[1].show_beyond_ring_marker);
}

void conflict_bypasses_altitude_filter_and_sorts_first() {
	Device device;
	device.sources.preferences.radar_range_nm = 4;
	device.add("NEAR", 2, 4.1f, 3000);
	device.add("FAR", 1, 9.9f, 9000);
	CHECK_EQUAL(1u, device.map().traffic.size());
	conflicts::AlertingResult advisory{{1, display::Source::adsb},
									   {.level = Level::advisory,
										.within_s = 40,
										.enters_corrective_volume = true}};
	device.sources.conflicts = {&advisory, 1};
	const auto shown = device.map();
	CHECK_EQUAL(2u, shown.traffic.size());
	CHECK_EQUAL(1u, shown.traffic[0].address);
	CHECK(shown.traffic[0].level == Level::advisory);
	CHECK_EQUAL(40, shown.traffic[0].conflict_in_s);
	CHECK(shown.traffic[0].show_beyond_ring_marker);
}

void same_address_different_source() {
	Device device;
	device.add("ICAO", 1, 2, 7500).source = traffic::Source::adsb;
	device.add("NONICAO", 1, 2, 7500).source = traffic::Source::adsb_other;
	conflicts::AlertingResult results[] = {
		{{1, conflicts::source(traffic::Source::adsb)},
		 {.level = Level::alert}},
		{{1, conflicts::source(traffic::Source::adsb_other)}, {}}};
	device.sources.conflicts = results;
	display::ViewState shown = device.map();
	CHECK_EQUAL(1u, shown.traffic.size());
	CHECK(shown.alert);
	CHECK(!shown.traffic.empty() &&
		  std::string_view(shown.traffic.front().callsign) == "ICAO");
	CHECK(display::mapping::target_details(device.traffic[1], device.sources)
			  .exclusion_reason ==
		  display::mapping::ExclusionReason::altitude_filter);

	results[1].conflict.level = Level::advisory;
	shown = device.map();
	CHECK_EQUAL(2u, shown.traffic.size());
	if (shown.traffic.size() != 2) {
		return;
	}
	CHECK(shown.traffic[0].source != shown.traffic[1].source);
	CHECK(shown.traffic[0].level == Level::alert);
	CHECK(shown.traffic[1].level == Level::advisory);
	for (const conflicts::AlertingResult& chosen : results) {
		device.sources.preferences.muted = {&chosen.target, 1};
		shown = device.map();
		CHECK(!shown.alert);
		CHECK(shown.traffic.size() == 2 && shown.traffic[0].muted &&
			  shown.traffic[1].muted);
	}
}

// The engine says its Bands in degrees true, feet and knots. A ViewState holds
// them against the own ship, which flies east at 3000 ft and 100 kt.
void bands() {
	using conflicts::Band;
	Device device;
	device.adsbee.gnss.speed_kt = 100;
	const Band track[] = {{70, 110, conflicts::BandRegion::corrective},
						  {300, 360, conflicts::BandRegion::preventive}};
	const Band altitude[] = {{2800, 3500, conflicts::BandRegion::preventive}};
	const Band speed[] = {{90, 140, conflicts::BandRegion::warning},
						  {140, 150, conflicts::BandRegion::recovery}};
	device.sources.track_bands = track;
	device.sources.altitude_bands = altitude;
	device.sources.speed_bands = speed;
	std::array<Band, 8> room;
	const auto map = [&](std::span<Band> bands) {
		return display::mapping::build_view_state(device.sources,
												  device.targets, bands);
	};

	display::ViewState shown = map(room);
	CHECK(!shown.conflict_indicator);
	CHECK_EQUAL(0u, shown.track_bands.size());
	CHECK_EQUAL(0u, shown.altitude_bands.size());
	CHECK_EQUAL(0u, shown.speed_bands.size());

	device.sources.preferences.conflict_indicator = true;
	shown = map(room);
	CHECK(shown.conflict_indicator);
	CHECK_EQUAL(2u, shown.track_bands.size());
	CHECK_EQUAL(-20, shown.track_bands[0].from);
	CHECK_EQUAL(20, shown.track_bands[0].to);
	CHECK(shown.track_bands[0].region == conflicts::BandRegion::corrective);
	CHECK_EQUAL(210, shown.track_bands[1].from);
	CHECK_EQUAL(270, shown.track_bands[1].to);
	CHECK_EQUAL(1u, shown.altitude_bands.size());
	CHECK(std::abs(shown.altitude_bands[0].from + 200) < 0.5f);
	CHECK(std::abs(shown.altitude_bands[0].to - 500) < 0.5f);
	CHECK(shown.altitude_bands[0].region == conflicts::BandRegion::preventive);
	CHECK_EQUAL(2u, shown.speed_bands.size());
	CHECK_EQUAL(-10, shown.speed_bands[0].from);
	CHECK_EQUAL(40, shown.speed_bands[0].to);
	CHECK(shown.speed_bands[1].region == conflicts::BandRegion::recovery);

	shown = map(std::span(room).first(3));
	CHECK_EQUAL(2u, shown.track_bands.size());
	CHECK_EQUAL(1u, shown.altitude_bands.size());
	CHECK_EQUAL(0u, shown.speed_bands.size());
	CHECK_EQUAL(0u, device.map().track_bands.size());

	device.adsbee.gnss.fix = adsbee::Fix::none;
	shown = map(room);
	CHECK(shown.conflict_indicator);
	CHECK_EQUAL(0u, shown.track_bands.size());
	CHECK_EQUAL(0u, shown.altitude_bands.size());
	CHECK_EQUAL(0u, shown.speed_bands.size());
}

void altitude_bands_use_gnss_without_barometer() {
	Device device;
	device.sources.preferences.conflict_indicator = true;
	device.adsbee.gnss.altitude_ft = 3100;
	device.barometer.status = Status::off;
	const conflicts::Band altitude[] = {
		{2800, 3500, conflicts::BandRegion::preventive}};
	device.sources.altitude_bands = altitude;
	std::array<conflicts::Band, 1> room;
	const auto shown = display::mapping::build_view_state(device.sources,
														  device.targets, room);
	CHECK_EQUAL(1u, shown.altitude_bands.size());
	CHECK(near(-300, shown.altitude_bands.front().from));
	CHECK(near(400, shown.altitude_bands.front().to));
}

struct HeadOnEncounter {
	static constexpr float speed_kt = 100;
	static constexpr float separation_nm = 2;
	Device device;
	conflicts::Detector conflicts;
	traffic::Target* target;
	std::array<conflicts::AlertingResult, 4> results;

	explicit HeadOnEncounter(int32_t altitude_ft)
		: target(&device.add("AHEAD", 1, 0, altitude_ft)) {
		device.adsbee.gnss.speed_kt = speed_kt;
		target->direction_valid = target->speed_valid = true;
		target->baro_rate_valid = true;
		target->track_deg = 270;
		target->speed_kt = speed_kt;
	}

	display::Target fly(int seconds, float own_baro_climb_fpm = 0) {
		const float longitude_degrees_per_nm =
			latitude_degrees_per_nm /
			std::cos(50 * units::radians_per_degree<float>);
		size_t count = 0;
		for (int second = 0; second <= seconds; second++) {
			const float flown = speed_kt * static_cast<float>(second) / 3600;
			device.sources.monotonic_now_us = kNow + second * 1000000LL;
			device.adsbee.last_gnss_report_us = device.barometer.updated_us =
				target->last_seen_us = device.sources.monotonic_now_us;
			device.adsbee.gnss.longitude = 8 + flown * longitude_degrees_per_nm;
			target->longitude =
				8 + (separation_nm - flown) * longitude_degrees_per_nm;
			device.barometer.altitude_m =
				(3000 + own_baro_climb_fpm * static_cast<float>(second) / 60) /
				units::feet_per_metre;
			device.sources.traffic = device.traffic;
			count = conflicts.detect(device.sources.conflict_input(), results)
						.value_or(0);
		}
		device.sources.conflicts = std::span(results).first(count);
		return device.map().traffic[0];
	}
};

void conflict() {
	HeadOnEncounter head_on(3000);
	display::Target shown = head_on.fly(5);
	CHECK(shown.level == Level::alert);
	CHECK(shown.conflict_in_s < 30);

	HeadOnEncounter above(4500);
	shown = above.fly(5);
	CHECK(shown.level == Level::other);
	CHECK_EQUAL(conflicts::NO_CONFLICT, shown.conflict_in_s);

	HeadOnEncounter hovering(3000);
	hovering.target->direction_valid = false;
	hovering.target->speed_kt = 0;
	hovering.fly(5);
	CHECK(hovering.results[0].conflict.enters_corrective_volume);

	HeadOnEncounter silent(3000);
	silent.target->baro_rate_valid = false;
	const display::Target no_rate = silent.fly(5);
	HeadOnEncounter level(3000);
	level.target->baro_rate_fpm = 0;
	const display::Target level_flight = level.fly(5);
	CHECK(level_flight.level == Level::alert);
	CHECK(no_rate.level == level_flight.level);
	CHECK_EQUAL(level_flight.conflict_in_s, no_rate.conflict_in_s);

	HeadOnEncounter unknown(3000);
	unknown.target->speed_valid = false;
	shown = unknown.fly(5);
	CHECK(shown.level == Level::other);
	CHECK_EQUAL(conflicts::NO_CONFLICT, shown.conflict_in_s);
}

void climbing_into_target() {
	HeadOnEncounter climbing(4500);
	CHECK(climbing.fly(8, 3000).level != Level::other);
}

void inside_ring_precedes_outside_alert() {
	display::Target ranked[3] = {};
	ranked[0].level = Level::alert;
	ranked[0].distance_nm = 5;
	ranked[0].radar_position = display::RadarPosition::beyond_ring;
	ranked[1].radar_position = ranked[2].radar_position =
		display::RadarPosition::inside_ring;
	ranked[1].distance_nm = 3;
	ranked[2].level = Level::advisory;
	ranked[2].distance_nm = 3.5f;
	CHECK_EQUAL(2u, display::mapping::sort_and_count_inside(ranked, true));
	CHECK(ranked[0].level == Level::advisory);
	CHECK(ranked[1].level == Level::other);
	CHECK(ranked[2].level == Level::alert);

	display::Target reported[2] = {};
	reported[0].address = 2;
	reported[1].address = 1;
	CHECK_EQUAL(0u, display::mapping::sort_and_count_inside(reported, false));
	CHECK_EQUAL(2u, reported[0].address);
}

void listing() {
	display::mapping::Preferences filter{.altitude_filter_ft = 3000};
	CHECK(display::mapping::passes_altitude_filter(-3000, filter));
	CHECK(!display::mapping::passes_altitude_filter(3001, filter));
	CHECK(display::mapping::passes_altitude_filter(std::nullopt, filter));
	filter.altitude_filter_ft = 0;
	CHECK(display::mapping::passes_altitude_filter(30000, filter));
}

void ring_band() {
	constexpr bool previously_inside = true, previously_outside = false;
	CHECK(display::mapping::inside_ring(3.85f, 4, previously_outside));
	CHECK(!display::mapping::inside_ring(3.95f, 4, previously_outside));
	CHECK(display::mapping::inside_ring(4.05f, 4, previously_inside));
	CHECK(!display::mapping::inside_ring(4.15f, 4, previously_inside));

	Device device;
	device.sources.preferences.radar_range_nm = 4;
	device.adsbee.gnss.speed_kt = 150;
	traffic::Target& target = device.add("HOVER", 1, 0, 3000);
	const struct {
		float north_nm;
		size_t expected_inside;
	} path[] = {{3.95f, 0}, {4.05f, 0}, {3.95f, 0}, {3.85f, 1}, {3.95f, 1},
				{4.05f, 1}, {3.95f, 1}, {4.15f, 0}, {4.05f, 0}, {3.95f, 0}};
	for (const auto& [north_nm, inside] : path) {
		target.latitude = 50 + north_nm * latitude_degrees_per_nm;
		const display::ViewState shown = device.map();
		CHECK_EQUAL(1u, shown.traffic.size());
		CHECK_EQUAL(inside, shown.traffic_inside);
		CHECK_EQUAL(inside == 1, (shown.traffic[0].radar_position ==
								  display::RadarPosition::inside_ring));
	}

	target.latitude = 50 + 3.85f * latitude_degrees_per_nm;
	CHECK_EQUAL(1u, device.map().traffic_inside);
	device.sources.preferences.radar_range_nm = 2;
	CHECK_EQUAL(0u, device.map().traffic_inside);
}

display::Target colocated_unconfigured_target(uint16_t track_deg) {
	HeadOnEncounter own(3000);
	own.target->track_deg = track_deg;
	const float longitude_degrees_per_nm =
		latitude_degrees_per_nm /
		std::cos(50 * units::radians_per_degree<float>);
	size_t count = 0;
	for (int second = 0; second <= 5; second++) {
		own.device.sources.monotonic_now_us = kNow + second * 1000000LL;
		own.device.adsbee.last_gnss_report_us =
			own.device.barometer.updated_us = own.target->last_seen_us =
				own.device.sources.monotonic_now_us;
		own.device.adsbee.gnss.longitude = own.target->longitude =
			8 + 100.0f * static_cast<float>(second) / 3600 *
					longitude_degrees_per_nm;
		own.device.sources.traffic = own.device.traffic;
		count = own.conflicts
					.detect(own.device.sources.conflict_input(), own.results)
					.value_or(0);
	}
	own.device.sources.conflicts = std::span(own.results).first(count);
	return own.device.map().traffic[0];
}

static_assert(!conflicts::in_conflict(Level::other) &&
			  conflicts::in_conflict(Level::proximate) &&
			  conflicts::in_conflict(Level::advisory) &&
			  conflicts::in_conflict(Level::alert));

void own_transponder() {
	const display::Target shown = colocated_unconfigured_target(90);
	CHECK(shown.own_ship_candidate);
	CHECK(shown.level == Level::other);
	CHECK_EQUAL(conflicts::NO_CONFLICT, shown.conflict_in_s);
	CHECK(colocated_unconfigured_target(0).level == Level::alert);
}

void unknown_altitude() {
	Device device;
	device.sources.preferences.altitude_filter_ft = 0;
	traffic::Target& near = device.add("NEAR", 0x111111, 0.05f, 9000);

	device.barometer.updated_us = kNow - kStateMaxAgeUs - 1;
	const auto relative_altitude = [&] {
		return display::mapping::target_details(device.traffic.front(),
												device.sources)
			.above_ft;
	};
	CHECK(!relative_altitude());
	CHECK(!device.map().traffic[0].relative_altitude_ft);
	CHECK(!device.map().traffic[0].own_ship_candidate);

	device.barometer.updated_us = kNow;
	device.barometer.status = Status::off;
	CHECK(!device.map().traffic[0].own_ship_candidate);
	device.barometer.status = Status::ok;
	device.barometer.altitude_m = NAN;
	CHECK(!relative_altitude());
	CHECK(!device.map().traffic[0].relative_altitude_ft);

	device.barometer.altitude_m = 0;
	near.baro_alt_ft = 0;
	CHECK(relative_altitude() == 0);
	CHECK(device.map().traffic[0].relative_altitude_ft == 0);
	near.baro_alt_ft = INT32_MAX;
	CHECK(device.map().traffic[0].relative_altitude_ft == INT16_MAX);
	near.baro_alt_valid = false;
	near.gnss_alt_valid = true;
	near.gnss_alt_ft = INT32_MIN;
	device.adsbee.gnss.altitude_ft = INT32_MAX;
	CHECK(device.map().traffic[0].relative_altitude_ft == INT16_MIN);

	near.latitude = NAN;
	CHECK_EQUAL(0u, device.map().traffic.size());
}

void selection() {
	Device device;
	device.add("OWN", 0x3D2A7F, 0.02f, 3050);
	device.add("NOWHERE", 0x222222, 1, 3000).position_valid = false;
	device.traffic.emplace_back();
	display::ViewState shown = device.map();
	CHECK_EQUAL(1u, shown.traffic.size());
	CHECK(shown.traffic[0].own_ship_candidate);

	device.sources.preferences.own_address = 0x3D2A7F;
	CHECK_EQUAL(0u, device.map().traffic.size());

	device.adsbee.gnss.fix = adsbee::Fix::none;
	shown = device.map();
	CHECK_EQUAL(1u, shown.traffic.size());
	CHECK_EQUAL(std::string_view("NOWHERE"), shown.traffic[0].callsign);
}

void order() {
	Device device;
	device.sources.preferences.radar_range_nm = 4;
	device.add("FAR", 1, 5, 3000);
	device.add("NEAR", 2, 1, 3000);
	device.add("MID", 3, 2, 3000);
	device.add("NEXT", 4, 1.5f, 3000);
	device.add("GONE", 5, 9, 3000);
	display::ViewState shown = device.map();
	CHECK_EQUAL(4u, shown.traffic.size());
	const char* expected[] = {"NEAR", "NEXT", "MID", "FAR"};
	for (size_t i = 0; i < shown.traffic.size(); i++) {
		CHECK_EQUAL(std::string_view(expected[i]), shown.traffic[i].callsign);
	}
	CHECK(!shown.alert);
	std::array<display::mapping::TargetDetails, 5> details;
	display::mapping::build_view_state(
		device.sources, std::span(device.targets).first(1), {}, details);
	CHECK(details[0].exclusion_reason ==
		  display::mapping::ExclusionReason::list_full);
	CHECK(!details[1].exclusion_reason);
	CHECK(details[0].radar_position == display::RadarPosition::beyond_ring);

	device.adsbee.gnss.fix = adsbee::Fix::none;
	shown = device.map();
	const char* reported[] = {"FAR", "NEAR", "MID", "NEXT"};
	CHECK_EQUAL(4u, shown.traffic.size());
	for (size_t i = 0; i < shown.traffic.size(); i++) {
		CHECK_EQUAL(std::string_view(reported[i]), shown.traffic[i].callsign);
	}
	CHECK_EQUAL(0u, shown.traffic_inside);
	CHECK(shown.traffic.front().radar_position ==
		  display::RadarPosition::unknown);
}

void mute_identity_includes_source() {
	Device device;
	device.add("NEAR", 2, 1, 3000);
	device.add("FAR", 1, 2, 3000);
	const conflicts::Id matching{2, display::Source::adsb};
	device.sources.preferences.muted = {&matching, 1};
	const auto shown = device.map();
	CHECK(shown.traffic[0].muted);
	CHECK(!shown.traffic[1].muted);
	const conflicts::Id different_source{2, display::Source::flarm};
	device.sources.preferences.muted = {&different_source, 1};
	CHECK(!device.map().traffic[0].muted);
}

void equal_distance_sorts_by_address() {
	for (const bool reverse : {false, true}) {
		Device device;
		const uint32_t first = reverse ? 1 : 3;
		const uint32_t last = reverse ? 3 : 1;
		device.add("FIRST", first, 1, 3000);
		device.add("MIDDLE", 2, 1, 3000);
		device.add("LAST", last, 1, 3000);
		const auto shown = device.map();
		CHECK_EQUAL(3u, shown.traffic.size());
		for (size_t i = 0; i < 3; i++) {
			CHECK_EQUAL(i + 1, shown.traffic[i].address);
		}
	}
}

void tracks() {
	Device device;
	// Two miles north and one east of the own ship, which flies east: on its
	// left and ahead of it.
	traffic::Target& target = device.add("TRACK", 0x111111, 2, 3000);
	target.longitude = 8 + latitude_degrees_per_nm /
							   std::cos(50 * units::radians_per_degree<float>);
	target.direction_valid = target.speed_valid = true;
	target.track_deg = 0;
	target.speed_kt = 120;
	traffic::Tracked& tracked = device.traffic.back();
	for (int age = 0; age < 10; age++) {
		tracked.past[age] = {target.latitude - static_cast<float>(age) *
												   (120 / 3600.0f) *
												   latitude_degrees_per_nm,
							 target.longitude,
							 3000,
							 120,
							 traffic::Sample::fresh,
							 traffic::AltitudeDatum::barometric};
	}
	display::Target shown = device.map().traffic[0];
	CHECK_EQUAL(10, shown.tail_count);
	// North is to the left of an own ship that flies east, east is ahead.
	CHECK(near(-2, shown.tail[0].right_nm));
	CHECK(near(1, shown.tail[0].ahead_nm));
	CHECK(near(-1.7f, shown.tail[9].right_nm));
	CHECK(near(1, shown.tail[9].ahead_nm));
	CHECK(shown.predicted);
	CHECK_EQUAL(120, shown.speed_kt);
	CHECK_EQUAL(0, shown.turn_dps);

	tracked.past[4].sample = traffic::Sample::none;
	CHECK_EQUAL(4, device.map().traffic[0].tail_count);
	tracked.past[4].sample = traffic::Sample::held;
	CHECK_EQUAL(10, device.map().traffic[0].tail_count);

	target.source = traffic::Source::flarm;
	target.flarm_turn_rate_dps = 6;
	CHECK_EQUAL(6, device.map().traffic[0].turn_dps);

	for (const bool missing_speed : {true, false}) {
		target.speed_valid = !missing_speed;
		target.direction_valid = missing_speed;
		shown = device.map().traffic[0];
		CHECK(!shown.predicted);
		CHECK_EQUAL(10, shown.tail_count);
	}
	target.speed_valid = target.direction_valid = true;
	target.speed_kt = conflicts::minimum_track_speed_kt - 1;
	CHECK(!device.map().traffic[0].predicted);
	target.speed_kt = conflicts::minimum_track_speed_kt;
	CHECK(device.map().traffic[0].predicted);
	target.speed_kt = 120;
}

void add_moving_target(Device& device) {
	auto& target = device.add("TRACK", 1, 1, 3000);
	target.speed_valid = target.direction_valid = true;
	target.speed_kt = 120;
	auto& sample = device.traffic.back().past[0];
	sample.latitude = target.latitude;
	sample.longitude = target.longitude;
	sample.sample = traffic::Sample::fresh;
	const auto shown = device.map().traffic[0];
	CHECK_EQUAL(1, shown.tail_count);
	CHECK(shown.predicted);
}

void retained_targets_keep_their_own_history() {
	Device device;
	for (uint32_t i = 0; i < 6; ++i) {
		const auto& target =
			device.add("TRACK", i + 1, 6.0f - static_cast<float>(i), 3000);
		auto& past = device.traffic.back().past;
		for (size_t age = 0; age <= i; ++age) {
			past[age].latitude = target.latitude;
			past[age].longitude = target.longitude;
			past[age].sample =
				age ? traffic::Sample::held : traffic::Sample::fresh;
		}
	}
	auto shown = device.map();
	for (size_t i = 0; i < shown.traffic.size(); ++i) {
		const auto& target = shown.traffic[i];
		CHECK_EQUAL(6u - i, target.address);
		CHECK_EQUAL(target.address, target.tail_count);
		CHECK(near(-target.distance_nm, target.tail[0].right_nm));
		for (size_t age = 1; age < target.tail_count; ++age) {
			CHECK_EQUAL(target.tail[0].right_nm, target.tail[age].right_nm);
			CHECK_EQUAL(target.tail[0].ahead_nm, target.tail[age].ahead_nm);
		}
	}
	// A muted replacement must also drop the history queued for its old slot.
	const conflicts::Id muted{6, display::Source::adsb};
	device.sources.preferences.muted = {&muted, 1};
	shown = device.map();
	CHECK_EQUAL(6u, shown.traffic.front().address);
	CHECK_EQUAL(0, shown.traffic.front().tail_count);
}

void track_history_off_hides_track() {
	Device device;
	add_moving_target(device);
	device.sources.preferences.track_history = false;
	const auto shown = device.map().traffic[0];
	CHECK_EQUAL(0, shown.tail_count);
	CHECK(!shown.predicted);
}

void muted_target_hides_track() {
	Device device;
	add_moving_target(device);
	const conflicts::Id muted{1, display::Source::adsb};
	device.sources.preferences.muted = {&muted, 1};
	const auto shown = device.map().traffic[0];
	CHECK_EQUAL(0, shown.tail_count);
	CHECK(!shown.predicted);
}

void lost_fix_hides_track() {
	Device device;
	add_moving_target(device);
	device.adsbee.gnss.fix = adsbee::Fix::none;
	const auto shown = device.map().traffic[0];
	CHECK_EQUAL(0, shown.tail_count);
	CHECK(!shown.predicted);
}

void kinds() {
	traffic::Target target;
	const struct {
		uint8_t category;
		Kind expected;
	} emitters[] = {
		{0, Kind::unknown},    {1, Kind::light},    {2, Kind::light},
		{3, Kind::airliner},   {5, Kind::airliner}, {6, Kind::airliner},
		{7, Kind::helicopter}, {9, Kind::glider},   {10, Kind::balloon},
		{12, Kind::light},     {14, Kind::unknown}, {17, Kind::unknown}};
	for (const auto& [category, expected] : emitters) {
		target.mode_s_category = category;
		CHECK(display::mapping::kind(target) == expected);
	}
	target.source = traffic::Source::flarm;
	const struct {
		uint8_t type;
		Kind expected;
	} flarm[] = {{0, Kind::unknown},  {1, Kind::glider},
				 {2, Kind::light},    {3, Kind::helicopter},
				 {7, Kind::glider},   {8, Kind::light},
				 {9, Kind::airliner}, {11, Kind::balloon},
				 {13, Kind::unknown}, {15, Kind::unknown}};
	for (const auto& [type, expected] : flarm) {
		target.flarm_aircraft_type = type;
		CHECK(display::mapping::kind(target) == expected);
	}
	CHECK(conflicts::source(traffic::Source::adsb_other) ==
		  display::Source::adsb_other);
	CHECK(conflicts::source(traffic::Source::adsr) == display::Source::adsr);
	CHECK(conflicts::source(traffic::Source::tisb) == display::Source::tisb);
	CHECK(conflicts::source(traffic::Source::flarm) == display::Source::flarm);
	CHECK(conflicts::source(traffic::Source::other) == display::Source::other);
}
}

int main() {
	return check::run({
		TEST(battery_states),
		TEST(bands),
		TEST(gnss),
		TEST(health_levels),
		TEST(device_facts),
		TEST(placement),
		TEST(altitude_filter),
		TEST(reach),
		TEST(unjudged_offscale_targets_use_reach),
		TEST(engine_closing_overrides_reach_estimate),
		TEST(conflict_bypasses_altitude_filter_and_sorts_first),
		TEST(same_address_different_source),
		TEST(conflict),
		TEST(inside_ring_precedes_outside_alert),
		TEST(listing),
		TEST(ring_band),
		TEST(own_transponder),
		TEST(unknown_altitude),
		TEST(selection),
		TEST(order),
		TEST(tracks),
		TEST(retained_targets_keep_their_own_history),
		TEST(relative_altitude_uses_two_gnss_altitudes),
		TEST(altitude_bands_use_gnss_without_barometer),
		TEST(climbing_into_target),
		TEST(mute_identity_includes_source),
		TEST(equal_distance_sorts_by_address),
		TEST(track_history_off_hides_track),
		TEST(muted_target_hides_track),
		TEST(lost_fix_hides_track),
		TEST(kinds),
	});
}
