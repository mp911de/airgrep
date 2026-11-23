#include "great_circle.hpp"

#include <cmath>

#include "navigation/units.h"

namespace navigation {
constexpr float kEarthRadiusNm = 3440.065f;

RangeBearing great_circle(float ref_latitude, float ref_longitude,
						  float latitude, float longitude) {
	const float lat1 = ref_latitude * units::radians_per_degree<float>;
	const float lat2 = latitude * units::radians_per_degree<float>;
	const float dlat = lat2 - lat1;
	const float dlon =
		(longitude - ref_longitude) * units::radians_per_degree<float>;
	const float a = std::sin(dlat / 2) * std::sin(dlat / 2) +
					std::cos(lat1) * std::cos(lat2) * std::sin(dlon / 2) *
						std::sin(dlon / 2);
	const float distance_nm =
		2 * std::atan2(std::sqrt(a), std::sqrt(1 - a)) * kEarthRadiusNm;
	const float y = std::sin(dlon) * std::cos(lat2);
	const float x = std::cos(lat1) * std::sin(lat2) -
					std::sin(lat1) * std::cos(lat2) * std::cos(dlon);
	float bearing_deg = std::atan2(y, x) / units::radians_per_degree<float>;
	if (bearing_deg < 0) {
		bearing_deg += 360;
	}
	return {distance_nm, bearing_deg};
}
}
