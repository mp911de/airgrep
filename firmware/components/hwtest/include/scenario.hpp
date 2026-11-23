#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace scenario {
inline constexpr float kSquareSideNm = 40;

struct Aircraft {
	double latitude_deg = 0, longitude_deg = 0;
	float altitude_ft = 0;
	float true_track_deg = 0;
	float groundspeed_kt = 0;
	float vertical_fpm = 0;
};

class Traffic {
   public:
	// The seed fixes the sequence. Early encounters meet ownship 25 to 40
	// seconds after start.
	Traffic(size_t count, uint32_t seed, double latitude_deg,
			double longitude_deg, size_t early_encounters = 0);

	void step(float seconds);

	// It flies straight and level through the middle and turns back with the
	// others at the edge.
	const Aircraft& own() const { return aircraft_[0]; }
	std::span<const Aircraft> traffic() const {
		return std::span(aircraft_).subspan(1);
	}

   private:
	struct Plan {
		// Degrees per second, to the right when positive. A hold turns half a
		// circle after every straight leg.
		float turn_dps = 0;
		bool holds = false;
		float hold_phase_elapsed_s = 0;
		float target_altitude_ft = 0;
	};

	void reflect_at_boundary(Aircraft& aircraft) const;
	void fly(Aircraft& aircraft, Plan& plan, float seconds) const;

	double latitude_, longitude_;
	float cos_centre_latitude_;
	std::vector<Aircraft> aircraft_;
	std::vector<Plan> plans_;
};
}
