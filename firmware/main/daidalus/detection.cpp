#include "detection.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "navigation/units.h"
#include "traffic/traffic_table.hpp"

namespace conflicts {

// Pressure altitude against pressure altitude where the target reports one,
// GNSS against GNSS otherwise. Mixing the two would be off by the weather.
// Nothing when neither pair is known.
std::optional<int32_t> ConflictInput::relative_altitude_ft(
	const traffic::Target& target, bool fix) const {
	int64_t apart = 0;
	if (target.baro_alt_valid && has_pressure_altitude()) {
		apart = target.baro_alt_ft - std::llround(double{barometer.altitude_m} *
												  units::feet_per_metre);
	} else if (target.gnss_alt_valid && fix) {
		apart = int64_t{target.gnss_alt_ft} - adsbee.gnss.altitude_ft;
	} else {
		return std::nullopt;
	}
	return static_cast<int32_t>(
		std::clamp<int64_t>(apart, INT16_MIN, INT16_MAX));
}

bool ConflictInput::has_pressure_altitude() const {
	return barometer.status == Status::ok &&
		   state_usable(barometer.updated_us, now_us) &&
		   std::isfinite(barometer.altitude_m);
}

float ConflictInput::own_altitude_ft(const adsbee::Gnss& own) const {
	return has_pressure_altitude()
			   ? barometer.altitude_m * units::feet_per_metre
			   : static_cast<float>(own.altitude_ft);
}

display::Source source(traffic::Source from) {
	switch (from) {
		case traffic::Source::adsb:
			return display::Source::adsb;
		case traffic::Source::adsb_other:
			return display::Source::adsb_other;
		case traffic::Source::adsr:
			return display::Source::adsr;
		case traffic::Source::tisb:
			return display::Source::tisb;
		case traffic::Source::flarm:
			return display::Source::flarm;
		default:
			return display::Source::other;
	}
}

static bool matches_own_transponder(const traffic::Target& report,
									const adsbee::Gnss& own, float distance_nm,
									int32_t relative_altitude_ft, bool still) {
	constexpr float speed_tolerance_kt = 10, track_tolerance_deg = 20;
	return near_own_ship(distance_nm, relative_altitude_ft) &&
		   report.speed_valid &&
		   std::abs(static_cast<float>(report.speed_kt) - own.speed_kt) <=
			   speed_tolerance_kt &&
		   (still || (report.direction_valid &&
					  std::abs(std::remainder(report.track_deg - own.track_deg,
											  360.0f)) <= track_tolerance_deg));
}

std::optional<size_t> Detector::detect(const ConflictInput& sources,
									   std::span<AlertingResult> results) {
	const adsbee::Gnss& own = sources.adsbee.gnss_at(sources.now_us);
	if (own.fix == adsbee::Fix::none) {
		return std::nullopt;
	}
	const barometer::StateData& barometer = sources.barometer;
	if (!sources.has_pressure_altitude()) {
		reading_us_ = 0;
		climb_fpm_ = 0;
	} else if (!reading_us_) {
		reading_us_ = barometer.updated_us;
		reading_m_ = barometer.altitude_m;
	} else if (const float apart =
				   static_cast<float>(barometer.updated_us - reading_us_) /
				   1e6f;
			   apart >= kClimbSpanS) {
		climb_fpm_ = (barometer.altitude_m - reading_m_) *
					 units::feet_per_metre * 60 / apart;
		reading_us_ = barometer.updated_us;
		reading_m_ = barometer.altitude_m;
	}
	// The traffic stands at its altitude against the own one, so the datum of
	// the own altitude does not matter.
	const conflicts::Flight self{
		.latitude_deg = own.latitude,
		.longitude_deg = own.longitude,
		.altitude_ft = sources.own_altitude_ft(own),
		.true_track_deg = own.track_deg,
		.groundspeed_kt = own.speed_kt,
		.vertical_fpm = climb_fpm_,
		.time_s = static_cast<double>(sources.adsbee.last_gnss_report_us) / 1e6,
	};
	std::array<conflicts::Flight, traffic::kListed> flights;
	std::array<conflicts::Conflict, traffic::kListed> conflicts;
	size_t count = 0;
	for (const traffic::Tracked& tracked : sources.traffic) {
		const traffic::Target& report = tracked.target;
		if (count == flights.size() || count == results.size()) {
			break;
		}
		if (!report.last_seen_us || report.address == sources.own_address) {
			continue;
		}
		float distance_nm = NAN, bearing_deg = NAN;
		if (!report.position_valid ||
			!traffic::relative_position(report, own.latitude, own.longitude,
										distance_nm, bearing_deg) ||
			!std::isfinite(distance_nm) || !std::isfinite(bearing_deg)) {
			continue;
		}
		auto above_ft = sources.relative_altitude_ft(report, true);
		if (!above_ft) {
			continue;
		}
		const traffic::Motion moves = traffic::motion(tracked);
		const bool still =
			moves.speed_valid && moves.speed_kt < minimum_track_speed_kt;
		if (matches_own_transponder(report, own, distance_nm, *above_ft,
									still) ||
			!moves.speed_valid || !(moves.direction_valid || still)) {
			continue;
		}
		results[count].target = {report.address, source(report.source)};
		flights[count++] = {
			.id = uint64_t{report.address} << 8U |
				  static_cast<uint8_t>(report.source),
			.latitude_deg = report.latitude,
			.longitude_deg = report.longitude,
			.altitude_ft = self.altitude_ft + static_cast<float>(*above_ft),
			.true_track_deg = still ? 0 : moves.track_deg,
			.groundspeed_kt = still ? 0 : moves.speed_kt,
			.vertical_fpm = moves.vertical_fpm,
			.time_s = static_cast<double>(report.last_seen_us) / 1e6,
		};
	}
	engine_.detect(self, std::span(flights).first(count),
				   std::span(conflicts).first(count));
	for (size_t i = 0; i < count; i++) {
		results[i].conflict = conflicts[i];
	}
	return count;
}

}
