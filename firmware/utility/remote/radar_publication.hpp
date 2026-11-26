#pragma once

#include <algorithm>
#include <tuple>

#include "daidalus/monitor.hpp"
#include "device/device_state.hpp"
#include "device/inspect.hpp"

namespace remote::radar {

inline bool same_traffic(const device::TrafficListing& before,
						 const device::TrafficListing& after, bool history) {
	const auto report = [](const traffic::Target& t) {
		return std::tuple(
			t.last_seen_us != 0, t.address, t.source, t.position_valid,
			t.latitude, t.longitude, t.baro_alt_valid, t.baro_alt_ft,
			t.gnss_alt_valid, t.gnss_alt_ft, t.direction_valid, t.track_deg,
			t.speed_valid, t.speed_kt, t.baro_rate_valid, t.baro_rate_fpm,
			t.gnss_rate_valid, t.gnss_rate_fpm, t.mode_s_category,
			t.flarm_aircraft_type, t.flarm_turn_rate_dps);
	};
	const auto trail = [](const traffic::Position& p) {
		return std::tie(p.sample, p.latitude, p.longitude);
	};
	return std::ranges::equal(
		before.nearest_targets, after.nearest_targets,
		[&](const traffic::Tracked& a, const traffic::Tracked& b) {
			return report(a.target) == report(b.target) &&
				   (!history ||
					std::ranges::equal(a.past, b.past, {}, trail, trail));
		});
}

inline bool same_navigation(const device::DeviceState& before,
							const device::DeviceState& after,
							bool has_geometry) {
	const auto navigation = [has_geometry](const device::DeviceState& picture) {
		const int64_t now = device::end_of_uptime_second_us(picture.hello);
		const auto& own = picture.states.adsbee.gnss_at(now);
		const bool fix = own.fix != adsbee::Fix::none;
		const bool used = fix && has_geometry;
		const conflicts::ConflictInput input{
			picture.states.adsbee, {}, picture.states.barometer, now};
		const bool pressure = used && input.has_pressure_altitude();
		return std::tuple(fix, used ? own.latitude : 0,
						  used ? own.longitude : 0, used ? own.track_deg : 0,
						  used ? own.speed_kt : 0, used ? own.altitude_ft : 0,
						  pressure,
						  pressure ? picture.states.barometer.altitude_m : 0);
	};
	return navigation(before) == navigation(after);
}

inline bool same_conflicts(const conflicts::StateData& before,
						   const conflicts::StateData& after, bool show_bands) {
	const auto threat = [](const conflicts::AlertingResult& result) {
		return std::tie(result.target.address, result.target.source,
						result.conflict.level,
						result.conflict.enters_corrective_volume);
	};
	if (!std::ranges::is_permutation(
			std::span(before.results).first(before.result_count),
			std::span(after.results).first(after.result_count), {}, threat,
			threat)) {
		return false;
	}
	const auto band = [](const conflicts::Band& b) {
		return std::tie(b.from, b.to, b.region);
	};
	return !show_bands ||
		   std::ranges::equal(
			   before.bands, after.bands,
			   [&](const conflicts::Bands& a, const conflicts::Bands& b) {
				   return std::ranges::equal(a.intervals(), b.intervals(), {},
											 band, band);
			   });
}
}
