#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

// Single event loop task on core 0. Events run in submission order, each
// action completes its promise at once, leaves it pending or reports an error.
// Listeners run on the task that completes the promise. Nothing here touches
// the heap: callables hold their capture inline, promises live in fixed slots.

namespace loop {
	enum class Result : uint8_t { done, pending, error };

	enum class Outcome : uint8_t { error, timeout, cancelled };

	union Value {
		uint32_t u;
		int32_t i;
		void* p;
		bool b;
	};

	// The FreeRTOS queue copies events bytewise, so captures must be trivially
	// copyable and fit the inline storage.
	inline constexpr size_t capture_bytes = 16;

	template <typename Sig>
	class Callable;

	template <typename R, typename... Args>
	class Callable<R(Args...)> {
	public:
		Callable() = default;

		template <typename F>
			requires(!std::is_same_v<std::decay_t<F>, Callable>)
		Callable(F f) {
			static_assert(sizeof(F) <= capture_bytes, "capture exceeds 16 bytes");
			static_assert(alignof(F) <= alignof(uint64_t), "capture over-aligned");
			static_assert(std::is_trivially_copyable_v<F>,
						"capture must be trivially copyable");
			std::memcpy(storage_, &f, sizeof f);
			invoke_ = [](const void* s, Args... a) -> R {
				return (*static_cast<const F*>(s))(a...);
			};
		}

		explicit operator bool() const { return invoke_ != nullptr; }
		R operator()(Args... a) const { return invoke_(storage_, a...); }

	private:
		alignas(uint64_t) unsigned char storage_[capture_bytes]{};
		R (*invoke_)(const void*, Args...) = nullptr;
	};

	struct Handle {
		uint32_t generation = 0;
		uint16_t slot = UINT16_MAX;

		bool valid() const { return slot != UINT16_MAX; }
	};

	// An action writes its value and returns done, returns pending and completes
	// through the handle later, or returns error.
	using Action = Callable<Result(Handle, Value&)>;
	using SuccessListener = Callable<void(Value)>;
	using ErrorListener = Callable<void(Outcome)>;

	struct Event {
		const char* name; // static string, logged at debug on run, warn on timeout
		Action action;
		uint32_t timeout_ms = 0; // 0 disables the timeout
	};

	static_assert(std::is_trivially_copyable_v<Event>);
	static_assert(sizeof(Event) <= 40);

	// Creates the loop task. Call before anything submits. Idempotent.
	void start();

	// All return an invalid handle when the queue, the slots or the scheduled
	// array are full. The submitter then frees its own payload.
	bool hsubmit(const Event& event, SuccessListener on_success = {},
				ErrorListener on_error = {});
	bool hsubmit(const char* name, Action action);

	Handle submit(const Event& event, SuccessListener on_success = {},
				ErrorListener on_error = {});
	Handle submit(const char* name, Action action);
	// The timeout of a scheduled event counts from its due time.
	Handle schedule(uint32_t delay_ms, const Event& event,
					SuccessListener on_success = {},
					ErrorListener on_error = {});
	// Runs every period until cancelled or the action returns error. The timeout
	// of the event is ignored.
	Handle schedule_periodic(uint32_t period_ms, const Event& event,
							SuccessListener on_success = {},
							ErrorListener on_error = {});

	// True only when the promise was live and not completed. The error listener
	// runs with Outcome::cancelled on the calling task.
	bool cancel(Handle handle);
	// Completion for actions that returned pending. First caller wins.
	bool complete(Handle handle, Value value);
	bool fail(Handle handle, Outcome outcome);
	// True while the promise exists and nothing has completed it yet.
	bool pending(Handle handle);
} // namespace loop
