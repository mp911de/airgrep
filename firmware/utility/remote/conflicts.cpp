#include "conflicts.hpp"

#include <vector>

namespace remote {

struct Conflicts::ConflictSnapshot {
	adsbee::StateData adsbee;
	barometer::StateData barometer;
	std::vector<traffic::Tracked> traffic;
	int64_t now_us;
	uint32_t own_address;
};

void Conflicts::alert(const conflicts::ConflictInput& sources, bool indicator) {
	const std::scoped_lock lock(mutex_);
	const int64_t sample_second = sources.now_us / 1'000'000;
	if (sample_second == last_sample_second_) {
		return;
	}
	last_sample_second_ = sample_second;
	loop_.submit([this,
				  snapshot = ConflictSnapshot{sources.adsbee,
											  sources.barometer,
											  {sources.traffic.begin(),
											   sources.traffic.end()},
											  sources.now_us,
											  sources.own_address},
				  indicator] { alerting(snapshot, indicator); });
}

bool Conflicts::take_latest(conflicts::StateData& state) {
	const std::scoped_lock lock(mutex_);
	if (taken_) {
		return false;
	}
	state = state_;
	taken_ = true;
	return true;
}

void Conflicts::publish() {
	const std::scoped_lock lock(mutex_);
	state_ = work_;
	taken_ = false;
}

void Conflicts::alerting(const ConflictSnapshot& snapshot, bool indicator) {
	const conflicts::ConflictInput sources{snapshot.adsbee, snapshot.traffic,
										   snapshot.barometer, snapshot.now_us,
										   snapshot.own_address};
	const bool follow = monitor_.alert(sources, indicator, work_) ==
						conflicts::BandSchedule::schedule;
	publish();
	for (size_t guide = 0; follow && guide < conflicts::kGuided; guide++) {
		loop_.submit([this, guide] {
			monitor_.calculate_bands(guide, work_);
			publish();
		});
	}
}
}
