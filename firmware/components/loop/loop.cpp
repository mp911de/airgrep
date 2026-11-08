#include "loop.hpp"
#include <atomic>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

namespace loop {
	namespace {
		static const char* TAG = "loop";
		constexpr uint16_t SLOTS = CONFIG_AIRGREP_LOOP_QUEUE_DEPTH;
		constexpr size_t SCHEDULED = CONFIG_AIRGREP_LOOP_SCHEDULED;
		constexpr int64_t TICK_US = int64_t{CONFIG_AIRGREP_LOOP_TICK_MS} * 1000;

		// One word per slot: the low bits hold the state, the rest the generation.
		// Every transition is one compare-and-swap that expects the handle's
		// generation, so a stale handle fails the compare and does nothing.
		// RESERVED covers the window between claiming the slot and queuing the
		// event: the sweep ignores it and cancel() returns false in it, although
		// the submission is about to succeed. Only the submitter holds the handle
		// then, so nobody else can observe the window.
		enum State : uint32_t { FREE, RESERVED, LIVE, RUNNING, DONE };

		constexpr uint32_t STATE_BITS = 3;
		constexpr uint32_t STATE_MASK = (1u << STATE_BITS) - 1;

		constexpr uint32_t pack(uint32_t generation, State state) {
			return (generation << STATE_BITS) | state;
		}

		struct Slot {
			std::atomic<uint32_t> word{pack(1, FREE)};
			const char* name;
			int64_t deadline_us; // 0 means none
			SuccessListener on_success;
			ErrorListener on_error;
		};

		struct QueuedEvent {
			const char* name;
			Action action;
			Handle handle;
			int64_t deadline_us;
		};

		static_assert(std::is_trivially_copyable_v<QueuedEvent>);

		struct ScheduledEvent {
			QueuedEvent event;
			int64_t due_us;
			uint32_t period_ms; // 0 for a one-shot
			uint32_t timeout_ms;
		};

		Slot s_slots[SLOTS];
		ScheduledEvent s_scheduled[SCHEDULED];
		size_t s_scheduled_count;
		portMUX_TYPE s_scheduled_mux = portMUX_INITIALIZER_UNLOCKED;
		QueueHandle_t s_queue;

		int64_t now_us() {
			return esp_timer_get_time();
		}

		int64_t deadline(int64_t now, uint32_t timeout_ms) {
			return timeout_ms ? now + int64_t{timeout_ms} * 1000 : 0;
		}

