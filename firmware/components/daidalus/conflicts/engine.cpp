#include <algorithm>
#include <bitset>
#include <cmath>
#include <concepts>
#include <exception>
#include <iterator>
#include <string>

#include "Daidalus.h"
#include "conflicts/conflicts.hpp"
#include "conflicts/limits.hpp"
#include "conflicts/traffic_filter.hpp"
#include "navigation/units.h"

namespace conflicts {
namespace {
using larcfm::Daidalus;
using larcfm::real;

// DAIDALUS computes in radians, metres and seconds. One of ours in its.
constexpr double kDeg = units::radians_per_degree<double>;
constexpr double kFt = units::metres_per_foot;
constexpr double kKt = units::metres_per_second_per_knot;
constexpr double kFpm = units::metres_per_second_per_fpm;

real to_si(std::floating_point auto value, double unit) {
	return static_cast<real>(static_cast<double>(value) * unit);
}

float from_si(real value, double unit) {
	return static_cast<float>(static_cast<double>(value) / unit);
}

// DAIDALUS counts its alert levels from 1: preventive, corrective, warning.
constexpr Level kLevels[] = {Level::other, Level::proximate, Level::advisory,
							 Level::alert};

struct GuideBands {
	int (Daidalus::*length)();
	larcfm::Interval (Daidalus::*stretch)(int);
	larcfm::BandsRegion::Region (Daidalus::*region)(int);
	double unit;
};

// In the order of Guide.
constexpr GuideBands kGuideBands[] = {
	{&Daidalus::horizontalDirectionBandsLength,
	 &Daidalus::horizontalDirectionIntervalAt,
	 &Daidalus::horizontalDirectionRegionAt, kDeg},
	{&Daidalus::horizontalSpeedBandsLength,
	 &Daidalus::horizontalSpeedIntervalAt, &Daidalus::horizontalSpeedRegionAt,
	 kKt},
	{&Daidalus::verticalSpeedBandsLength, &Daidalus::verticalSpeedIntervalAt,
	 &Daidalus::verticalSpeedRegionAt, kFpm},
	{&Daidalus::altitudeBandsLength, &Daidalus::altitudeIntervalAt,
	 &Daidalus::altitudeRegionAt, kFt},
};

bool to_band_region(larcfm::BandsRegion::Region from, BandRegion& region) {
	using larcfm::BandsRegion;
	switch (from) {
		case BandsRegion::FAR:
			region = BandRegion::preventive;
			return true;
		case BandsRegion::MID:
			region = BandRegion::corrective;
			return true;
		case BandsRegion::NEAR:
			region = BandRegion::warning;
			return true;
		case BandsRegion::RECOVERY:
			region = BandRegion::recovery;
			return true;
		default:
			return false;
	}
}
}

larcfm::Position Flight::position() const {
	return larcfm::Position::mkLatLonAlt(to_si(latitude_deg, kDeg),
										 to_si(longitude_deg, kDeg),
										 to_si(altitude_ft, kFt));
}

larcfm::Velocity Flight::velocity() const {
	return larcfm::Velocity::mkTrkGsVs(to_si(true_track_deg, kDeg),
									   to_si(groundspeed_kt, kKt),
									   to_si(vertical_fpm, kFpm));
}

std::string Flight::identifier() const { return std::to_string(id); }

struct ConflictEngine::Engine {
	Engine() {
		daidalus.set_DO_365B();
		// DO-365B gives its preventive level, the first, no region.
		larcfm::Alerter alerter = daidalus.getAlerterAt(1);
		larcfm::AlertThresholds preventive = alerter.getLevel(1);
		preventive.setRegion(larcfm::BandsRegion::FAR);
		alerter.setLevel(1, preventive);
		daidalus.clearAlerters();
		daidalus.addAlerter(alerter);

		daidalus.setHorizontalDirectionStep(to_si(kTrackStepDeg, kDeg));
		daidalus.setHorizontalSpeedStep(to_si(kSpeedStepKt, kKt));
		daidalus.setMinHorizontalSpeed(to_si(kMinSpeedKt, kKt));
		daidalus.setMaxHorizontalSpeed(to_si(kMaxSpeedKt, kKt));
		daidalus.setBelowRelativeHorizontalSpeed(to_si(kSpeedBandSpanKt, kKt));
		daidalus.setAboveRelativeHorizontalSpeed(to_si(kSpeedBandSpanKt, kKt));
		daidalus.setVerticalSpeedStep(to_si(kVerticalStepFpm, kFpm));
		daidalus.setMinVerticalSpeed(to_si(-kMaxVerticalSpeedFpm, kFpm));
		daidalus.setMaxVerticalSpeed(to_si(kMaxVerticalSpeedFpm, kFpm));
		daidalus.setAltitudeStep(to_si(kAltitudeStepFt, kFt));
		daidalus.setMinAltitude(to_si(kMinAltitudeFt, kFt));
		daidalus.setBelowRelativeAltitude(to_si(kAltitudeBandSpanFt, kFt));
		daidalus.setAboveRelativeAltitude(to_si(kAltitudeBandSpanFt, kFt));
		daidalus.setRecoveryStabilityTime(to_si(kRecoveryStableS, 1));
	}

