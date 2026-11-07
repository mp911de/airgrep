#include <atomic>
#include <cstring>
#include <new>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "loop.hpp"
#include "unity.h"

using namespace loop;

// Lambdas capture one pointer to this record, 4 bytes.
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
	new(&s_probe) Probe();
	start();
	return s_probe;
}

static bool wait_for(std::atomic<int>& counter, int target, int ms) {
	for (int waited = 0; waited < ms; waited += 5) {
		if (counter.load() >= target) {
			return true;
		}
		vTaskDelay(pdMS_TO_TICKS(5));
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
	int64_t until = esp_timer_get_time() + us;
	while (esp_timer_get_time() < until) {
	}
}

TEST_CASE (

"Events run in submission order"
,
"[loop]"
)
{
    Probe& p = fresh();
    for (int i = 0; i < 4; i++) {
        Action a = [&p, i](Handle, Value&) {
            p.order[p.runs++] = i;
            return Result::done;
        };
        TEST_ASSERT_TRUE(submit(Event{"order", a}).valid());
    }
    TEST_ASSERT_TRUE(wait_for(p.runs, 4, 200));
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(i, p.order[i].load());
    }
}

TEST_CASE (

"Success listener receives the action value"
,
"[loop]"
)
{
    Probe& p = fresh();
    Action a = [](Handle, Value& v) {
        v.u = 0xC0FFEE;
        return Result::done;
    };
    TEST_ASSERT_TRUE(submit(Event{"value", a}, count_success(&p), count_error(&p)).valid());
    TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
    TEST_ASSERT_EQUAL_HEX32(0xC0FFEE, p.value.load());
    TEST_ASSERT_EQUAL(0, p.errors.load());
}

TEST_CASE (

"Error result reaches the error listener as error"
,
"[loop]"
)
{
    Probe& p = fresh();
    Action a = [](Handle, Value&) { return Result::error; };
    TEST_ASSERT_TRUE(submit(Event{"error", a}, count_success(&p), count_error(&p)).valid());
    TEST_ASSERT_TRUE(wait_for(p.errors, 1, 200));
    TEST_ASSERT_EQUAL((int)Outcome::error, p.outcome.load());
    TEST_ASSERT_EQUAL(0, p.successes.load());
}

TEST_CASE (

"Cancel before run skips the action and reports cancelled"
,
"[loop]"
)
{
    Probe& p = fresh();
    // The blocker parks the loop so the second event is still queued.
    Action blocker = [](Handle, Value&) {
        vTaskDelay(pdMS_TO_TICKS(30));
        return Result::done;
    };
    Action counted = [&p](Handle, Value&) {
        p.runs++;
        return Result::done;
    };
    TEST_ASSERT_TRUE(submit(Event{"blocker", blocker}).valid());
    Handle h = submit(Event{"cancelled", counted}, count_success(&p), count_error(&p));
    TEST_ASSERT_TRUE(h.valid());
    TEST_ASSERT_TRUE(cancel(h));
    TEST_ASSERT_EQUAL((int)Outcome::cancelled, p.outcome.load());
    TEST_ASSERT_EQUAL_PTR(xTaskGetCurrentTaskHandle(), p.listener_task.load());
    vTaskDelay(pdMS_TO_TICKS(60));
    TEST_ASSERT_EQUAL(0, p.runs.load());
    TEST_ASSERT_EQUAL(1, p.errors.load());
}

TEST_CASE (

"Cancel after completion returns false"
,
"[loop]"
)
{
    Probe& p = fresh();
    Action a = [](Handle, Value&) { return Result::done; };
    Handle h = submit(Event{"done", a}, count_success(&p), count_error(&p));
    TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
    TEST_ASSERT_FALSE(cancel(h));
    TEST_ASSERT_EQUAL(0, p.errors.load());
}

TEST_CASE (

"Cancel while pending reports cancelled and drops the later completion"
,
"[loop]"
)
{
    Probe& p = fresh();
    Handle h = submit(Event{"pending", pending_action(&p)}, count_success(&p), count_error(&p));
    TEST_ASSERT_TRUE(wait_for(p.runs, 1, 200));
    TEST_ASSERT_TRUE(cancel(h));
    TEST_ASSERT_FALSE(complete(h, Value{.u = 1}));
    TEST_ASSERT_EQUAL((int)Outcome::cancelled, p.outcome.load());
    TEST_ASSERT_EQUAL(0, p.successes.load());
}

TEST_CASE (

"Cancelled periodic event stops after its first run"
,
"[loop]"
)
{
    Probe& p = fresh();
    Action a = [&p](Handle, Value&) {
        p.runs++;
        return Result::done;
    };
    Handle h = schedule_periodic(20, Event{"periodic", a}, count_success(&p), count_error(&p));
    TEST_ASSERT_TRUE(h.valid());
    TEST_ASSERT_TRUE(wait_for(p.runs, 1, 200));
    TEST_ASSERT_TRUE(cancel(h));
    int runs = p.runs.load();
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(runs, p.runs.load());
    TEST_ASSERT_EQUAL((int)Outcome::cancelled, p.outcome.load());
}

