#include <atomic>
#include <cstring>
#include <new>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "loop.hpp"
#include "unity.h"

using namespace loop;

struct Probe {
	std::atomic<int> runs{0};
	std::atomic<int> successes{0};
	std::atomic<int> errors{0};
	std::atomic<uint32_t> value{0};
	std::atomic<int> outcome{-1};
	std::atomic<int> order[4]{};
	std::atomic<int64_t> ran_at{0};
	std::atomic<int64_t> last_run_at{0};
	std::atomic<TaskHandle_t> listener_task{nullptr};
	std::atomic<bool> dropped_ran{false};
	Handle handle;
	Handle other;
	int accepted = 0;
};

static Probe s_probe;

static Probe& fresh() {
	s_probe.~Probe();
	new (&s_probe) Probe();
	start();
	return s_probe;
}

static bool wait_for(std::atomic<int>& counter, int target, int ms) {
	const int64_t until = esp_timer_get_time() + int64_t{ms} * 1000;
	while (esp_timer_get_time() < until) {
		if (counter.load() >= target) {
			return true;
		}
		vTaskDelay(1);
	}
	return counter.load() >= target;
}

static SuccessListener count_success(Probe* p) {
	return [p](Value v) {
		p->value = v.u;
		p->listener_task = xTaskGetCurrentTaskHandle();
		p->successes++;
	};
}

static ErrorListener count_error(Probe* p) {
	return [p](Outcome o) {
		p->outcome = (int)o;
		p->listener_task = xTaskGetCurrentTaskHandle();
		p->errors++;
	};
}

static Action pending_action(Probe* p) {
	return [p](Handle, Value&) {
		p->runs++;
		return Result::pending;
	};
}

static void busy_us(int64_t us) {
	const int64_t until = esp_timer_get_time() + us;
	while (esp_timer_get_time() < until) {
	}
}

TEST_CASE(

	"Tasks run in submission order", "[loop]") {
	Probe& p = fresh();
	for (int i = 0; i < 4; i++) {
		const Action a = [&p, i](Handle, Value&) {
			p.order[p.runs++] = i;
			return Result::done;
		};
		TEST_ASSERT_TRUE(submit(Task{"order", a}).valid());
	}
	TEST_ASSERT_TRUE(wait_for(p.runs, 4, 200));
	for (int i = 0; i < 4; i++) {
		TEST_ASSERT_EQUAL(i, p.order[i].load());
	}
}

TEST_CASE(

	"Success listener receives the action value", "[loop]") {
	Probe& p = fresh();
	const Action a = [](Handle, Value& v) {
		v.u = 0xC0FFEE;
		return Result::done;
	};
	TEST_ASSERT_TRUE(
		submit(Task{"value", a}, count_success(&p), count_error(&p)).valid());
	TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
	TEST_ASSERT_EQUAL_HEX32(0xC0FFEE, p.value.load());
	TEST_ASSERT_EQUAL(0, p.errors.load());
}

TEST_CASE(

	"Error result reaches the error listener as error", "[loop]") {
	Probe& p = fresh();
	const Action a = [](Handle, Value&) { return Result::error; };
	TEST_ASSERT_TRUE(
		submit(Task{"error", a}, count_success(&p), count_error(&p)).valid());
	TEST_ASSERT_TRUE(wait_for(p.errors, 1, 200));
	TEST_ASSERT_EQUAL((int)Outcome::error, p.outcome.load());
	TEST_ASSERT_EQUAL(0, p.successes.load());
}

