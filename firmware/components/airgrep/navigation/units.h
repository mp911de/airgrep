#pragma once

#include <math.h>

#define AIRGREP_RADIANS_PER_DEGREE (M_PI / 180.0)

#ifdef __cplusplus

namespace units {
inline constexpr double metres_per_foot = 0.3048;
inline constexpr double metres_per_nautical_mile = 1852.0;
inline constexpr double metres_per_second_per_knot =
	metres_per_nautical_mile / 3600.0;
inline constexpr float feet_per_metre =
	static_cast<float>(1.0 / metres_per_foot);
inline constexpr float knots_per_mps =
	static_cast<float>(1.0 / metres_per_second_per_knot);
inline constexpr double metres_per_second_per_fpm = metres_per_foot / 60.0;
// Local tangent-plane approximation for latitude. Longitude also scales by
// cos(latitude).
inline constexpr float nm_per_degree_latitude = 60.0f;
template <typename T>
inline constexpr T radians_per_degree =
	static_cast<T>(AIRGREP_RADIANS_PER_DEGREE);
}
#else
static const float radians_per_degree = (float)AIRGREP_RADIANS_PER_DEGREE;
#endif

#undef AIRGREP_RADIANS_PER_DEGREE
