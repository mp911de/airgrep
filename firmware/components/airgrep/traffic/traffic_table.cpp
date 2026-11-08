#include "traffic_table.hpp"

namespace traffic {
	void Table::upsert(const Target& target, int64_t now_us) {
		size_t slot = capacity_, oldest = capacity_;
		for (size_t i = 0; i < capacity_; i++) {
			if (!present_(i)) {
				if (slot == capacity_) slot = i;
				continue;
			}
			if (slots_[i].address == target.address && slots_[i].source == target.
				source) {
				slot = i;
				oldest = capacity_;
				break;
			}
			if (oldest == capacity_ || slots_[i].seen_us<slots_[oldest].seen_us) oldest =
				i;
		}
		if (slot == capacity_) slot = oldest; // full, the least recently seen target goes
		else if (!present_(slot)) size_++;
		slots_[slot] = target;
		slots_[slot].seen_us = now_us ? now_us : 1;
	}

	size_t Table::evict(int64_t now_us, uint32_t max_age_ms) {
		size_t gone = 0;
		for (size_t i = 0; i < capacity_; i++) {
			if (!present_(i) || now_us - slots_[i].seen_us < int64_t(max_age_ms) * 1000)
				continue;
			slots_[i].seen_us = 0;
			size_--;
			gone++;
		}
		return gone;
	}

	size_t Table::find(uint32_t address, Target* out, size_t count) const {
		size_t n = 0;
		for (size_t i = 0; i < capacity_ && n < count; i++)
			if (present_(i) && slots_[i].address == address) out[n++] = slots_[i];
		return n;
	}
}