TEST_CASE(

	"Cancel before run skips the action and reports cancelled", "[loop]") {
	Probe& p = fresh();
	static StaticSemaphore_t storage;
	auto gate = xSemaphoreCreateBinaryStatic(&storage);
	const Action blocker = [gate](Handle, Value&) {
		xSemaphoreTake(gate, portMAX_DELAY);
		return Result::done;
	};
	const Action counted = [&p](Handle, Value&) {
		p.runs++;
		return Result::done;
	};
	TEST_ASSERT_TRUE(submit(Task{"blocker", blocker}).valid());
	const Handle h =
		submit(Task{"cancelled", counted}, count_success(&p), count_error(&p));
	const bool cancelled = cancel(h);
	xSemaphoreGive(gate);
	TEST_ASSERT_TRUE(h.valid());
	TEST_ASSERT_TRUE(cancelled);
	TEST_ASSERT_EQUAL((int)Outcome::cancelled, p.outcome.load());
	TEST_ASSERT_EQUAL_PTR(xTaskGetCurrentTaskHandle(), p.listener_task.load());
	const Action fence = [&p](Handle, Value&) {
		p.successes++;
		return Result::done;
	};
	TEST_ASSERT_TRUE(submit(Task{"fence", fence}).valid());
	TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
	TEST_ASSERT_EQUAL(0, p.runs.load());
	TEST_ASSERT_EQUAL(1, p.errors.load());
}

TEST_CASE(

	"Cancel after completion returns false", "[loop]") {
	Probe& p = fresh();
	const Action a = [](Handle, Value&) { return Result::done; };
	const Handle h =
		submit(Task{"done", a}, count_success(&p), count_error(&p));
	TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
	TEST_ASSERT_FALSE(cancel(h));
	TEST_ASSERT_EQUAL(0, p.errors.load());
}

TEST_CASE(

	"Cancel while pending reports cancelled and drops the later completion",
	"[loop]") {
	Probe& p = fresh();
	const Handle h = submit(Task{"pending", pending_action(&p)},
							count_success(&p), count_error(&p));
	TEST_ASSERT_TRUE(wait_for(p.runs, 1, 200));
	TEST_ASSERT_TRUE(cancel(h));
	TEST_ASSERT_FALSE(complete(h, Value{.u = 1}));
	TEST_ASSERT_EQUAL((int)Outcome::cancelled, p.outcome.load());
	TEST_ASSERT_EQUAL(0, p.successes.load());
}

TEST_CASE(

	"Cancelled periodic task stops after its first run", "[loop]") {
	Probe& p = fresh();
	const Action a = [&p](Handle, Value&) {
		p.runs++;
		return Result::done;
	};
	const Handle h = schedule_periodic(20, {"periodic", a}, count_error(&p));
	TEST_ASSERT_TRUE(h.valid());
	TEST_ASSERT_TRUE(wait_for(p.runs, 1, 200));
	TEST_ASSERT_TRUE(cancel(h));
	const int runs = p.runs.load();
	vTaskDelay(pdMS_TO_TICKS(100));
	TEST_ASSERT_EQUAL(runs, p.runs.load());
	TEST_ASSERT_EQUAL((int)Outcome::cancelled, p.outcome.load());
}

TEST_CASE(

	"Pending action times out", "[loop]") {
	Probe& p = fresh();
	const int64_t submitted = esp_timer_get_time();
	const Handle h = submit(Task{"slow", pending_action(&p), 50},
							count_success(&p), count_error(&p));
	TEST_ASSERT_TRUE(h.valid());
	TEST_ASSERT_TRUE(wait_for(p.errors, 1, 300));
	const int64_t elapsed_ms = (esp_timer_get_time() - submitted) / 1000;
	TEST_ASSERT_EQUAL((int)Outcome::timeout, p.outcome.load());
	TEST_ASSERT_GREATER_OR_EQUAL(50, elapsed_ms);
	TEST_ASSERT_LESS_THAN(120, elapsed_ms);
	TEST_ASSERT_FALSE(cancel(h));
}

TEST_CASE("An overdue scheduled action expires at its original deadline",
		  "[loop]") {
	Probe& p = fresh();
	auto arm = [&p](Handle, Value&) {
		p.handle = schedule(5, Task{"overdue", pending_action(&p), 10}, {},
							count_error(&p));
		busy_us(30000);
		return Result::done;
	};
	TEST_ASSERT_TRUE(submit("occupy loop", arm).valid());
	TEST_ASSERT_TRUE(wait_for(p.errors, 1, 300));
	TEST_ASSERT_TRUE(p.handle.valid());
	TEST_ASSERT_EQUAL((int)Outcome::timeout, p.outcome.load());
	TEST_ASSERT_EQUAL(0, p.runs.load());
}

