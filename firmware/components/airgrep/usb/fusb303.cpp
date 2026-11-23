#include "fusb303.hpp"

#include "esp_timer.h"
#include "events/events.hpp"
#include "i2c_bus.h"
#include "sdkconfig.h"
#include "subscribers.hpp"

namespace fusb {
namespace {
// FUSB303B datasheet Table 11 register map, Tables 12 and 18.
constexpr uint8_t kRegDeviceId = 0x01;
constexpr uint8_t kRegControl1 = 0x05;
constexpr uint8_t kRegReset = 0x0a;
constexpr uint8_t kRegStatus = 0x11;
constexpr uint8_t kRegStatus1 = 0x12;
constexpr uint8_t kRegType = 0x13;
constexpr unsigned kVersionFusb303b = 1;  // DEVICE_ID VER_ID
constexpr uint8_t kSwReset = 1 << 0;      // RESET SW_RES

i2c_master_dev_handle_t device;
loop::Handle periodic;
State poll_state;
bool poll_in_progress;
unsigned generation;  // bumped by reset, orphans the steps of the running chain
int64_t earliest_i2c_access_us;
Completion init_complete;

Subscribers<Handler, FUSB_MAX_SUBSCRIBERS> subscribers;

void publish() {
	State::update([](State& shared) { shared = poll_state; });
}

void fail_poll(esp_err_t result) {
	int64_t now = esp_timer_get_time();
	State::update([&](State& shared) {
		shared.last_error = result;
		shared.error_us = now;
	});
	poll_in_progress = false;
}

void stop(esp_err_t result) {
	fail_poll(result);
	loop::cancel(periodic);
	periodic = {};
}

esp_err_t read(uint8_t reg, uint8_t* bytes, size_t length = 1) {
	return i2c_master_transmit_receive(device, &reg, 1, bytes, length,
									   CONFIG_AIRGREP_I2C_TIMEOUT_MS);
}

esp_err_t write(uint8_t reg, uint8_t value) {
	const uint8_t bytes[] = {reg, value};
	return i2c_master_transmit(device, bytes, sizeof bytes,
							   CONFIG_AIRGREP_I2C_TIMEOUT_MS);
}

using Step = loop::Result (*)(loop::Handle, loop::Value&);

void submit_step(const char* name, Step step) {
	const unsigned current = generation;
	if (!loop::try_submit(
			name, [current, step](loop::Handle handle, loop::Value& value) {
				if (current != generation) {
					return loop::Result::done;
				}
				return step(handle, value);
			})) {
		fail_poll(ESP_ERR_NO_MEM);
	}
}

void finish_init(esp_err_t result) {
	if (result != ESP_OK && result != ESP_ERR_NOT_FOUND) {
		fail_poll(result);
	}
	if (init_complete) {
		init_complete(result);
	}
	init_complete = {};
}

// A plug carries the advertisement it came with. Later only a change of the
// advertisement is published.
void publish_events() {
	static bool attached;
	static uint16_t advertised_ma;
	const bool plug =
		events::update_if_changed(attached, poll_state.port.attached);
	const bool power = events::update_if_changed(
		advertised_ma, uint16_t(poll_state.cc_limit_ma));
	if (plug) {
		events::publish(
			events::Event::usb_attachment_changed(attached, {advertised_ma}));
	} else if (power) {
		events::publish(events::Event::usb_power_changed({advertised_ma}));
	}
}

loop::Result read_live_status(loop::Handle, loop::Value&) {
	uint8_t data[kRegType - kRegStatus + 1];
	const esp_err_t result = read(kRegStatus, data, sizeof data);
	if (result != ESP_OK) {
		fail_poll(result);
		return loop::Result::done;
	}
	poll_state.raw_status = data[0];
	poll_state.raw_status1 = data[kRegStatus1 - kRegStatus];
	poll_state.raw_type = data[kRegType - kRegStatus];
	poll_state.port = decode_status(poll_state.raw_status);
	poll_state.remedy = decode_remedy(poll_state.raw_status1);
	poll_state.role = decode_type(poll_state.raw_type);
	poll_state.cc_limit_ma = cc_limit(poll_state.port, poll_state.role);
	poll_state.updated_us = esp_timer_get_time();
	publish();
	poll_in_progress = false;
	subscribers.notify(poll_state);
	publish_events();
	return loop::Result::done;
}

loop::Result configure(loop::Handle, loop::Value&) {
	esp_err_t result = read(kRegControl1, &poll_state.raw_control1);
	if (result == ESP_OK) {
		result = write(kRegControl1, poll_state.raw_control1 | kControl1Enable);
	}
	if (result == ESP_OK) {
		result = read(kRegControl1, &poll_state.raw_control1);
	}
	if (result == ESP_OK) {
		poll_state.control = decode_control1(poll_state.raw_control1);
		if (!poll_state.control.enabled) {
			result = ESP_ERR_INVALID_RESPONSE;
		}
	}
	if (result != ESP_OK) {
		stop(result);
		return loop::Result::done;
	}
	poll_state.configured = true;
	publish();
	submit_step("fusb sample", read_live_status);
	return loop::Result::done;
}

loop::Result read_identity_step(loop::Handle, loop::Value&) {
	esp_err_t result = read(kRegDeviceId, &poll_state.raw_identity);
	if (result == ESP_OK) {
		poll_state.identity = decode_identity(poll_state.raw_identity);
		if (poll_state.identity.version != kVersionFusb303b) {
			result = ESP_ERR_INVALID_RESPONSE;
		}
	}
	if (result != ESP_OK) {
		stop(result);
		finish_init(result);
		return loop::Result::done;
	}
	poll_state.identity_known = true;
	poll_state.status = Status::ok;
	publish();
	finish_init(ESP_OK);
	submit_step("fusb configure", configure);
	return loop::Result::done;
}

loop::Result poll_usb_c(loop::Handle, loop::Value&) {
	if (poll_in_progress || esp_timer_get_time() < earliest_i2c_access_us) {
		return loop::Result::done;
	}
	poll_in_progress = true;
	poll_state = State::copy();
	submit_step("fusb poll", !poll_state.identity_known ? read_identity_step
							 : !poll_state.configured   ? configure
														: read_live_status);
	return loop::Result::done;
}
}

bool subscribe(Handler handler, void* context) {
	return subscribers.add(handler, context);
}

// EN_N is tied to GND on the carrier, so tEN ran out while the chip booted.
// The first access can follow at once.
void init(Completion complete) {
	if (periodic.valid()) {
		return;
	}
	init_complete = complete;
	esp_err_t result = i2c_bus_add_if_present(&i2c_config, &device);
	if (result == ESP_ERR_NOT_FOUND) {
		State::update([](State& shared) { shared.status = Status::off; });
	}
	if (result == ESP_OK) {
		periodic = loop::schedule_periodic(FUSB_POLL_PERIOD_MS,
										   {"fusb poll", poll_usb_c});
		if (!periodic.valid()) {
			result = ESP_ERR_NO_MEM;
		}
	}
	if (result == ESP_OK && !loop::try_submit("fusb poll", poll_usb_c)) {
		result = ESP_ERR_NO_MEM;
	}
	if (result != ESP_OK) {
		finish_init(result);
	}
}

esp_err_t reset() {
	const bool queued =
		loop::try_submit("fusb reset", [](loop::Handle, loop::Value&) {
			if (!device || esp_timer_get_time() < earliest_i2c_access_us) {
				fail_poll(ESP_ERR_INVALID_STATE);
				return loop::Result::done;
			}
			const esp_err_t result = write(kRegReset, kSwReset);
			++generation;
			poll_in_progress = false;
			earliest_i2c_access_us =
				esp_timer_get_time() + int64_t{FUSB_RESET_DELAY_MS} * 1000;
			if (result == ESP_OK) {
				State::update([](State& shared) { shared = State{}; });
			} else {
				fail_poll(result);
			}
			return loop::Result::done;
		});
	return queued ? ESP_OK : ESP_ERR_NO_MEM;
}
}
