#include "conflicts/traffic_filter.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "conflicts/limits.hpp"
#include "navigation/units.h"

namespace conflicts {
namespace {

float min_time_to_violation_s(const Flight& own, const Flight& flight,
							  float east_scale) {
	const float north_nm =
		static_cast<float>(flight.latitude_deg - own.latitude_deg) *
		units::nm_per_degree_latitude;
	const float east_nm =
		static_cast<float>(
			std::remainder(flight.longitude_deg - own.longitude_deg, 360.0)) *
		units::nm_per_degree_latitude * east_scale;
	const float apart_nm = std::hypot(north_nm, east_nm) - kWellClearNm;
	const float apart_ft =
		std::fabs(flight.altitude_ft - own.altitude_ft) - kWellClearFt;
	const float closing_kt =
		own.groundspeed_kt + kSpeedBandSpanKt + flight.groundspeed_kt;
	const float closing_fpm =
		kMaxVerticalSpeedFpm + std::fabs(flight.vertical_fpm);
	return std::max(
		{0.0f, apart_nm / closing_kt * 3600, apart_ft / closing_fpm * 60});
}
}

size_t filter_traffic(const Flight& own, std::span<const Flight> traffic,
					  std::span<uint16_t> ranked) {
	if (!own.is_finite()) {
		return 0;
	}
	std::array<float, kMaxAlertTargets> times_s{};
	const size_t capacity = std::min(ranked.size(), times_s.size());
	size_t count = 0;
	const float east_scale = std::cos(static_cast<float>(
		own.latitude_deg * units::radians_per_degree<double>));
	for (size_t i = 0; i < traffic.size() && i <= UINT16_MAX; i++) {
		if (!traffic[i].is_finite()) {
			continue;
		}
		const float time_s =
			min_time_to_violation_s(own, traffic[i], east_scale);
		if (!(time_s <= kLookAheadS)) {
			continue;
		}
		size_t slot = count;
		while (slot > 0 && times_s[slot - 1] > time_s) {
			slot--;
		}
		if (slot == capacity) {
			continue;
		}
		for (size_t later = std::min(count, capacity - 1); later > slot;
			 later--) {
			times_s[later] = times_s[later - 1];
			ranked[later] = ranked[later - 1];
		}
		times_s[slot] = time_s;
		ranked[slot] = static_cast<uint16_t>(i);
		count = std::min(count + 1, capacity);
	}
	return count;
}
}
