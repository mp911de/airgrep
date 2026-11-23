#pragma once

#include <mutex>

#include "daidalus/monitor.hpp"
#include "loop.hpp"

namespace remote {
class Conflicts {
   public:
	void alert(const conflicts::ConflictInput& sources, bool indicator);
	bool take_latest(conflicts::StateData& state);
	Loop& loop() { return loop_; }

   private:
	struct ConflictSnapshot;
	void alerting(const ConflictSnapshot& snapshot, bool indicator);
	void publish();

	std::mutex mutex_;
	int64_t last_sample_second_ = -1;
	conflicts::StateData state_;
	bool taken_ = true;
	// Both belong to the loop. The engine works on them outside the lock of
	// the State, which then takes a copy.
	conflicts::Monitor monitor_;
	conflicts::StateData work_;
	// The last member: it ends before what its Tasks work on.
	Loop loop_;
};
}