TEST_CASE (

"Pending action times out"
,
"[loop]"
)
{
    Probe& p = fresh();
    int64_t submitted = esp_timer_get_time();
    Handle h = submit(Event{"slow", pending_action(&p), 50}, count_success(&p), count_error(&p));
    TEST_ASSERT_TRUE(h.valid());
    TEST_ASSERT_TRUE(wait_for(p.errors, 1, 300));
    int64_t elapsed_ms = (esp_timer_get_time() - submitted) / 1000;
    TEST_ASSERT_EQUAL((int)Outcome::timeout, p.outcome.load());
    TEST_ASSERT_GREATER_OR_EQUAL(50, elapsed_ms);
    TEST_ASSERT_LESS_THAN(120, elapsed_ms);
    TEST_ASSERT_FALSE(cancel(h));
}

TEST_CASE (

"No timeout when disabled"
,
"[loop]"
)
{
    Probe& p = fresh();
    Handle h = submit(Event{"forever", pending_action(&p)}, count_success(&p), count_error(&p));
    vTaskDelay(pdMS_TO_TICKS(150));
    TEST_ASSERT_EQUAL(0, p.errors.load());
    TEST_ASSERT_TRUE(cancel(h));
}

TEST_CASE (

"Full queue gives an invalid handle and runs nothing"
,
"[loop]"
)
{
    Probe& p = fresh();
    Action filler = [&p](Handle, Value&) {
        Action noop = [](Handle, Value&) { return Result::done; };
        while (submit(Event{"fill", noop}).valid()) {
            p.accepted++;
        }
        Action dropped = [&p](Handle, Value&) {
            p.dropped_ran = true;
            return Result::done;
        };
        p.handle = submit(Event{"dropped", dropped});
        p.runs++;
        return Result::done;
    };
    TEST_ASSERT_TRUE(submit(Event{"filler", filler}).valid());
    TEST_ASSERT_TRUE(wait_for(p.runs, 1, 500));
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_GREATER_THAN(100, p.accepted);
    TEST_ASSERT_FALSE(p.handle.valid());
    TEST_ASSERT_FALSE(p.dropped_ran.load());
}

TEST_CASE (

"Stale handle cancel is a no-op"
,
"[loop]"
)
{
    Probe& p = fresh();
    Action a = [](Handle, Value&) { return Result::done; };
    Handle h = submit(Event{"done", a}, count_success(&p), count_error(&p));
    TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
    TEST_ASSERT_FALSE(cancel(h));
    TEST_ASSERT_FALSE(cancel(h));
    TEST_ASSERT_FALSE(cancel(Handle{}));
    TEST_ASSERT_EQUAL(0, p.errors.load());
}

TEST_CASE (

"Handle from a reused slot does not reach the new promise"
,
"[loop]"
)
{
    Probe& p = fresh();
    Action a = [](Handle, Value&) { return Result::done; };
    Handle old = submit(Event{"first", a}, count_success(&p), count_error(&p));
    TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
    Handle reused;
    for (int i = 0; i < 300 && !reused.valid(); i++) {
        Handle h = submit(Event{"second", pending_action(&p)}, {}, count_error(&p));
        TEST_ASSERT_TRUE(h.valid());
        if (h.slot == old.slot) {
            reused = h;
        } else {
            TEST_ASSERT_TRUE(cancel(h));
        }
    }
    TEST_ASSERT_TRUE(reused.valid());
    int errors = p.errors.load();
    TEST_ASSERT_FALSE(cancel(old));
    TEST_ASSERT_EQUAL(errors, p.errors.load());
    TEST_ASSERT_TRUE(cancel(reused));
}

static void completer(void* arg) {
	Probe* p = static_cast<Probe*>(arg);
	if (complete(p->handle, Value{.u = 2})) {
		p->order[0]++;
	}
	p->order[1] = 1;
	vTaskDelete(nullptr);
}

TEST_CASE (

"Two tasks completing one promise deliver exactly one outcome"
,
"[loop]"
)
{
    Probe& p = fresh();
    for (int round = 0; round < 20; round++) {
        p.handle = submit(Event{"race", pending_action(&p)}, count_success(&p), count_error(&p));
        TEST_ASSERT_TRUE(wait_for(p.runs, round + 1, 200));
        p.order[0] = 0;
        p.order[1] = 0;
        xTaskCreatePinnedToCore(completer, "completer", 2048, &p, 6, nullptr, 1);
        if (complete(p.handle, Value{.u = 1})) {
            p.order[0]++;
        }
        TEST_ASSERT_TRUE(wait_for(p.order[1], 1, 200));
        TEST_ASSERT_EQUAL(1, p.order[0].load());
        TEST_ASSERT_EQUAL(round + 1, p.successes.load());
        TEST_ASSERT_EQUAL(0, p.errors.load());
    }
}

