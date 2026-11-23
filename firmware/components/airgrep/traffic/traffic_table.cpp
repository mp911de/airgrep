#include "traffic_table.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>

namespace traffic {
namespace {
constexpr int64_t kFreshPositionMaxAgeUs = 1000000;
struct RankedTarget {
	size_t slot{};
	std::optional<float> distance_nm;
	bool precedes(const RankedTarget& other, const Target* targets) const {
		if (distance_nm && other.distance_nm) {
			return *distance_nm < *other.distance_nm;
		}
		if (distance_nm || other.distance_nm) {
			return distance_nm.has_value();
		}
		return targets[slot].last_seen_us > targets[other.slot].last_seen_us;
	}
};
}

void Table::upsert(const Target& target, int64_t now_us) {
	if (!capacity_) {
		return;
	}
	size_t slot = capacity_, oldest = capacity_;
	for (size_t i = 0; i < capacity_; i++) {
		if (!present_(i)) {
			if (slot == capacity_) {
				slot = i;
			}
			continue;
		}
		if (slots_[i].address == target.address &&
			slots_[i].source == target.source) {
			slot = i;
			oldest = capacity_;
			break;
		}
		if (oldest == capacity_ ||
			slots_[i].last_seen_us < slots_[oldest].last_seen_us) {
			oldest = i;
		}
	}
	if (slot == capacity_) {
		slot = oldest;
	}
	Target received = target;
	received.last_seen_us = now_us ? now_us : 1;
	replace_slot(slot, received);
}

void Table::replace_slot(size_t slot, const Target& target) {
	const Target& previous = slots_[slot];
	if (!previous.last_seen_us || previous.address != target.address ||
		previous.source != target.source || !target.last_seen_us) {
		forget_(slot);
	}
	std::memcpy(&slots_[slot], &target, sizeof target);
}

bool Table::write_bytes(size_t offset, std::span<const uint8_t> bytes) {
	const size_t length = capacity_ * sizeof(Target);
	if (offset > length || bytes.size() > length - offset) {
		return false;
	}
	while (!bytes.empty()) {
		const size_t slot = offset / sizeof(Target);
		const size_t within = offset % sizeof(Target);
		const size_t count = std::min(bytes.size(), sizeof(Target) - within);
		Target target;
		std::memcpy(&target, &slots_[slot], sizeof target);
		std::memcpy(reinterpret_cast<uint8_t*>(&target) + within, bytes.data(),
					count);
		replace_slot(slot, target);
		bytes = bytes.subspan(count);
		offset += count;
	}
	return true;
}

bool Table::upsert_batch(const Target* targets, size_t count, int64_t now_us) {
	if (!targets || !count || count > 32 || !capacity_) {
		return false;
	}
	for (size_t i = 0; i < count; ++i) {
		upsert(targets[i], now_us);
	}
	return true;
}

size_t Table::size() const {
	size_t count = 0;
	for (size_t i = 0; i < capacity_; ++i) {
		count += present_(i);
	}
	return count;
}

size_t Table::evict(int64_t now_us, uint32_t max_age_ms) {
	size_t gone = 0;
	for (size_t i = 0; i < capacity_; i++) {
		if (!present_(i) || now_us - slots_[i].last_seen_us <
								static_cast<int64_t>(max_age_ms) * 1000) {
			continue;
		}
		slots_[i].last_seen_us = 0;
		gone++;
	}
	return gone;
}

void Table::forget_(size_t slot) {
	for (size_t age = 0; history_ && age < kHistorySeconds; age++) {
		past_(slot, age) = {};
	}
}

void Table::sample(int64_t now_us) {
	if (!history_) {
		return;
	}
	head_ = (head_ + 1) % kHistorySeconds;
	for (size_t i = 0; i < capacity_; i++) {
		const Target& target = slots_[i];
		Position& now = past_(i, 0);
		now = {};
		if (!present_(i) || !target.position_valid) {
			continue;
		}
		now.latitude = target.latitude;
		now.longitude = target.longitude;
		if (target.baro_alt_valid) {
			now.altitude_ft = target.baro_alt_ft;
			now.altitude_datum = AltitudeDatum::barometric;
		} else if (target.gnss_alt_valid) {
			now.altitude_ft = target.gnss_alt_ft;
			now.altitude_datum = AltitudeDatum::gnss;
		} else {
			now.altitude_datum = AltitudeDatum::unavailable;
		}
		now.speed_kt = target.speed_kt;
		now.sample = now_us - target.last_seen_us <= kFreshPositionMaxAgeUs
						 ? Sample::fresh
						 : Sample::held;
	}
}

size_t Table::nearest(const Reference* reference, Tracked* out,
					  size_t limit) const {
	limit = limit < kListed ? limit : kListed;
	RankedTarget nearest[kListed];
	size_t count = 0;
	for (size_t i = 0; i < capacity_; i++) {
		if (!present_(i)) {
			continue;
		}
		RankedTarget candidate{i, std::nullopt};
		float distance = NAN, bearing = NAN;
		if (reference &&
			relative_position(slots_[i], reference->latitude,
							  reference->longitude, distance, bearing) &&
			distance >= 0) {
			candidate.distance_nm = distance;
		}
		size_t at = count;
		while (at > 0 && candidate.precedes(nearest[at - 1], slots_)) {
			at--;
		}
		if (at >= limit) {
			continue;
		}
		count = count < limit ? count + 1 : count;
		for (size_t move = count - 1; move > at; move--) {
			nearest[move] = nearest[move - 1];
		}
		nearest[at] = candidate;
	}
	for (size_t n = 0; n < count; n++) {
		out[n].target = slots_[nearest[n].slot];
		for (size_t age = 0; age < kHistorySeconds; age++) {
			out[n].past[age] =
				history_ ? past_(nearest[n].slot, age) : Position{};
		}
	}
	return count;
}

size_t Table::find(uint32_t address, Target* out, size_t count) const {
	size_t n = 0;
	for (size_t i = 0; i < capacity_ && n < count; i++) {
		if (present_(i) && slots_[i].address == address) {
			out[n++] = slots_[i];
		}
	}
	return n;
}
}
