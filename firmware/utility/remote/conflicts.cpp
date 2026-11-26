#include "conflicts.hpp"

#include <cmath>
#include <vector>

#include "radar_publication.hpp"

namespace remote {
namespace {
using TargetKey = std::pair<uint32_t, display::Source>;

std::vector<TargetKey> usable_targets(const conflicts::ConflictInput& input,
									  bool fixed) {
	std::vector<TargetKey> ids;
	if (!fixed) {
		return ids;
	}
	const bool pressure = input.has_pressure_altitude();
	for (const auto& tracked : input.traffic) {
		const auto& t = tracked.target;
		if (!t.last_seen_us || t.address == input.own_address ||
			!t.position_valid || !std::isfinite(t.latitude) ||
			!std::isfinite(t.longitude) ||
			!(t.gnss_alt_valid || (pressure && t.baro_alt_valid))) {
			continue;
		}
		if (!(t.speed_valid &&
			  (t.direction_valid ||
			   t.speed_kt < conflicts::minimum_track_speed_kt))) {
			const auto movement = traffic::motion(tracked);
			if (!movement.speed_valid ||
				!(movement.direction_valid ||
				  movement.speed_kt < conflicts::minimum_track_speed_kt)) {
				continue;
			}
		}
		ids.emplace_back(t.address, conflicts::source(t.source));
	}
	return ids;
}
}

struct Conflicts::ConflictSnapshot {
	adsbee::StateData adsbee;
	barometer::StateData barometer;
	std::vector<traffic::Tracked> traffic;
	int64_t now_us;
	uint32_t own_address;
	bool indicator, fixed;
	std::vector<TargetKey> usable;
};

void Conflicts::alert(const conflicts::ConflictInput& sources, bool indicator) {
	const bool fixed =
		sources.adsbee.gnss_at(sources.now_us).fix != adsbee::Fix::none;
	auto usable = usable_targets(sources, fixed);
	const std::scoped_lock lock(mutex_);
	if (latest_) {
		const bool same_traffic =
			std::ranges::is_permutation(latest_->usable, usable);
		const bool own_changed = latest_->own_address != sources.own_address;
		if (sources.now_us / 1'000'000 == latest_->now_us / 1'000'000 &&
			fixed == latest_->fixed && indicator == latest_->indicator &&
			!own_changed && same_traffic) {
			return;
		}
		const auto removed = [&](const conflicts::AlertingResult& result) {
			return std::ranges::find(usable, TargetKey{result.target.address,
													   result.target.source}) ==
				   usable.end();
		};
		if (own_changed || (!fixed && latest_->fixed) ||
			(!same_traffic &&
			 std::ranges::any_of(
				 std::span(state_.results).first(state_.result_count),
				 removed))) {
			state_.result_count = 0;
			state_.bands = {};
			taken_ = false;
		}
		if (!indicator && state_.indicator) {
			state_.indicator = false;
			state_.bands = {};
			taken_ = false;
		}
	}
	latest_ = std::make_shared<ConflictSnapshot>(
		ConflictSnapshot{sources.adsbee,
						 sources.barometer,
						 {sources.traffic.begin(), sources.traffic.end()},
						 sources.now_us,
						 sources.own_address,
						 indicator,
						 fixed,
						 std::move(usable)});
	loop_.submit([this, snapshot = latest_] { alerting(snapshot); });
}

bool Conflicts::take_latest(conflicts::StateData& state, Change before_copy) {
	const std::scoped_lock lock(mutex_);
	if (taken_) {
		return false;
	}
	if (before_copy) {
		before_copy(state, state_);
	}
	state = state_;
	taken_ = true;
	return true;
}

void Conflicts::publish(bool complete) {
	const std::scoped_lock lock(mutex_);
	if (active_ != latest_ ||
		(!complete && state_.update_count &&
		 state_.engine_failures == work_.engine_failures &&
		 radar::same_conflicts(state_, work_, false))) {
		return;
	}
	state_ = work_;
	taken_ = false;
}

void Conflicts::alerting(std::shared_ptr<const ConflictSnapshot> snapshot) {
	active_ = std::move(snapshot);
	const conflicts::ConflictInput sources{active_->adsbee, active_->traffic,
										   active_->barometer, active_->now_us,
										   active_->own_address};
	const auto skipped = work_.band_updates_skipped;
	const bool follow = monitor_.alert(sources, active_->indicator, work_) ==
						conflicts::BandSchedule::schedule;
	// An unchanged encounter keeps its completed guidance until all three
	// guides are ready.
	publish(!follow && work_.band_updates_skipped == skipped);
	if (follow) {
		loop_.submit([this] {
			// A later alert can run first. The monitor then calculates all
			// guides against that latest detector state on this same loop.
			for (size_t guide = 0; guide < conflicts::kGuided; guide++) {
				monitor_.calculate_bands(guide, work_);
			}
			publish(true);
		});
	}
}
}
