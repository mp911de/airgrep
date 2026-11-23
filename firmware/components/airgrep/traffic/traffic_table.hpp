#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "traffic_protocol.hpp"

// Bounded table of targets keyed by address and source, over memory the owner
// provides. No locking, no clock: the owner serializes access and passes the
// time. Hardware free.
namespace traffic {
struct Reference {
	float latitude, longitude;
};

inline constexpr size_t kListed = 60;

class Table {
   public:
	// history is room for kHistorySeconds positions per slot, or null for a
	// table that keeps no past.
	Table(Target* slots, size_t capacity, Position* history = nullptr,
		  size_t history_head = 0)
		: slots_(slots),
		  capacity_(capacity),
		  history_(history),
		  head_(history_head) {}
	size_t history_head() const { return head_; }

	// Stores the target under its address and source, replacing the previous
	// report. A full table gives up its least recently seen entry.
	void upsert(const Target& target, int64_t now_us);
	// Rejects invalid batch sizes before changing any slot.
	bool upsert_batch(const Target* targets, size_t count, int64_t now_us);
	bool write_bytes(size_t offset, std::span<const uint8_t> bytes);
	size_t evict(int64_t now_us, uint32_t max_age_ms);
	size_t find(uint32_t address, Target* out, size_t count) const;
	size_t size() const;
	// Notes where every target is now. The owner calls it once a second.
	void sample(int64_t now_us);
	// The targets nearest to the reference first, each with its past, at
	// most limit and at most kListed. A target without a position comes
	// after those that have one. Without a reference the one heard last comes
	// first. Returns how many were written.
	size_t nearest(const Reference* reference, Tracked* out,
				   size_t limit) const;
	template <typename F>
	void each(F f) const {
		for (size_t i = 0; i < capacity_; i++) {
			if (present_(i)) {
				f(slots_[i]);
			}
		}
	}

   private:
	bool present_(size_t i) const { return slots_[i].last_seen_us != 0; }
	// The past of a slot is a ring. Every slot is sampled at the same moment,
	// so one head serves them all.
	Position& past_(size_t slot, size_t age) const {
		return history_[slot * kHistorySeconds +
						(head_ + kHistorySeconds - age) % kHistorySeconds];
	}
	void replace_slot(size_t slot, const Target& target);
	void forget_(size_t slot);
	Target* slots_;
	size_t capacity_;
	Position* history_;
	size_t head_ = 0;
};
}
