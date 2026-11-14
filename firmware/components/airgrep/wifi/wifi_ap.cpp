#include "wifi_ap.hpp"

#include <cstring>

#include "adsbee/adsbee_link.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "events/events.hpp"
#include "loop.hpp"

namespace wifi {
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
constexpr const char* tag = "wifi";
Config wanted;
bool switch_known, dirty;
loop::Handle attach_step;

void mark_dirty(bool value) {
	dirty = value;
	State::update([&](State& state) { state.pending_apply = value; });
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
	bool mismatch = switch_known && reported != wanted;
	State::update([&](State& state) {
		state.reported_known = true;
		state.reported = reported;
		state.reported_us = message.received_us;
		if (mismatch) {
			state.mismatches++;
		}
	});
	if (mismatch) {
		ESP_LOGW(tag, "ADSBee runs Wi-Fi %s as %s, resending",
				 reported.enabled ? "on" : "off", reported.ssid);
	}
	if (mismatch || dirty) {
		apply_wifi_state();
	}
}

void on_switch(const events::Event& event, void*) {
	switch_known = true;
	wanted.enabled = event.position.on;
	State::update([&](State& s) {
		s.switch_known = true;
		s.wanted = wanted.enabled;
	});
	// Before the link is up the attach hook sends the position, no error to
	// record.
	if (adsbee::ready()) {
		apply_wifi_state();
	}
}

loop::Result periodic(loop::Handle, loop::Value&) {
	if (!adsbee::ready()) {
		return loop::Result::done;
	}
	if (dirty) {
		apply_wifi_state();
	} else {
		query_access_point();
	}
	return loop::Result::done;
}
}  // namespace

esp_err_t apply_wifi_state() {
	if (!switch_known || !adsbee::ready()) {
		if (switch_known) {
			mark_dirty(true);
		}
		record_error(ESP_ERR_INVALID_STATE);
		return ESP_ERR_INVALID_STATE;
	}
	char command[sizeof WIFI_AP_SET_COMMAND + 8 + WIFI_AP_SSID_SIZE +
				 WIFI_AP_PASSWORD_SIZE];
	if (!format_set(wanted, command, sizeof command)) {
		record_error(ESP_ERR_INVALID_SIZE);
		return ESP_ERR_INVALID_SIZE;
	}
	if (!adsbee::request(command, {}, {}).valid()) {
		mark_dirty(true);
		record_error(ESP_ERR_INVALID_STATE);
		return ESP_ERR_INVALID_STATE;
	}
	mark_dirty(false);
	int64_t now = esp_timer_get_time();
	State::update([&](State& state) {
		state.sent_us = now;
		state.sent++;
	});
	ESP_LOGI(tag, "Wi-Fi %s", wanted.enabled ? "on" : "off");
	// The set reply has no config line, ask once the reply window is over.
	if (!loop::schedule(WIFI_AP_VERIFY_DELAY_MS,
						{"wifi verify",
						 [](loop::Handle, loop::Value&) {
							 query_access_point();
							 return loop::Result::done;
						 }})
			 .valid()) {
		record_error(ESP_ERR_NO_MEM);
	}
	return ESP_OK;
}

esp_err_t query_access_point() {
	if (!adsbee::ready()) {
		return ESP_ERR_INVALID_STATE;
	}
	if (!adsbee::request(WIFI_AP_QUERY_COMMAND, {}, {}).valid()) {
		record_error(ESP_ERR_INVALID_STATE);
		return ESP_ERR_INVALID_STATE;
	}
	return ESP_OK;
}

void on_link_attached() {
	loop::cancel(attach_step);
	attach_step =
		loop::schedule(WIFI_AP_ATTACH_DELAY_MS,
					   {"wifi attach", [](loop::Handle, loop::Value&) {
							attach_step = {};
							apply_wifi_state();
							return loop::Result::done;
						}});
	if (!attach_step.valid()) {
		record_error(ESP_ERR_NO_MEM);
	}
}

esp_err_t start(const char* ssid) {
	std::strncpy(wanted.ssid, ssid, sizeof wanted.ssid - 1);
	std::strncpy(wanted.password, WIFI_AP_PASSWORD, sizeof wanted.password - 1);
	wanted.channel = WIFI_AP_CHANNEL;
	if (!adsbee::subscribe(is_reply, on_reply, nullptr)) {
		return ESP_ERR_NO_MEM;
	}
	if (!loop::schedule_periodic(WIFI_AP_POLL_PERIOD_MS,
								 {"wifi poll", periodic})
			 .valid()) {
		return ESP_ERR_NO_MEM;
	}
	State::update([](State& state) { state.status = Status::ok; });
	return events::subscribe(events::of(events::Type::wifi_switch_changed),
							 on_switch)
			   ? ESP_OK
			   : ESP_ERR_NO_MEM;
}
}  // namespace wifi
