#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace cpu {
// Each reader measures from its previous sample, or from boot on its first.
template <std::size_t Cores>
class Window {
   public:
	struct Load {
		std::array<unsigned, Cores> cores{};
		unsigned overall = 0, busiest = 0;
	};

	Load sample(uint64_t now, auto idle_counter) {
		Load load;
		const uint64_t elapsed = now - time_;
		for (std::size_t core = 0; core < Cores; ++core) {
			const uint64_t idle = idle_counter(core);
			const uint64_t delta = idle - idle_[core];
			const unsigned percent =
				delta < elapsed ? (elapsed - delta) * 100 / elapsed : 0;
			load.cores[core] = percent;
			load.overall += percent;
			load.busiest = std::max(load.busiest, percent);
			idle_[core] = idle;
		}
		load.overall /= Cores;
		time_ = now;
		return load;
	}

   private:
	std::array<uint64_t, Cores> idle_{};
	uint64_t time_ = 0;
};
}
