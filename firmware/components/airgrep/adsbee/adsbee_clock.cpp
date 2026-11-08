#include "adsbee_clock.hpp"
#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include "adsbee_link.hpp"
#include "adsbee_time.hpp"
#include "esp_log.h"
#include "esp_timer.h"

namespace adsbee::timesync {
	portMUX_TYPE State::mutex_ = portMUX_INITIALIZER_UNLOCKED;

	State& State::storage() {
		static State state;
		return state;
	}

	State State::copy() {
		State result;
		update([&](State& state) { result = state; });
		return result;
	}

	namespace {
		constexpr const char* tag = "timesync";
		TimeSample samples[ADSBEE_TIME_PING_COUNT];
		unsigned sent;
		bool running, had_fix;

		void error(esp_err_t result) {
			int64_t now = esp_timer_get_time();
			State::update([&](State& state) {
				state.last_error = result;
				state.error_us = now;
			});
		}

		bool is_time_ping(const Message& message) {
			return message.kind == DECODER_AT_REPLY &&
				std::strncmp(message.text, ADSBEE_TIME_PING_REPLY, sizeof ADSBEE_TIME_PING_REPLY - 1) == 0;
		}

		void on_time_ping(const Message& message, void*) {
			TimePing ping;
			if (!decode_time_ping(message.text, message.len, ping)) return;
			for (unsigned i = 0; i < sent; i++) {
				if (uint64_t(samples[i].t1_us) != ping.echo) continue;
				samples[i].answered = ping.epoch_known;
				samples[i].t4_us = message.received_us;
				samples[i].t2_ms = ping.epoch_ms;
				samples[i].pps = ping.pps;
			}
		}

		// Applies the best sample to the system clock: step when far off or never set, slew otherwise.
		void finish() {
			running = false;
			TimeEstimate estimate = estimate_time(samples, sent);
			unsigned answered = 0;
			for (unsigned i = 0; i < sent; i++) answered += samples[i].answered;
			State::update([&](State& state) {
				state.running = false;
				state.answered = uint8_t(answered);
			});
			if (!estimate.valid) {
				error(answered ? ESP_ERR_NOT_SUPPORTED : ESP_ERR_TIMEOUT);
				return;
			}
			// The system clock at the reply's arrival, then how far it was from the ADSBee's UTC.
			int64_t now = esp_timer_get_time();
			timeval tv;
			gettimeofday(&tv, nullptr);
			int64_t system_now = int64_t(tv.tv_sec) * 1000000 + tv.tv_usec;
			int64_t system_at_t4 = system_now - (now - estimate.t4_us);
			int64_t correction = estimate.utc_at_t4_us - system_at_t4;
			bool set = State::copy().set;
			esp_err_t result = ESP_OK;
			if (!set || correction > ADSBEE_TIME_STEP_THRESHOLD_US || correction < -ADSBEE_TIME_STEP_THRESHOLD_US) {
				int64_t target = system_now + correction;
				timeval next{time_t(target / 1000000), suseconds_t(target % 1000000)};
				if (settimeofday(&next, nullptr) != 0) result = ESP_FAIL;
			}
			else {
				timeval delta{time_t(correction / 1000000), suseconds_t(correction % 1000000)};
				if (adjtime(&delta, nullptr) != 0) result = ESP_FAIL;
			}
			if (result != ESP_OK) {
				error(result);
				return;
			}
			ESP_LOGI(tag, "clock %s by %lld us, rtt %lld us, pps %lu", set ? "slewed" : "set",
					(long long)correction, (long long)estimate.rtt_us, (unsigned long)estimate.pps);
			State::update([&](State& state) {
				state.set = true;
				state.synced_us = now;
				state.correction_us = correction;
				state.rtt_us = estimate.rtt_us;
				state.pps = estimate.pps;
			});
		}

		loop::Result ping(loop::Handle, loop::Value&) {
			if (!adsbee::State::copy().tx_attached) {
				running = false;
				State::update([](State& state) { state.running = false; });
				error(ESP_ERR_INVALID_STATE);
				return loop::Result::done;
			}
			if (sent < ADSBEE_TIME_PING_COUNT) {
				char command[sizeof ADSBEE_TIME_PING_COMMAND + 20];
				int64_t t1 = esp_timer_get_time();
				std::snprintf(command, sizeof command, ADSBEE_TIME_PING_COMMAND "%lld", (long long)t1);
				samples[sent] = TimeSample{};
				samples[sent].t1_us = t1;
				sent++;
				if (!adsbee::request(command, {}, {}, ADSBEE_TIME_PING_WINDOW_MS).valid())
					error(ESP_ERR_INVALID_STATE);
				if (!loop::schedule(ADSBEE_TIME_PING_SPACING_MS, {"timesync ping", ping}).valid()) {
					finish();
					error(ESP_ERR_NO_MEM);
				}
				return loop::Result::done;
			}
			finish();
			return loop::Result::done;
		}

		loop::Result periodic(loop::Handle, loop::Value&) {
			const auto link = adsbee::State::copy();
			if (link.tx_attached && link.initialized) sync();
			return loop::Result::done;
		}
	}

	esp_err_t sync() {
		if (running) return ESP_ERR_INVALID_STATE;
		if (!adsbee::State::copy().tx_attached) return ESP_ERR_INVALID_STATE;
		running = true;
		sent = 0;
		State::update([](State& state) { state.running = true; });
		if (!loop::hsubmit("timesync burst", ping)) {
			running = false;
			State::update([](State& state) { state.running = false; });
			return ESP_ERR_NO_MEM;
		}
		return ESP_OK;
	}

	void attached() { sync(); }

	void fix(bool valid) {
		if (valid && !had_fix) sync();
		had_fix = valid;
	}

	esp_err_t start() {
		if (!adsbee::subscribe(is_time_ping, on_time_ping, nullptr)) return ESP_ERR_NO_MEM;
		if (!loop::schedule_periodic(ADSBEE_TIME_SYNC_PERIOD_MS, {"timesync period", periodic}).valid())
			return ESP_ERR_NO_MEM;
		return ESP_OK;
	}
}