TEST_CASE (

"Listeners run on the completing task"
,
"[loop]"
)
{
    Probe& p = fresh();
    Action a = [](Handle, Value&) { return Result::done; };
    TEST_ASSERT_TRUE(submit(Event{"inline", a}, count_success(&p), count_error(&p)).valid());
    TEST_ASSERT_TRUE(wait_for(p.successes, 1, 200));
    TEST_ASSERT_EQUAL_STRING("loop", pcTaskGetName(p.listener_task.load()));

    Handle h = submit(Event{"pending", pending_action(&p)}, count_success(&p), count_error(&p));
    TEST_ASSERT_TRUE(wait_for(p.runs, 1, 200));
    TEST_ASSERT_TRUE(complete(h, Value{.u = 7}));
    TEST_ASSERT_EQUAL_PTR(xTaskGetCurrentTaskHandle(), p.listener_task.load());
    TEST_ASSERT_EQUAL(7, p.value.load());
}

TEST_CASE (

"Submit from inside an action runs after the current action"
,
"[loop]"
)
{
    Probe& p = fresh();
    Action outer = [&p](Handle, Value&) {
        Action inner = [&p](Handle, Value&) {
            p.order[p.runs++] = 2;
            return Result::done;
        };
        submit(Event{"inner", inner});
        busy_us(5000);
        p.order[p.runs++] = 1;
        return Result::done;
    };
    TEST_ASSERT_TRUE(submit(Event{"outer", outer}).valid());
    TEST_ASSERT_TRUE(wait_for(p.runs, 2, 200));
    TEST_ASSERT_EQUAL(1, p.order[0].load());
    TEST_ASSERT_EQUAL(2, p.order[1].load());
}

TEST_CASE (

"Delayed event runs after its delay even while the queue is saturated"
,
"[loop]"
)
{
    Probe& p = fresh();
    int64_t scheduled_at = esp_timer_get_time();
    Action delayed = [&p](Handle, Value&) {
        p.ran_at = esp_timer_get_time();
        p.successes++;
        return Result::done;
    };
    TEST_ASSERT_TRUE(schedule(100, Event{"delayed", delayed}).valid());
    Action filler = [&p](Handle, Value&) {
        Action slow = [&p](Handle, Value&) {
            busy_us(2000);
            p.last_run_at = esp_timer_get_time();
            p.runs++;
            return Result::done;
        };
        while (submit(Event{"slow", slow}).valid()) {
            p.accepted++;
        }
        return Result::done;
    };
    TEST_ASSERT_TRUE(submit(Event{"filler", filler}).valid());
    TEST_ASSERT_TRUE(wait_for(p.successes, 1, 1000));
    TEST_ASSERT_TRUE(wait_for(p.runs, p.accepted, 2000));
    int64_t delay_ms = (p.ran_at.load() - scheduled_at) / 1000;
    TEST_ASSERT_GREATER_OR_EQUAL(100, delay_ms);
    TEST_ASSERT_LESS_THAN(150, delay_ms);
    TEST_ASSERT_TRUE(p.ran_at.load() < p.last_run_at.load());
    TEST_ASSERT_GREATER_THAN(100, p.accepted);
}

TEST_CASE (

"Periodic event runs repeatedly until cancelled"
,
"[loop]"
)
{
    Probe& p = fresh();
    Action a = [&p](Handle, Value&) {
        p.runs++;
        return Result::done;
    };
    Handle h = schedule_periodic(20, Event{"poll", a}, count_success(&p), count_error(&p));
    TEST_ASSERT_TRUE(wait_for(p.runs, 5, 400));
    TEST_ASSERT_TRUE(cancel(h));
    TEST_ASSERT_EQUAL(0, p.successes.load());
    TEST_ASSERT_EQUAL(1, p.errors.load());
}

TEST_CASE (

"Listeners run at most once"
,
"[loop]"
)
{
    Probe& p = fresh();
    Handle h = submit(Event{"once", pending_action(&p)}, count_success(&p), count_error(&p));
    TEST_ASSERT_TRUE(wait_for(p.runs, 1, 200));
    TEST_ASSERT_TRUE(complete(h, Value{.u = 1}));
    TEST_ASSERT_FALSE(complete(h, Value{.u = 2}));
    TEST_ASSERT_FALSE(fail(h, Outcome::error));
    TEST_ASSERT_FALSE(cancel(h));
    TEST_ASSERT_EQUAL(1, p.successes.load());
    TEST_ASSERT_EQUAL(0, p.errors.load());
    TEST_ASSERT_EQUAL(1, p.value.load());
}
