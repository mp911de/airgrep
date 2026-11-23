#pragma once

#include "freertos/FreeRTOS.h"

template <typename State>
class LockedState {
   public:
	template <typename F>
	static void update(F f) {
		State& state = storage();
		const Guard guard;
		f(state);
	}

	static State copy() {
		State result;
		update([&](const State& state) { result = state; });
		return result;
	}

   private:
	inline static portMUX_TYPE mutex_ = portMUX_INITIALIZER_UNLOCKED;
	struct Guard {
		Guard() { portENTER_CRITICAL(&mutex_); }
		~Guard() { portEXIT_CRITICAL(&mutex_); }
		Guard(const Guard&) = delete;
		Guard& operator=(const Guard&) = delete;
		Guard(Guard&&) = delete;
		Guard& operator=(Guard&&) = delete;
	};

	static State& storage() {
		static State state;
		return state;
	}
};
