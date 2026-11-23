#include "wifi_ap.hpp"

#include <cstring>

#include "adsbee/adsbee_link.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "events/events.hpp"
#include "loop.hpp"
#include "wifi_requests.hpp"

namespace wifi {
namespace {
constexpr const char* tag = "wifi";
Config wanted;
bool slider_sampled;
Requests requests;
void dispatch();

void want_apply() {
	requests.apply();
	State::update([](State& state) { state.set_pending = true; });
	dispatch();
}

void record_error(esp_err_t result) {
	int64_t now = esp_timer_get_time();
	State::update([&](State& state) {
		state.last_error = result;
		state.error_us = now;
	});
}

bool is_reply(const adsbee::Message& message) {
	return message.kind == DECODER_AT_REPLY &&
		   std::strncmp(message.text, WIFI_AP_REPLY,
						sizeof WIFI_AP_REPLY - 1) == 0;
}

void on_reply(const adsbee::Message& message, void*) {
	Config reported;
	if (!decode_config(message.text, message.len, reported)) {
		return;
	}
	bool mismatch = slider_sampled && reported != wanted;
	State::update([&](State& state) {
		state.reported_known = true;
		state.last_reported_config = reported;
		state.reported_us = message.received_us;
		if (mismatch) {
			state.mismatches++;
		}
	});
	if (mismatch) {
		ESP_LOGW(tag, "ADSBee runs Wi-Fi %s as %s, resending",
				 reported.enabled ? "on" : "off", reported.ssid);
		want_apply();
	}
}

void on_event(const events::Event& event, void*) {
	if (event.type == events::Type::adsbee_reply_window_closed) {
		dispatch();
		return;
	}
	if (event.type == events::Type::adsbee_initialized) {
		if (slider_sampled) {
			want_apply();
		}
		return;
	}
	slider_sampled = true;
	wanted.enabled = event.data.position.on;
	State::update([&](State& s) {
		s.slider_sampled = true;
		s.slider_enabled = wanted.enabled;
	});
	want_apply();
}

loop::Result periodic(loop::Handle, loop::Value&) {
	if (!adsbee::accepts_requests()) {
		return loop::Result::done;
	}
	requests.poll();
	dispatch();
	return loop::Result::done;
}
void dispatch() {
	if (adsbee::busy() || !adsbee::accepts_requests()) {
		return;
	}
	const Command work = requests.submit();
	if (work == Command::none) {
		return;
	}
	char command[sizeof WIFI_AP_SET_COMMAND + 8 + WIFI_AP_SSID_SIZE +
				 WIFI_AP_PASSWORD_SIZE];
	if (work == Command::set && !format_set(wanted, command, sizeof command)) {
		requests.finished();
		record_error(ESP_ERR_INVALID_SIZE);
		return;
	}
	const auto written = [work, enabled = wanted.enabled] {
		requests.written(enabled == wanted.enabled);
		if (work == Command::set) {
			const int64_t now = esp_timer_get_time();
			State::update([&](State& state) {
				state.set_pending = requests.set_pending();
				state.sent_us = now;
				state.sent++;
			});
		}
	};
	const char* line = work == Command::set ? command : WIFI_AP_QUERY_COMMAND;
	auto handle = adsbee::request(
		line, [](loop::Value) { requests.finished(); },
		[](loop::Outcome outcome) {
			requests.finished();
			record_error(outcome == loop::Outcome::timeout ? ESP_ERR_TIMEOUT
														   : ESP_FAIL);
		},
		ADSBEE_REQUEST_TIMEOUT_MS, written);
	if (!handle.valid()) {
		requests.finished();
		record_error(ESP_ERR_NO_MEM);
	}
}
}

esp_err_t apply_wifi_state() {
	if (!State::copy().slider_sampled || !adsbee::accepts_requests()) {
		return ESP_ERR_INVALID_STATE;
	}
	return loop::try_submit("wifi apply",
							[](loop::Handle, loop::Value&) {
								want_apply();
								return loop::Result::done;
							})
			   ? ESP_OK
			   : ESP_ERR_NO_MEM;
}

esp_err_t query_access_point() {
	if (!adsbee::accepts_requests()) {
		return ESP_ERR_INVALID_STATE;
	}
	return loop::try_submit("wifi query", periodic) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t start() {
	if (!adsbee::subscribe(is_reply, on_reply, nullptr)) {
		return ESP_ERR_NO_MEM;
	}
	if (!loop::schedule_periodic(WIFI_AP_POLL_PERIOD_MS,
								 {"wifi poll", periodic})
			 .valid()) {
		return ESP_ERR_NO_MEM;
	}
	State::update([](State& state) { state.status = Status::ok; });
	return ESP_OK;
}

esp_err_t prepare(const char* ssid) {
	std::strncpy(wanted.ssid, ssid, sizeof wanted.ssid - 1);
	std::strncpy(wanted.password, WIFI_AP_PASSWORD, sizeof wanted.password - 1);
	wanted.channel = WIFI_AP_CHANNEL;
	return events::subscribe(
			   events::mask_for(events::Type::wifi_switch_changed,
								events::Type::adsbee_initialized,
								events::Type::adsbee_reply_window_closed),
			   on_event)
			   ? ESP_OK
			   : ESP_ERR_NO_MEM;
}
}