TEST_CASE("A completion listener sees its promise as finished", "[loop]") {
	Probe& p = fresh();
	auto action = [&p](Handle self, Value&) {
		p.handle = self;
		return Result::done;
	};
	auto listener = [&p](Value) {
		p.value = pending(p.handle);
		++p.successes;
	};
	TEST_ASSERT_TRUE(submit({"listener", action}, listener).valid());
	TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
	TEST_ASSERT_EQUAL(0u, p.value.load());
}

TEST_CASE("No timeout when disabled", "[loop]") {
	Probe& p = fresh();
	const Handle h = submit(Task{"forever", pending_action(&p)},
							count_success(&p), count_error(&p));
	vTaskDelay(pdMS_TO_TICKS(150));
	TEST_ASSERT_EQUAL(0, p.errors.load());
	TEST_ASSERT_TRUE(cancel(h));
}

TEST_CASE(

	"Full queue gives an invalid handle and runs nothing", "[loop]") {
	Probe& p = fresh();
	const Action filler = [&p](Handle, Value&) {
		const Action counted = [&p](Handle, Value&) {
			p.successes++;
			return Result::done;
		};
		while (submit(Task{"fill", counted}).valid()) {
			p.accepted++;
		}
		const Action dropped = [&p](Handle, Value&) {
			p.dropped_ran = true;
			return Result::done;
		};
		p.handle = submit(Task{"dropped", dropped});
		p.runs++;
		return Result::done;
	};
	TEST_ASSERT_TRUE(submit(Task{"filler", filler}).valid());
	TEST_ASSERT_TRUE(wait_for(p.runs, 1, 500));
	TEST_ASSERT_GREATER_THAN(0, p.accepted);
	TEST_ASSERT_TRUE(wait_for(p.successes, p.accepted, 2000));
	TEST_ASSERT_FALSE(p.handle.valid());
	TEST_ASSERT_FALSE(p.dropped_ran.load());
}

TEST_CASE(

	"Stale handle cancel is a no-op", "[loop]") {
	Probe& p = fresh();
	const Action a = [](Handle, Value&) { return Result::done; };
	const Handle h =
		submit(Task{"done", a}, count_success(&p), count_error(&p));
	TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
	TEST_ASSERT_FALSE(cancel(h));
	TEST_ASSERT_FALSE(cancel(h));
	TEST_ASSERT_FALSE(cancel(Handle{}));
	TEST_ASSERT_EQUAL(0, p.errors.load());
}

TEST_CASE(

	"Handle from a reused slot does not reach the new promise", "[loop]") {
	Probe& p = fresh();
	const Action a = [](Handle, Value&) { return Result::done; };
	const Handle old =
		submit(Task{"first", a}, count_success(&p), count_error(&p));
	TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
	Handle reused;
	for (int i = 0; i < 300 && !reused.valid(); i++) {
		const Handle h =
			submit(Task{"second", pending_action(&p)}, {}, count_error(&p));
		TEST_ASSERT_TRUE(h.valid());
		if (h.slot == old.slot) {
			reused = h;
		} else {
			TEST_ASSERT_TRUE(cancel(h));
		}
	}
	TEST_ASSERT_TRUE(reused.valid());
	const int errors = p.errors.load();
	TEST_ASSERT_FALSE(cancel(old));
	TEST_ASSERT_EQUAL(errors, p.errors.load());
	TEST_ASSERT_TRUE(cancel(reused));
}

