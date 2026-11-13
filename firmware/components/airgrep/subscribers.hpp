#pragma once

#include <cstddef>

#include "freertos/FreeRTOS.h"

// A fixed list of handlers. Any task adds, any task notifies. Handlers run
// outside the lock, so a handler may subscribe.
template <typename Handler, size_t Capacity>
class Subscribers {
   public:
	bool add(Handler handler, void* context) {
		if (!handler) {
			return false;
		}
		portENTER_CRITICAL(&mutex_);
		const bool free = count_ < Capacity;
		if (free) {
			entries_[count_++] = {handler, context};
		}
		portEXIT_CRITICAL(&mutex_);
		return free;
	}

	// Calls each handler with the arguments, then its context.
	template <typename... Args>
	void notify(const Args&... args) {
		Entry current[Capacity];
		portENTER_CRITICAL(&mutex_);
		const size_t count = count_;
		for (size_t i = 0; i < count; i++) {
			current[i] = entries_[i];
		}
		portEXIT_CRITICAL(&mutex_);
		for (size_t i = 0; i < count; i++) {
			current[i].handler(args..., current[i].context);
		}
	}

   private:
	struct Entry {
		Handler handler;
		void* context;
	};

	Entry entries_[Capacity]{};
	size_t count_ = 0;
	portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
};
