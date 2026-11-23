#include "adsbee_clock.hpp"

#include <sys/time.h>

#include <cstdio>
#include <cstring>
#include <utility>

#include "adsbee_link.hpp"
#include "adsbee_time.hpp"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "events/events.hpp"

namespace adsbee::timesync {
namespace {
constexpr const char* tag = "timesync";
TimeSample samples[ADSBEE_TIME_PING_COUNT];
unsigned sent, attempted;
bool sync_requested_for_fix;
bool set_burst_active(bool active) {
	static bool active_on_scheduler;
	bool changed = false;
	State::update([&](State& status) {
		changed = active_on_scheduler != active;
		active_on_scheduler = active;
		status.burst_running = active;
	});
	return changed;
}

void record_error(esp_err_t result) {
	int64_t now = esp_timer_get_time();
	State::update([&](State& state) {
		state.last_error = result;
		state.error_us = now;
	});
}

bool is_time_ping(const Message& message) {
	return message.kind == DECODER_AT_REPLY &&
		   std::strncmp(message.text, ADSBEE_TIME_PING_REPLY,
						sizeof ADSBEE_TIME_PING_REPLY - 1) == 0;
}

void on_time_ping(const Message& message, void*) {
	TimePing ping;
	if (!decode_time_ping(message.text, message.len, ping)) {
		return;
	}
	for (unsigned i = 0; i < sent; i++) {
		if (!std::cmp_equal(samples[i].t1_us, ping.echo)) {
			continue;
		}
		samples[i].answered = ping.epoch_known;
		samples[i].t4_us = message.received_us;
		samples[i].t2_ms = ping.epoch_ms;
		samples[i].pps = ping.pps;
	}
}

void finish_time_sync() {
	TimeEstimate estimate = estimate_time(samples, sent);
	unsigned answered = 0;
	for (unsigned i = 0; i < sent; i++) {
		answered += samples[i].answered;
	}
	State::update(
		[&](State& state) { state.burst_replies = uint8_t(answered); });
	set_burst_active(false);
	if (!estimate.valid) {
		record_error(answered ? ESP_ERR_NOT_SUPPORTED : ESP_ERR_TIMEOUT);
		return;
	}
	int64_t now = esp_timer_get_time();
	timeval tv{};
	int64_t system_now = 0, projected_utc_us = 0, correction = 0;
	const int64_t elapsed_since_reply_us = now - estimate.reply_received_us;
	if (gettimeofday(&tv, nullptr) != 0 ||
		__builtin_mul_overflow(tv.tv_sec, 1000000, &system_now) ||
		__builtin_add_overflow(system_now, tv.tv_usec, &system_now) ||
		__builtin_add_overflow(estimate.utc_at_reply_us, elapsed_since_reply_us,
							   &projected_utc_us) ||
		__builtin_sub_overflow(projected_utc_us, system_now, &correction)) {
		record_error(ESP_ERR_INVALID_RESPONSE);
		return;
	}
	const bool set = State::copy().clock_set_from_gnss;
	esp_err_t result = ESP_OK;
	if (!set || correction > ADSBEE_TIME_STEP_THRESHOLD_US ||
		correction < -ADSBEE_TIME_STEP_THRESHOLD_US) {
		const timeval next{time_t(projected_utc_us / 1000000),
						   suseconds_t(projected_utc_us % 1000000)};
		if (settimeofday(&next, nullptr) != 0) {
			result = ESP_FAIL;
		}
	} else {
		const timeval delta{time_t(correction / 1000000),
							suseconds_t(correction % 1000000)};
		if (adjtime(&delta, nullptr) != 0) {
			result = ESP_FAIL;
		}
	}
	if (result != ESP_OK) {
		record_error(result);
		return;
	}
	ESP_LOGI(tag, "clock %s by %lld us, rtt %lld us, pps %lu",
			 set ? "slewed" : "set", (long long)correction,
			 (long long)estimate.rtt_us, (unsigned long)estimate.pps);
	State::update([&](State& state) {
		state.clock_set_from_gnss = true;
		state.last_sync_us = now;
		state.last_correction_us = correction;
		state.rtt_us = estimate.rtt_us;
		state.pps = estimate.pps;
	});
	if (!set) {
		events::publish(events::Event::clock_set());
	}
}

loop::Result send_time_ping(loop::Handle, loop::Value&) {
	if (!accepts_requests()) {
		set_burst_active(false);
		record_error(ESP_ERR_INVALID_STATE);
		return loop::Result::done;
	}
	if (attempted < ADSBEE_TIME_PING_COUNT) {
		attempted++;
		if (!busy() || !State::copy().clock_set_from_gnss) {
			char command[sizeof ADSBEE_TIME_PING_COMMAND + 20];
			const int64_t t1 = esp_timer_get_time();
			(void)std::snprintf(command, sizeof command,
								ADSBEE_TIME_PING_COMMAND "%lld", (long long)t1);
			samples[sent] = TimeSample{};
			samples[sent].t1_us = t1;
			sent++;
			if (!request(command, {}, {}, ADSBEE_TIME_PING_WINDOW_MS).valid()) {
				record_error(ESP_ERR_INVALID_STATE);
			}
		}
		if (!loop::schedule(ADSBEE_TIME_PING_SPACING_MS,
							{"timesync ping", send_time_ping})
				 .valid()) {
			finish_time_sync();
			record_error(ESP_ERR_NO_MEM);
		}
		return loop::Result::done;
	}
	finish_time_sync();
	return loop::Result::done;
}

loop::Result periodic(loop::Handle, loop::Value&) {
	if (adsbee::State::copy().gnss_at(esp_timer_get_time()).fix != Fix::none) {
		schedule_gnss_time_sync();
	}
	return loop::Result::done;
}

loop::Result begin_time_sync(loop::Handle, loop::Value&) {
	sent = attempted = 0;
	if (!loop::try_submit("timesync burst", send_time_ping)) {
		set_burst_active(false);
		record_error(ESP_ERR_NO_MEM);
	}
	return loop::Result::done;
}
}

esp_err_t schedule_gnss_time_sync() {
	if (!accepts_requests() || !set_burst_active(true)) {
		return ESP_ERR_INVALID_STATE;
	}
	const uint32_t delay =
		ADSBEE_TIME_SYNC_DELAY_MS + esp_random() % ADSBEE_TIME_SYNC_JITTER_MS;
	if (!loop::schedule(delay, {"timesync delay", begin_time_sync}).valid()) {
		set_burst_active(false);
		return ESP_ERR_NO_MEM;
	}
	return ESP_OK;
}

namespace {
void on_gnss(const Gnss& gnss, int64_t, bool acquired, void*) {
	const bool fix = gnss.fix != Fix::none;
	if (fix && (acquired || !sync_requested_for_fix)) {
		sync_requested_for_fix = schedule_gnss_time_sync() == ESP_OK;
	} else if (!fix) {
		sync_requested_for_fix = false;
	}
}
}

esp_err_t start() {
	if (!subscribe(is_time_ping, on_time_ping, nullptr) ||
		!subscribe_gnss(on_gnss, nullptr)) {
		return ESP_ERR_NO_MEM;
	}
	if (!loop::schedule_periodic(ADSBEE_TIME_SYNC_PERIOD_MS,
								 {"timesync period", periodic})
			 .valid()) {
		return ESP_ERR_NO_MEM;
	}
	return ESP_OK;
}
}