TEST_CASE(
	"Completing and reusing a timed slot preserves the replacement deadline",
	"[loop]") {
	Probe& p = fresh();
	for (int i = 0; i < 32; ++i) {
		const Handle old = submit(Task{"old", pending_action(&p), 2000},
								  count_success(&p), count_error(&p));
		TEST_ASSERT_TRUE(old.valid());
		TEST_ASSERT_TRUE(wait_for(p.runs, i + 1, 200));
		TEST_ASSERT_TRUE(complete(old, Value{}));
		const Handle next =
			submit(Task{"replacement",
						[](Handle, Value&) { return Result::pending; }, 20},
				   {}, count_error(&p));
		TEST_ASSERT_TRUE(next.valid());
		TEST_ASSERT_EQUAL(old.slot, next.slot);
		TEST_ASSERT_FALSE(fail(old, Outcome::error));
		TEST_ASSERT_TRUE(wait_for(p.errors, i + 1, 250));
		TEST_ASSERT_EQUAL((int)Outcome::timeout, p.outcome.load());
	}
	TEST_ASSERT_EQUAL(32, p.successes.load());
	TEST_ASSERT_EQUAL(32, p.errors.load());
}

static void completer(void* arg) {
	Probe* p = static_cast<Probe*>(arg);
	if (complete(p->handle, Value{.u = 2})) {
		p->order[0]++;
	}
	p->order[1] = 1;
	vTaskDelete(nullptr);
}

TEST_CASE(

	"Two tasks completing one promise deliver exactly one outcome", "[loop]") {
	Probe& p = fresh();
	for (int round = 0; round < 20; round++) {
		p.handle = submit(Task{"race", pending_action(&p)}, count_success(&p),
						  count_error(&p));
		TEST_ASSERT_TRUE(wait_for(p.runs, round + 1, 200));
		p.order[0] = 0;
		p.order[1] = 0;
		xTaskCreatePinnedToCore(completer, "completer", 2048, &p, 6, nullptr,
								1);
		if (complete(p.handle, Value{.u = 1})) {
			p.order[0]++;
		}
		TEST_ASSERT_TRUE(wait_for(p.order[1], 1, 200));
		TEST_ASSERT_EQUAL(1, p.order[0].load());
		TEST_ASSERT_EQUAL(round + 1, p.successes.load());
		TEST_ASSERT_EQUAL(0, p.errors.load());
	}
}

TEST_CASE(

	"Listeners run on the completing task", "[loop]") {
	Probe& p = fresh();
	const Action a = [](Handle, Value&) { return Result::done; };
	TEST_ASSERT_TRUE(
		submit(Task{"inline", a}, count_success(&p), count_error(&p)).valid());
	TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
	TEST_ASSERT_EQUAL_STRING("loop", pcTaskGetName(p.listener_task.load()));

	const Handle h = submit(Task{"pending", pending_action(&p)},
							count_success(&p), count_error(&p));
	TEST_ASSERT_TRUE(wait_for(p.runs, 1, 200));
	TEST_ASSERT_TRUE(complete(h, Value{.u = 7}));
	TEST_ASSERT_EQUAL_PTR(xTaskGetCurrentTaskHandle(), p.listener_task.load());
	TEST_ASSERT_EQUAL(7, p.value.load());
}

TEST_CASE(

	"Submit from inside an action runs after the current action", "[loop]") {
	Probe& p = fresh();
	const Action outer = [&p](Handle, Value&) {
		const Action inner = [&p](Handle, Value&) {
			p.order[p.runs++] = 2;
			return Result::done;
		};
		submit(Task{"inner", inner});
		busy_us(5000);
		p.order[p.runs++] = 1;
		return Result::done;
	};
	TEST_ASSERT_TRUE(submit(Task{"outer", outer}).valid());
	TEST_ASSERT_TRUE(wait_for(p.runs, 2, 200));
	TEST_ASSERT_EQUAL(1, p.order[0].load());
	TEST_ASSERT_EQUAL(2, p.order[1].load());
}

