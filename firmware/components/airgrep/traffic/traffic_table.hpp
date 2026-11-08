#pragma once

#include <cstddef>
#include <cstdint>
#include "traffic_protocol.hpp"

// Bounded table of targets keyed by address and source, over memory the owner provides.
// No locking, no clock: the owner serializes access and passes the time. Hardware free.
namespace traffic {
	class Table {
	public:
		Table(Target* slots, size_t capacity) : slots_(slots), capacity_(capacity) {
		}

		// Stores the target under its address and source, replacing the previous report. A full
		// table gives up its least recently seen entry.
		void upsert(const Target& target, int64_t now_us);
		// Removes every target not seen for max_age_ms. Returns how many went.
		size_t evict(int64_t now_us, uint32_t max_age_ms);
		// Targets with this address, any source. Returns how many were written, at most count.
		size_t find(uint32_t address, Target* out, size_t count) const;
		size_t size() const { return size_; }
		// Visits every present target.
		template <typename F>
		void each(F f) const {
			for (size_t i = 0; i < capacity_; i++)
				if (present_(i)) f(slots_[i]);
		}

	private:
		bool present_(size_t i) const { return slots_[i].seen_us != 0; }
		Target* slots_;
		size_t capacity_;
		size_t size_ = 0;
	};
}
