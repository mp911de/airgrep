#pragma once

#include <memory>
#include <mutex>

#include "daidalus/monitor.hpp"
#include "loop.hpp"

namespace remote {
class Conflicts {
   public:
	void alert(const conflicts::ConflictInput& sources, bool indicator);
	// Runs on the reader's thread under the publication lock, before the copy.
	using Change = void (*)(const conflicts::StateData&,
							const conflicts::StateData&);
	bool take_latest(conflicts::StateData& state, Change before_copy = nullptr);
	Loop& loop() { return loop_; }

   private:
	struct ConflictSnapshot;
	void alerting(std::shared_ptr<const ConflictSnapshot> snapshot);
	void publish(bool complete);

	std::mutex mutex_;
	std::shared_ptr<const ConflictSnapshot> latest_;
	conflicts::StateData state_;
	bool taken_ = true;
	// Both belong to the loop. The engine works on them outside the lock of
	// the State, which then takes a copy.
	conflicts::Monitor monitor_;
	conflicts::StateData work_;
	std::shared_ptr<const ConflictSnapshot> active_;
	// The last member: it ends before what its Tasks work on.
	Loop loop_;
};
}