	// Loads one step: the own ship and count flights, each from flight_at.
	// on_loaded sees each flight DAIDALUS took, with its index there.
	// setOwnshipState clears the traffic list. Departed IDs are removed
	// first, so their alert history is forgotten while continuing flights
	// keep theirs.
	void load(const Flight& own, size_t count, auto flight_at, auto on_loaded) {
		std::bitset<kMaxAlertTargets> retained;
		for (size_t n = 0; n < count; ++n) {
			const Flight& flight = flight_at(n);
			const int index = daidalus.aircraftIndex(flight.identifier());
			if (index > 0 && !flight.position().isInvalid() &&
				!flight.velocity().isInvalid()) {
				retained.set(static_cast<size_t>(index - 1));
			}
		}
		for (int i = daidalus.lastTrafficIndex(); i > 0; --i) {
			if (!retained.test(static_cast<size_t>(i - 1))) {
				daidalus.removeTrafficAircraft(
					daidalus.getAircraftStateAt(i).getId());
			}
		}
		daidalus.setOwnshipState("own", own.position(), own.velocity(),
								 static_cast<real>(own.time_s));
		for (size_t n = 0; n < count; ++n) {
			const Flight& flight = flight_at(n);
			const int index = daidalus.addTrafficState(
				flight.identifier(), flight.position(), flight.velocity(),
				static_cast<real>(flight.time_s));
			if (index > 0) {
				on_loaded(n, index);
			}
		}
	}

	Daidalus daidalus;
};

ConflictEngine::ConflictEngine()
	: alerting_(std::make_unique<Engine>()),
	  banding_(std::make_unique<Engine>()) {}
ConflictEngine::~ConflictEngine() = default;

void ConflictEngine::detect(const Flight& own, std::span<const Flight> traffic,
							std::span<Conflict> results) {
	std::ranges::fill(results, Conflict{});
	std::array<uint16_t, kMaxAlertTargets> ranked{};
	const size_t count = filter_traffic(
		own, traffic.first(std::min(traffic.size(), results.size())), ranked);
	const auto flight_at = [&](size_t n) -> const Flight& {
		return traffic[ranked[n]];
	};
	own_ = own;
	band_traffic_count_ = std::min(count, band_traffic_.size());
	for (size_t n = 0; n < band_traffic_count_; n++) {
		band_traffic_[n] = flight_at(n);
	}
	band_traffic_loaded_ = false;
	Daidalus& daidalus = alerting_->daidalus;
	bool failed = false;
	try {
		alerting_->load(own, count, flight_at, [&](size_t n, int index) {
			Conflict& conflict = results[ranked[n]];
			conflict.enters_corrective_volume =
				std::isfinite(daidalus.timeToCorrectiveVolume(index));
			const int level = daidalus.alertLevel(index);
			if (level <= 0) {
				return;
			}
			const real begins =
				daidalus.violationOfAlertThresholds(index, level).getTimeIn();
			conflict.level =
				kLevels[std::min<int>(level, std::size(kLevels) - 1)];
			conflict.within_s = std::isfinite(begins)
									? static_cast<uint16_t>(std::clamp<real>(
										  begins, 0, NO_CONFLICT - 1))
									: 0;
		});
		failed = daidalus.hasError();
	} catch (const std::exception&) {
		failed = true;
	}
	if (!failed) {
		return;
	}
	std::ranges::fill(results, Conflict{});
	alerting_ = std::make_unique<Engine>();
	failures_++;
}

size_t ConflictEngine::bands(Guide guide, std::span<Band> results) {
	Daidalus& daidalus = banding_->daidalus;
	const GuideBands& of = kGuideBands[static_cast<size_t>(guide)];
	size_t count = 0;
	bool failed = false;
	try {
		if (!band_traffic_loaded_) {
			banding_->load(
				own_, band_traffic_count_,
				[&](size_t n) -> const Flight& { return band_traffic_[n]; },
				[](size_t, int) {});
			band_traffic_loaded_ = true;
		}
		const int length = (daidalus.*of.length)();
		for (int i = 0; i < length && count < results.size(); i++) {
			const larcfm::Interval stretch = (daidalus.*of.stretch)(i);
			Band& band = results[count];
			if (to_band_region((daidalus.*of.region)(i), band.region)) {
				band.from = from_si(stretch.low, of.unit);
				band.to = from_si(stretch.up, of.unit);
				count++;
			}
		}
		failed = daidalus.hasError();
	} catch (const std::exception&) {
		failed = true;
	}
	if (!failed) {
		return count;
	}
	banding_ = std::make_unique<Engine>();
	band_traffic_loaded_ = false;
	failures_++;
	return 0;
}
}
