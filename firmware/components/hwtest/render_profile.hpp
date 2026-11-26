#pragma once

#include <array>
#include <chrono>

#include "hwtest.hpp"

namespace hwtest::render {
class DrawProfile {
   public:
	using Clock = std::chrono::steady_clock;
	void start();
	static void stop();
	void enter(const char* name);
	void leave(const char* name);
	bool print(Output& output, Clock::duration draw);

   private:
	struct Operation {
		const char* name{};
		Clock::duration self{};
		unsigned calls{};
	};
	std::array<Operation, 64> operations_{};
	std::array<Operation*, 48> stack_{};
	size_t used_{}, depth_{};
	Clock::time_point last_{};
	bool invalid_{};
};
}
