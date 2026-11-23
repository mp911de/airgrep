#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace larcfm {
class Position;
class Velocity;
}

namespace conflicts {
enum class Level : uint8_t { other, proximate, advisory, alert };

constexpr bool in_conflict(Level level) { return level != Level::other; }

inline constexpr uint16_t NO_CONFLICT = UINT16_MAX;

// An aircraft at one moment. All flights of one step share the datum of the
// altitude, whichever it is.
struct Flight {
	// Names the aircraft from step to step, so an alert holds while it clears.
	uint64_t id = 0;
	double latitude_deg = 0, longitude_deg = 0;
	float altitude_ft = 0;
	float true_track_deg = 0;
	float groundspeed_kt = 0;
	float vertical_fpm = 0;
	// When this was true, on a clock that all flights share.
	double time_s = 0;

	bool is_finite() const {
		return std::isfinite(latitude_deg) && std::isfinite(longitude_deg) &&
			   std::isfinite(altitude_ft) && std::isfinite(groundspeed_kt) &&
			   std::isfinite(true_track_deg) && std::isfinite(vertical_fpm) &&
			   std::isfinite(time_s);
	}
	// As DAIDALUS takes them.
	larcfm::Position position() const;
	larcfm::Velocity velocity() const;
	std::string identifier() const;
};

struct Conflict {
	Level level = Level::other;
	// Seconds until it begins, 0 once it has, NO_CONFLICT without one.
	uint16_t within_s = NO_CONFLICT;
	bool enters_corrective_volume = false;
};

enum class Guide : uint8_t { track, ground_speed, vertical_speed, altitude };

enum class BandRegion : uint8_t { preventive, corrective, warning, recovery };

// A stretch of what the own ship can change, track, ground speed or altitude,
// that leads into a conflict. from lies below to.
struct Band {
	float from = 0, to = 0;
	BandRegion region = BandRegion::preventive;
};

inline constexpr float kLookAheadS = 180;
inline constexpr float kWellClearNm = 0.66f;
inline constexpr int32_t kWellClearFt = 450;
// Of those that can reach the own ship within the look-ahead, the engine
// alerts on this many, the soonest first, and computes its Bands against the
// soonest of them: these bound what a step costs under a crowded sky.
inline constexpr size_t kMaxAlertTargets = 100, kMaxBandTargets = 25;
// How far the Bands of ground speed and of altitude reach to either side of
// the own ship.
inline constexpr float kSpeedBandSpanKt = 50, kAltitudeBandSpanFt = 2000;

class ConflictEngine {
   public:
	ConflictEngine();
	ConflictEngine(const ConflictEngine&) = delete;
	ConflictEngine& operator=(const ConflictEngine&) = delete;
	ConflictEngine(ConflictEngine&&) = delete;
	ConflictEngine& operator=(ConflictEngine&&) = delete;
	~ConflictEngine();
	void detect(const Flight& own, std::span<const Flight> traffic,
				std::span<Conflict> results);
	// The Bands of one Guide for the step that detect() saw last, against the
	// kMaxBandTargets soonest flights: degrees true, knots, feet per minute or
	// feet.
	size_t bands(Guide guide, std::span<Band> results);
	// A step that fails says no conflict and no Band, and the engine starts
	// anew. How often that happened.
	uint32_t failures() const { return failures_; }

   private:
	struct Engine;
	std::unique_ptr<Engine> alerting_;
	// A second one for the Bands: it sees fewer flights than the first, and
	// meets those of a step when a Band of that step is asked for.
	std::unique_ptr<Engine> banding_;
	Flight own_;
	std::array<Flight, kMaxBandTargets> band_traffic_;
	size_t band_traffic_count_ = 0;
	bool band_traffic_loaded_ = false;
	uint32_t failures_ = 0;
};
}
