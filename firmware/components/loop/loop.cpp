#include "loop.hpp"

#include <algorithm>
#include <atomic>

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace loop {
namespace {
const char* TAG = "loop";
constexpr uint16_t SLOTS = CONFIG_AIRGREP_LOOP_QUEUE_DEPTH;
constexpr size_t SCHEDULED = CONFIG_AIRGREP_LOOP_SCHEDULED;
constexpr int64_t TICK_US = int64_t{CONFIG_AIRGREP_LOOP_TICK_MS} * 1000;

// One word per slot: the low bits hold the state, the rest the generation.
// Every transition is one compare-and-swap that expects the handle's
// generation, so a stale handle fails the compare and does nothing.
enum State : uint32_t { FREE, RESERVED, LIVE, RUNNING, DONE };

constexpr int64_t no_deadline = 0;
constexpr uint32_t one_shot = 0;

constexpr uint32_t STATE_BITS = 3;
constexpr uint32_t STATE_MASK = (1u << STATE_BITS) - 1;

constexpr uint32_t pack(uint32_t generation, State state) noexcept {
	return (generation << STATE_BITS) | state;
}

struct Slot {
	std::atomic<uint32_t> word{pack(1, FREE)};
	int64_t deadline_us = no_deadline;
	SuccessListener on_success;
	ErrorListener on_error;
};

struct QueuedTask {
	const char* name{};
	Action action;
	Handle handle;
	int64_t deadline_us{};
};

static_assert(std::is_trivially_copyable_v<QueuedTask>);

struct ScheduledTask {
	QueuedTask task;
	int64_t due_us{};
	uint32_t period_ms = one_shot;
	uint32_t timeout_ms{};
};

Slot s_slots[SLOTS];
portMUX_TYPE s_deadline_mux = portMUX_INITIALIZER_UNLOCKED;
ScheduledTask s_scheduled[SCHEDULED];
size_t s_scheduled_count;
portMUX_TYPE s_scheduled_mux = portMUX_INITIALIZER_UNLOCKED;
QueueHandle_t s_queue;

int64_t now_us() { return esp_timer_get_time(); }

int64_t deadline(int64_t now, uint32_t timeout_ms) {
	return timeout_ms ? now + int64_t{timeout_ms} * 1000 : no_deadline;
}

uint32_t at_least_one_ms(uint32_t ms) { return ms ? ms : 1; }

bool cas(Slot& slot, uint32_t expected, uint32_t desired) {
	return slot.word.compare_exchange_strong(expected, desired,
											 std::memory_order_acq_rel);
}

uint32_t generation_of(const Slot& slot) {
	return slot.word.load(std::memory_order_acquire) >> STATE_BITS;
}

Handle reserve(SuccessListener on_success, ErrorListener on_error) {
	for (uint16_t i = 0; i < SLOTS; i++) {
		Slot& slot = s_slots[i];
		const uint32_t generation = generation_of(slot);
		if (cas(slot, pack(generation, FREE), pack(generation, RESERVED))) {
			slot.on_success = on_success;
			slot.on_error = on_error;
			return Handle{generation, i};
		}
	}
	return Handle{};
}

void release(Slot& slot, uint32_t generation) {
	cas(slot, pack(generation, DONE), pack(generation + 1, FREE));
}

void publish_if_reserved(Handle handle) {
	cas(s_slots[handle.slot], pack(handle.generation, RESERVED),
		pack(handle.generation, LIVE));
}

void abandon(Handle handle) {
	cas(s_slots[handle.slot], pack(handle.generation, RESERVED),
		pack(handle.generation + 1, FREE));
}

// Takes a live or running promise to done, runs one listener and frees
// the slot. The winner owns the slot until release, so listeners run
// exactly once.
bool finish(Handle handle, bool success, Value value, Outcome outcome) {
	Slot& slot = s_slots[handle.slot];
	const uint32_t generation = handle.generation;
	if (!cas(slot, pack(generation, LIVE), pack(generation, DONE)) &&
		!cas(slot, pack(generation, RUNNING), pack(generation, DONE))) {
		return false;
	}
	if (success && slot.on_success) {
		slot.on_success(value);
	}
	if (!success && slot.on_error) {
		slot.on_error(outcome);
	}
	release(slot, generation);
	return true;
}

bool succeed(Handle handle, Value value) {
	return finish(handle, true, value, Outcome::error);
}

bool fail_promise(Handle handle, Outcome outcome) {
	return finish(handle, false, Value{}, outcome);
}

bool is_live(Handle handle) {
	const uint32_t w =
		s_slots[handle.slot].word.load(std::memory_order_acquire);
	return w == pack(handle.generation, LIVE);
}

void insert_locked(const ScheduledTask& item) {
	size_t i = s_scheduled_count;
	while (i > 0 && s_scheduled[i - 1].due_us > item.due_us) {
		s_scheduled[i] = s_scheduled[i - 1];
		i--;
	}
	s_scheduled[i] = item;
	s_scheduled_count++;
}

bool insert_scheduled(const ScheduledTask& item) {
	portENTER_CRITICAL(&s_scheduled_mux);
	if (s_scheduled_count == SCHEDULED) {
		portEXIT_CRITICAL(&s_scheduled_mux);
		return false;
	}
	insert_locked(item);
	publish_if_reserved(item.task.handle);
	portEXIT_CRITICAL(&s_scheduled_mux);
	return true;
}

size_t scheduled_index_locked(Handle handle) {
	for (size_t i = 0; i < s_scheduled_count; ++i) {
		const Handle queued = s_scheduled[i].task.handle;
		if (queued.slot == handle.slot &&
			queued.generation == handle.generation) {
			return i;
		}
	}
	return s_scheduled_count;
}

void erase_locked(size_t i) {
	--s_scheduled_count;
	for (; i < s_scheduled_count; ++i) {
		s_scheduled[i] = s_scheduled[i + 1];
	}
}

void remove_scheduled(Handle handle) {
	portENTER_CRITICAL(&s_scheduled_mux);
	const size_t i = scheduled_index_locked(handle);
	if (i < s_scheduled_count) {
		erase_locked(i);
	}
	portEXIT_CRITICAL(&s_scheduled_mux);
}

size_t count_due(int64_t now) {
	portENTER_CRITICAL(&s_scheduled_mux);
	size_t n = 0;
	while (n < s_scheduled_count && s_scheduled[n].due_us <= now) {
		n++;
	}
	portEXIT_CRITICAL(&s_scheduled_mux);
	return n;
}

bool pop_due(int64_t now, ScheduledTask& out) {
	portENTER_CRITICAL(&s_scheduled_mux);
	const bool due = s_scheduled_count > 0 && s_scheduled[0].due_us <= now;
	if (due) {
		out = s_scheduled[0];
		erase_locked(0);
		// A periodic task keeps its timer slot while its action runs.
		if (out.period_ms && is_live(out.task.handle)) {
			ScheduledTask next = out;
			next.due_us = now + int64_t{out.period_ms} * 1000;
			insert_locked(next);
		}
	}
	portEXIT_CRITICAL(&s_scheduled_mux);
	return due;
}

int64_t next_due() {
	portENTER_CRITICAL(&s_scheduled_mux);
	const int64_t due = s_scheduled_count > 0 ? s_scheduled[0].due_us : 0;
	portEXIT_CRITICAL(&s_scheduled_mux);
	return due;
}

void run_once(const QueuedTask& task) {
	Slot& slot = s_slots[task.handle.slot];
	const uint32_t generation = task.handle.generation;
	portENTER_CRITICAL(&s_deadline_mux);
	const bool claimed =
		cas(slot, pack(generation, LIVE), pack(generation, RUNNING));
	if (claimed) {
		slot.deadline_us = task.deadline_us;
	}
	portEXIT_CRITICAL(&s_deadline_mux);
	if (!claimed) {
		return;
	}
	ESP_LOGD(TAG, "run %s", task.name);
	Value value{};
	switch (task.action(task.handle, value)) {
		case Result::done:
			succeed(task.handle, value);
			break;
		case Result::error:
			fail_promise(task.handle, Outcome::error);
			break;
		case Result::pending:
			cas(slot, pack(generation, RUNNING), pack(generation, LIVE));
			break;
	}
}

void run_periodic(const ScheduledTask& item) {
	if (!is_live(item.task.handle)) {
		return;
	}
	ESP_LOGD(TAG, "run %s", item.task.name);
	Value value{};
	if (item.task.action(item.task.handle, value) == Result::error) {
		remove_scheduled(item.task.handle);
		fail_promise(item.task.handle, Outcome::error);
	}
}

int64_t expire_and_find_next_deadline(int64_t now) {
	int64_t next = no_deadline;
	for (uint16_t i = 0; i < SLOTS; i++) {
		const Slot& slot = s_slots[i];
		if ((slot.word.load(std::memory_order_relaxed) & STATE_MASK) != LIVE) {
			continue;
		}
		portENTER_CRITICAL(&s_deadline_mux);
		const uint32_t w = slot.word.load(std::memory_order_acquire);
		const int64_t due = slot.deadline_us;
		portEXIT_CRITICAL(&s_deadline_mux);
		if ((w & STATE_MASK) != LIVE || !due) {
			continue;
		}
		if (due > now) {
			if (next == no_deadline || due < next) {
				next = due;
			}
			continue;
		}
		const Handle handle{w >> STATE_BITS, i};
		if (fail_promise(handle, Outcome::timeout)) {
			ESP_LOGW(TAG, "slot %u timed out", unsigned(i));
		}
	}
	return next;
}

// Fails expired promises and runs the scheduled tasks that were due when
// the sweep began, no more, so a re-arming task cannot starve the queue.
// Returns the next wake time, 0 when nothing is pending.
int64_t sweep() {
	int64_t now = now_us();
	int64_t next = expire_and_find_next_deadline(now);
	ScheduledTask item;
	for (size_t budget = count_due(now); budget > 0 && pop_due(now, item);
		 budget--) {
		if (item.period_ms) {
			run_periodic(item);
		} else {
			item.task.deadline_us = deadline(item.due_us, item.timeout_ms);
			if (item.task.deadline_us != no_deadline &&
				item.task.deadline_us <= now) {
				fail_promise(item.task.handle, Outcome::timeout);
			} else {
				run_once(item.task);
			}
		}
		now = now_us();
		next = expire_and_find_next_deadline(now);
	}
	const int64_t due = next_due();
	if (due && (next == no_deadline || due < next)) {
		next = due;
	}
	return next;
}

TickType_t wait_ticks(int64_t next_wake) {
	int64_t wait = TICK_US;
	if (next_wake) {
		const int64_t until = next_wake - now_us();
		wait = std::min(wait, until);
	}
	if (wait <= 0) {
		return 0;
	}
	return (TickType_t)((wait * configTICK_RATE_HZ + 999999) / 1000000);
}

void watchdog_reset() {
#if CONFIG_ESP_TASK_WDT_EN
	esp_task_wdt_reset();
#endif
}

void run_loop(void*) {
#if CONFIG_ESP_TASK_WDT_EN
	ESP_ERROR_CHECK(esp_task_wdt_add(nullptr));
#endif
	for (;;) {
		watchdog_reset();
		int64_t next_wake = sweep();
		QueuedTask task;
		while (xQueueReceive(s_queue, &task, wait_ticks(next_wake)) == pdTRUE) {
			watchdog_reset();
			publish_if_reserved(task.handle);
			run_once(task);
			next_wake = sweep();
		}
	}
}

Handle enqueue(const Task& task, uint32_t delay_ms, uint32_t period_ms,
			   SuccessListener on_success, ErrorListener on_error) {
	if (!s_queue) {
		ESP_LOGW(TAG, "not started, dropped %s", task.name);
		return Handle{};
	}
	if (!task.action) {
		ESP_LOGW(TAG, "no action, dropped %s", task.name);
		return Handle{};
	}
	Handle handle = reserve(on_success, on_error);
	if (!handle.valid()) {
		ESP_LOGW(TAG, "slots full, dropped %s", task.name);
		return handle;
	}
	const int64_t now = now_us();
	QueuedTask queued{task.name, task.action, handle,
					  delay_ms ? no_deadline : deadline(now, task.timeout_ms)};
	portENTER_CRITICAL(&s_deadline_mux);
	s_slots[handle.slot].deadline_us = queued.deadline_us;
	portEXIT_CRITICAL(&s_deadline_mux);
	bool accepted = false;
	if (delay_ms == 0) {
		accepted = xQueueSend(s_queue, &queued, 0) == pdTRUE;
		if (accepted) {
			publish_if_reserved(handle);
		}
	} else {
		const ScheduledTask item{queued, now + int64_t{delay_ms} * 1000,
								 period_ms, task.timeout_ms};
		accepted = insert_scheduled(item);
	}
	if (!accepted) {
		ESP_LOGW(TAG, "queue full, dropped %s", task.name);
		abandon(handle);
		return Handle{};
	}
	return handle;
}
}

void start() {
	if (s_queue) {
		return;
	}
	static StaticQueue_t queue_storage;
	static uint8_t queue_items[SLOTS * sizeof(QueuedTask)];
	static StaticTask_t task_storage;
	static StackType_t stack[CONFIG_AIRGREP_LOOP_STACK / sizeof(StackType_t)];
	s_queue = xQueueCreateStatic(SLOTS, sizeof(QueuedTask), queue_items,
								 &queue_storage);
	xTaskCreateStaticPinnedToCore(
		run_loop, "loop", CONFIG_AIRGREP_LOOP_STACK / sizeof(StackType_t),
		nullptr, CONFIG_AIRGREP_LOOP_PRIORITY, stack, &task_storage, 0);
	ESP_LOGI(TAG, "tick %d ms, %d slots, %d scheduled",
			 CONFIG_AIRGREP_LOOP_TICK_MS, SLOTS, (int)SCHEDULED);
}

bool try_submit(const Task& task, SuccessListener on_success,
				ErrorListener on_error) {
	return enqueue(task, 0, one_shot, on_success, on_error).valid();
}

bool try_submit(const char* name, Action action) {
	return enqueue(Task{name, action}, 0, one_shot, {}, {}).valid();
}

Handle submit(const Task& task, SuccessListener on_success,
			  ErrorListener on_error) {
	return enqueue(task, 0, one_shot, on_success, on_error);
}

Handle submit(const char* name, Action action) {
	return enqueue(Task{name, action}, 0, one_shot, {}, {});
}

Handle schedule(uint32_t delay_ms, const Task& task, SuccessListener on_success,
				ErrorListener on_error) {
	return enqueue(task, at_least_one_ms(delay_ms), one_shot, on_success,
				   on_error);
}

Handle schedule_periodic(uint32_t period_ms, const PeriodicTask& task,
						 ErrorListener on_error) {
	const uint32_t period = at_least_one_ms(period_ms);
	return enqueue({task.name, task.action}, period, period, {}, on_error);
}

bool postpone(Handle handle, uint32_t delay_ms) {
	portENTER_CRITICAL(&s_scheduled_mux);
	const size_t i = scheduled_index_locked(handle);
	const bool scheduled = i < s_scheduled_count && is_live(handle);
	if (scheduled) {
		ScheduledTask item = s_scheduled[i];
		const int64_t due = now_us() + int64_t{delay_ms} * 1000;
		if (due > item.due_us) {
			erase_locked(i);
			item.due_us = due;
			insert_locked(item);
		}
	}
	portEXIT_CRITICAL(&s_scheduled_mux);
	return scheduled;
}

bool cancel(Handle handle) {
	if (!handle.valid() || handle.slot >= SLOTS) {
		return false;
	}
	Slot& slot = s_slots[handle.slot];
	if (!cas(slot, pack(handle.generation, LIVE),
			 pack(handle.generation, DONE))) {
		return false;
	}
	remove_scheduled(handle);
	if (slot.on_error) {
		slot.on_error(Outcome::cancelled);
	}
	release(slot, handle.generation);
	return true;
}

bool complete(Handle handle, Value value) {
	return handle.valid() && handle.slot < SLOTS && succeed(handle, value);
}

bool fail(Handle handle, Outcome outcome) {
	return handle.valid() && handle.slot < SLOTS &&
		   fail_promise(handle, outcome);
}

bool pending(Handle handle) {
	if (!handle.valid() || handle.slot >= SLOTS) {
		return false;
	}
	const uint32_t w =
		s_slots[handle.slot].word.load(std::memory_order_acquire);
	return w == pack(handle.generation, LIVE) ||
		   w == pack(handle.generation, RUNNING);
}
}