		uint32_t at_least_one_ms(uint32_t ms) {
			return ms ? ms : 1;
		}

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
				uint32_t generation = generation_of(slot);
				if (cas(slot, pack(generation, FREE), pack(generation, RESERVED))) {
					slot.deadline_us = 0;
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

		void publish(Handle handle) {
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
			uint32_t generation = handle.generation;
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

		bool settle(Handle handle, Outcome outcome) {
			return finish(handle, false, Value{}, outcome);
		}

		// Reserved counts as live here: the submitter publishes right after queuing.
		bool is_live(Handle handle) {
			uint32_t w = s_slots[handle.slot].word.load(std::memory_order_acquire);
			return w == pack(handle.generation, LIVE) || w == pack(
				handle.generation, RESERVED);
		}

		// Scheduled events stay sorted by due time, earliest first.
		bool insert_scheduled(const ScheduledEvent& item) {
			portENTER_CRITICAL(&s_scheduled_mux);
			if (s_scheduled_count == SCHEDULED) {
				portEXIT_CRITICAL(&s_scheduled_mux);
				return false;
			}
			size_t i = s_scheduled_count;
			while (i > 0 && s_scheduled[i - 1].due_us > item.due_us) {
				s_scheduled[i] = s_scheduled[i - 1];
				i--;
			}
			s_scheduled[i] = item;
			s_scheduled_count++;
			portEXIT_CRITICAL(&s_scheduled_mux);
			return true;
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

		bool pop_due(int64_t now, ScheduledEvent& out) {
			portENTER_CRITICAL(&s_scheduled_mux);
			bool due = s_scheduled_count > 0 && s_scheduled[0].due_us <= now;
			if (due) {
				out = s_scheduled[0];
				s_scheduled_count--;
				for (size_t i = 0; i < s_scheduled_count; i++) {
					s_scheduled[i] = s_scheduled[i + 1];
				}
			}
			portEXIT_CRITICAL(&s_scheduled_mux);
			return due;
		}

		int64_t next_due() {
			portENTER_CRITICAL(&s_scheduled_mux);
			int64_t due = s_scheduled_count > 0 ? s_scheduled[0].due_us : 0;
			portEXIT_CRITICAL(&s_scheduled_mux);
			return due;
		}

		// Runs a one-shot action: uncancellable while it executes, cancellable again
		// when it returns pending. Mirrors Netty's PromiseTask.run.
		void run_once(const QueuedEvent& event) {
			Slot& slot = s_slots[event.handle.slot];
			uint32_t generation = event.handle.generation;
			if (!cas(slot, pack(generation, LIVE), pack(generation, RUNNING)) &&
				!cas(slot, pack(generation, RESERVED), pack(generation, RUNNING))) {
				return;
			}
			ESP_LOGD(TAG, "run %s", event.name);
			slot.deadline_us = event.deadline_us;
			Value value{};
			switch (event.action(event.handle, value)) {
			case Result::done:
				succeed(event.handle, value);
				break;
			case Result::error:
				settle(event.handle, Outcome::error);
				break;
			case Result::pending:
				cas(slot, pack(generation, RUNNING), pack(generation, LIVE));
				break;
			}
		}

		// Periodic actions are never uncancellable: the loop checks before and after
		// each run and re-arms only while the promise is still live, as Netty's
		// ScheduledFutureTask does.
		void run_periodic(ScheduledEvent item, int64_t now) {
			if (!is_live(item.event.handle)) {
				return;
			}
			ESP_LOGD(TAG, "run %s", item.event.name);
			Value value{};
			if (item.event.action(item.event.handle, value) == Result::error) {
				settle(item.event.handle, Outcome::error);
				return;
			}
			if (!is_live(item.event.handle)) {
				return;
			}
			item.due_us = now + int64_t{item.period_ms} * 1000;
			if (!insert_scheduled(item)) {
				ESP_LOGW(TAG, "scheduled full, %s stops", item.event.name);
				settle(item.event.handle, Outcome::error);
			}
		}

		// Fails expired promises. Returns the earliest remaining deadline, 0 if none.
		int64_t sweep_deadlines(int64_t now) {
			int64_t next = 0;
			for (uint16_t i = 0; i < SLOTS; i++) {
				Slot& slot = s_slots[i];
				uint32_t w = slot.word.load(std::memory_order_acquire);
				if ((w & STATE_MASK) != LIVE || slot.deadline_us == 0) {
					continue;
				}
				if (slot.deadline_us > now) {
					if (next == 0 || slot.deadline_us < next) {
						next = slot.deadline_us;
					}
					continue;
				}
				Handle handle{w >> STATE_BITS, i};
				const char* name = slot.name;
				if (settle(handle, Outcome::timeout)) {
					ESP_LOGW(TAG, "%s timed out", name);
				}
			}
			return next;
		}

		// Fails expired promises and runs the scheduled events that were due when
		// the sweep began, no more, so a re-arming event cannot starve the queue.
		// Returns the next wake time, 0 when nothing is pending.
		int64_t sweep() {
			int64_t now = now_us();
			int64_t next = sweep_deadlines(now);
			ScheduledEvent item;
			for (size_t budget = count_due(now); budget > 0 && pop_due(now, item); budget
				--) {
				if (item.period_ms) {
					run_periodic(item, now);
				}
				else {
					item.event.deadline_us = deadline(now, item.timeout_ms);
					run_once(item.event);
				}
				now = now_us();
				next = sweep_deadlines(now);
			}
			int64_t due = next_due();
			if (due && (next == 0 || due < next)) {
				next = due;
			}
			return next;
		}

		TickType_t wait_ticks(int64_t next_wake) {
			int64_t wait = TICK_US;
			if (next_wake) {
				int64_t until = next_wake - now_us();
				if (until < wait) {
					wait = until;
				}
			}
			if (wait <= 0) {
				return 0;
			}
			return (TickType_t)((wait * configTICK_RATE_HZ + 999999) / 1000000);
		}

		void loop_task(void*) {
			for (;;) {
				int64_t next_wake = sweep();
				QueuedEvent event;
				while (xQueueReceive(s_queue, &event, wait_ticks(next_wake)) == pdTRUE) {
					run_once(event);
					next_wake = sweep();
				}
			}
		}

		// delay_ms 0 queues the event now, otherwise it waits in the scheduled
		// array. period_ms 0 is a one-shot, otherwise the event re-arms.
		Handle enqueue(const Event& event, uint32_t delay_ms, uint32_t period_ms,
						SuccessListener on_success, ErrorListener on_error) {
			if (!s_queue) {
				ESP_LOGW(TAG, "not started, dropped %s", event.name);
				return Handle{};
			}
			if (!event.action) {
				ESP_LOGW(TAG, "no action, dropped %s", event.name);
				return Handle{};
			}
			Handle handle = reserve(on_success, on_error);
			if (!handle.valid()) {
				ESP_LOGW(TAG, "slots full, dropped %s", event.name);
				return handle;
			}
			s_slots[handle.slot].name = event.name;
			int64_t now = now_us();
			QueuedEvent queued{event.name, event.action, handle, 0};
			bool accepted;
			if (delay_ms == 0) {
				queued.deadline_us = deadline(now, event.timeout_ms);
				s_slots[handle.slot].deadline_us = queued.deadline_us;
				accepted = xQueueSend(s_queue, &queued, 0) == pdTRUE;
			}
			else {
				ScheduledEvent item{
					queued, now + int64_t{delay_ms} * 1000, period_ms, event.timeout_ms
				};
				accepted = insert_scheduled(item);
			}
			if (!accepted) {
				ESP_LOGW(TAG, "queue full, dropped %s", event.name);
				abandon(handle);
				return Handle{};
			}
			publish(handle);
			return handle;
		}
	} // namespace

	void start() {
		if (s_queue) {
			return;
		}
		static StaticQueue_t queue_storage;
		static uint8_t queue_items[SLOTS * sizeof(QueuedEvent)];
		static StaticTask_t task_storage;
		static StackType_t stack[CONFIG_AIRGREP_LOOP_STACK / sizeof(StackType_t)];
		s_queue = xQueueCreateStatic(SLOTS, sizeof(QueuedEvent), queue_items,
									&queue_storage);
		xTaskCreateStaticPinnedToCore(loop_task, "loop",
									CONFIG_AIRGREP_LOOP_STACK / sizeof(StackType_t),
									nullptr, CONFIG_AIRGREP_LOOP_PRIORITY, stack,
									&task_storage, 0);
		ESP_LOGI(TAG, "tick %d ms, %d slots, %d scheduled", CONFIG_AIRGREP_LOOP_TICK_MS,
				SLOTS,
				(int)SCHEDULED);
	}

	bool hsubmit(const Event& event, SuccessListener on_success,
				ErrorListener on_error) {
		return enqueue(event, 0, 0, on_success, on_error).valid();
	}

	bool hsubmit(const char* name, Action action) {
		return enqueue(Event{name, action}, 0, 0, {}, {}).valid();
	}

	Handle submit(const Event& event, SuccessListener on_success,
				ErrorListener on_error) {
		return enqueue(event, 0, 0, on_success, on_error);
	}

	Handle submit(const char* name, Action action) {
		return enqueue(Event{name, action}, 0, 0, {}, {});
	}

	Handle schedule(uint32_t delay_ms, const Event& event, SuccessListener on_success,
					ErrorListener on_error) {
		return enqueue(event, at_least_one_ms(delay_ms), 0, on_success, on_error);
	}

	Handle schedule_periodic(uint32_t period_ms, const Event& event,
							SuccessListener on_success,
							ErrorListener on_error) {
		uint32_t period = at_least_one_ms(period_ms);
		return enqueue(event, period, period, on_success, on_error);
	}

	bool cancel(Handle handle) {
		if (!handle.valid() || handle.slot >= SLOTS) {
			return false;
		}
		Slot& slot = s_slots[handle.slot];
		if (!cas(slot, pack(handle.generation, LIVE), pack(handle.generation, DONE))) {
			return false;
		}
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
		return handle.valid() && handle.slot < SLOTS && settle(handle, outcome);
	}

	bool pending(Handle handle) {
		if (!handle.valid() || handle.slot >= SLOTS) {
			return false;
		}
		uint32_t w = s_slots[handle.slot].word.load(std::memory_order_acquire);
		return (w >> STATE_BITS) == handle.generation && (w & STATE_MASK) != FREE;
	}
} // namespace loop
