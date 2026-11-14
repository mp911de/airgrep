#include "traffic_table.hpp"

namespace traffic {
void Table::upsert(const Target& target, int64_t now_us) {
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
		if (oldest == capacity_ || slots_[i].seen_us < slots_[oldest].seen_us) {
			oldest = i;
		}
	}
	if (slot == capacity_) {
		slot = oldest;  // full, the least recently seen target goes
	}
	slots_[slot] = target;
	slots_[slot].seen_us = now_us ? now_us : 1;
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
	// Raw State writes can change occupancy without going through upsert().
	size_t count = 0;
	for (size_t i = 0; i < capacity_; ++i) {
		count += present_(i);
	}
	return count;
}

size_t Table::evict(int64_t now_us, uint32_t max_age_ms) {
	size_t gone = 0;
	for (size_t i = 0; i < capacity_; i++) {
		if (!present_(i) ||
			now_us - slots_[i].seen_us < int64_t(max_age_ms) * 1000) {
			continue;
		}
		slots_[i].seen_us = 0;
		gone++;
	}
	return gone;
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
}  // namespace traffic
