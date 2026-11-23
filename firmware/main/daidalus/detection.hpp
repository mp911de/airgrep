#pragma once

#include <optional>
#include <span>

#include "adsbee/adsbee_protocol.hpp"
#include "baro/bmp580_i2c.hpp"
#include "conflicts/conflicts.hpp"
#include "display/view_state.hpp"
#include "traffic/traffic_protocol.hpp"

namespace conflicts {
using Id = display::TargetId;

struct AlertingResult {
	Id target{};
	conflicts::Conflict conflict;
};

struct ConflictInput {
	const adsbee::StateData& adsbee;
	std::span<const traffic::Tracked> traffic;
	const barometer::StateData& barometer;
	int64_t now_us;
	uint32_t own_address = display::NO_ADDRESS;

	bool has_pressure_altitude() const;
	float own_altitude_ft(const adsbee::Gnss& own) const;
	std::optional<int32_t> relative_altitude_ft(const traffic::Target& target,
												bool fix) const;
};

constexpr bool near_own_ship(float distance_nm, int32_t relative_altitude_ft) {
	return distance_nm <= 0.1f && relative_altitude_ft >= -200 &&
		   relative_altitude_ft <= 200;
}

inline constexpr int16_t minimum_track_speed_kt = 5;

// The own vertical speed comes from two readings of the barometer this far
// apart, in seconds. Closer ones are noise.
inline constexpr float kClimbSpanS = 2.0f;

// Keep the detector across updates for alert hysteresis and climb rate.
class Detector {
   public:
	std::optional<size_t> detect(const ConflictInput& sources,
								 std::span<AlertingResult> results);
	size_t bands(conflicts::Guide guide, std::span<conflicts::Band> found) {
		return engine_.bands(guide, found);
	}
	uint32_t engine_failures() const { return engine_.failures(); }

   private:
	conflicts::ConflictEngine engine_;
	int64_t reading_us_ = 0;
	float reading_m_ = 0;
	float climb_fpm_ = 0;
};

display::Source source(traffic::Source from);
}