TEST_CASE(

	"Delayed task runs after its delay even while the queue is saturated",
	"[loop]") {
	Probe& p = fresh();
	const Action filler = [&p](Handle, Value&) {
		const Action slow = [&p](Handle, Value&) {
			if (p.runs == 0) {
				const int64_t scheduled_at = esp_timer_get_time();
				const Action delayed = [&p, scheduled_at](Handle, Value&) {
					p.ran_at = esp_timer_get_time() - scheduled_at;
					p.value = p.runs.load();
					p.successes++;
					return Result::done;
				};
				p.handle = schedule(100, Task{"delayed", delayed});
			}
			constexpr int queue_work_us = 200000;
			busy_us(queue_work_us / p.accepted);
			p.runs++;
			return Result::done;
		};
		while (submit(Task{"slow", slow}).valid()) {
			p.accepted++;
		}
		return Result::done;
	};
	TEST_ASSERT_TRUE(submit(Task{"filler", filler}).valid());
	TEST_ASSERT_TRUE(wait_for(p.successes, 1, 1000));
	TEST_ASSERT_TRUE(p.handle.valid());
	TEST_ASSERT_GREATER_THAN(0, p.accepted);
	TEST_ASSERT_TRUE(wait_for(p.runs, p.accepted, 2000));
	TEST_ASSERT_GREATER_OR_EQUAL(100000, p.ran_at.load());
	TEST_ASSERT_LESS_THAN(150000, p.ran_at.load());
	TEST_ASSERT_LESS_THAN_UINT(p.accepted, p.value.load());
}

TEST_CASE(

	"Periodic task runs repeatedly until cancelled", "[loop]") {
	Probe& p = fresh();
	const Action a = [&p](Handle, Value&) {
		p.runs++;
		return Result::done;
	};
	const Handle h = schedule_periodic(20, {"poll", a}, count_error(&p));
	TEST_ASSERT_TRUE(wait_for(p.runs, 5, 400));
	TEST_ASSERT_TRUE(cancel(h));
	TEST_ASSERT_EQUAL(1, p.errors.load());
}

TEST_CASE("A full scheduler keeps a periodic poll and a reserved release",
		  "[loop]") {
	Probe& p = fresh();
	static Handle fillers[CONFIG_AIRGREP_LOOP_SCHEDULED];
	auto fill = [&p](Handle poll, Value&) {
		if (++p.runs == 1) {
			p.other = schedule(1, {"release", [&p](Handle, Value&) {
									   p.ran_at = esp_timer_get_time();
									   ++p.successes;
									   return Result::done;
								   }});
			const Action noop = [](Handle, Value&) { return Result::done; };
			while (p.accepted < CONFIG_AIRGREP_LOOP_SCHEDULED) {
				const Handle h = schedule(5000, {"filler", noop});
				if (!h.valid()) {
					break;
				}
				fillers[p.accepted++] = h;
			}
			p.last_run_at = esp_timer_get_time();
			p.order[0] = postpone(p.other, 40);
			p.dropped_ran = schedule(5000, {"full", noop}).valid();
		} else {
			cancel(poll);
		}
		return Result::done;
	};
	TEST_ASSERT_TRUE(schedule_periodic(10, {"poll", fill}).valid());
	const bool polled = wait_for(p.runs, 2, 500);
	const bool released = wait_for(p.successes, 1, 500);
	for (int i = 0; i < p.accepted; ++i) {
		cancel(fillers[i]);
	}
	TEST_ASSERT_TRUE(polled);
	TEST_ASSERT_TRUE(released);
	TEST_ASSERT_FALSE(p.dropped_ran.load());
	TEST_ASSERT_EQUAL(1, p.order[0].load());
	TEST_ASSERT_TRUE(p.ran_at.load() - p.last_run_at.load() >= 40000);
}

TEST_CASE(

	"Listeners run at most once", "[loop]") {
	Probe& p = fresh();
	const Handle h = submit(Task{"once", pending_action(&p)}, count_success(&p),
							count_error(&p));
	TEST_ASSERT_TRUE(wait_for(p.runs, 1, 200));
	TEST_ASSERT_TRUE(complete(h, Value{.u = 1}));
	TEST_ASSERT_FALSE(complete(h, Value{.u = 2}));
	TEST_ASSERT_FALSE(fail(h, Outcome::error));
	TEST_ASSERT_FALSE(cancel(h));
	TEST_ASSERT_EQUAL(1, p.successes.load());
	TEST_ASSERT_EQUAL(0, p.errors.load());
	TEST_ASSERT_EQUAL(1, p.value.load());
}
