#pragma once

namespace navigation {
struct RangeBearing {
	float distance_nm;
	float bearing_deg;
};

// Initial true bearing from the reference position to the target.
RangeBearing great_circle(float ref_latitude, float ref_longitude,
						  float latitude, float longitude);
}
