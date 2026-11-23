#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

// Single event loop on core 0. Tasks run in submission order, each
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

// The FreeRTOS queue copies tasks bytewise, so captures must be trivially
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
	Callable(F f)
		: invoke_([](const void* s, Args... a) -> R {
			  return (*static_cast<const F*>(s))(a...);
		  }) {
		static_assert(sizeof(F) <= capture_bytes, "capture exceeds 16 bytes");
		static_assert(alignof(F) <= alignof(uint64_t), "capture over-aligned");
		static_assert(std::is_trivially_copyable_v<F>,
					  "capture must be trivially copyable");
		std::memcpy(storage_, static_cast<const void*>(&f), sizeof f);
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

inline constexpr uint32_t no_timeout = 0;

struct Task {
	const char* name{};  // static string for diagnostics
	Action action;
	uint32_t timeout_ms = no_timeout;
};

static_assert(std::is_trivially_copyable_v<Task>);
static_assert(sizeof(Task) <= 40);

// Starts the loop. Call before anything submits. Idempotent.
void start();

// All return an invalid handle when the queue, the slots or the scheduled
// array are full. The submitter then frees its own payload.
bool try_submit(const Task& task, SuccessListener on_success = {},
				ErrorListener on_error = {});
bool try_submit(const char* name, Action action);

Handle submit(const Task& task, SuccessListener on_success = {},
			  ErrorListener on_error = {});
Handle submit(const char* name, Action action);
Handle schedule(uint32_t delay_ms, const Task& task,
				SuccessListener on_success = {}, ErrorListener on_error = {});
struct PeriodicTask {
	const char* name{};
	Action action;
};
Handle schedule_periodic(uint32_t period_ms, const PeriodicTask& task,
						 ErrorListener on_error = {});

// Moves an existing timer no earlier than delay_ms from now, keeping its slot.
// Returns false if the handle no longer has a scheduled action.
bool postpone(Handle handle, uint32_t delay_ms);

// A running one-shot cannot be cancelled until its action returns pending.
// On cancellation the error listener
// runs with Outcome::cancelled on the calling task.
bool cancel(Handle handle);
// Completion for actions that returned pending. First caller wins.
bool complete(Handle handle, Value value);
bool fail(Handle handle, Outcome outcome);
bool pending(Handle handle);
}
