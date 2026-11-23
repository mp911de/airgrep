#include "tca9534.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "events/events.hpp"
#include "i2c_bus.h"
#include "sdkconfig.h"
#include "subscribers.hpp"

namespace expander {
bool State::is_valid() {
	return state_usable(copy().updated_us, esp_timer_get_time());
}

namespace {
i2c_master_dev_handle_t device;
loop::Handle periodic;
unsigned configuration_step;
unsigned pulse_pin;  // zero while idle
using PulseStep = loop::Result (*)(loop::Handle, loop::Value&);
PulseStep retry_pulse;

Subscribers<Handler, EXPANDER_MAX_SUBSCRIBERS> subscribers;

void notify(uint8_t changed) { subscribers.notify(State::copy(), changed); }

void record_error(esp_err_t result) {
	int64_t now = esp_timer_get_time();
	State::update([&](State& state) {
		state.last_error = result;
		state.error_us = now;
	});
	notify(0);
}

esp_err_t read(uint8_t reg, uint8_t& value) {
	return i2c_master_transmit_receive(device, &reg, 1, &value, 1,
									   CONFIG_AIRGREP_I2C_TIMEOUT_MS);
}

esp_err_t write(uint8_t reg, uint8_t value) {
	const uint8_t data[] = {reg, value};
	const esp_err_t result = i2c_master_transmit(device, data, sizeof data,
												 CONFIG_AIRGREP_I2C_TIMEOUT_MS);
	if (result == ESP_OK) {
		State::update([&](State& state) {
			if (reg == 1) {
				state.raw_output = value;
				state.output = decode_output(value);
			} else if (reg == 2) {
				state.raw_polarity = value;
				state.polarity = decode_polarity(value);
			} else if (reg == 3) {
				state.raw_configuration = value;
				state.configuration = decode_configuration(value);
			}
		});
	}
	return result;
}

loop::Result poll_expander(loop::Handle handle, loop::Value& value) {
	if (retry_pulse) {
		return retry_pulse(handle, value);
	}
	if (!State::copy().configured) {
		static constexpr uint8_t registers[] = {1, 2, 3};
		static constexpr uint8_t values[] = {0x30, 0x00, 0xff};
		const esp_err_t result =
			write(registers[configuration_step], values[configuration_step]);
		if (result != ESP_OK) {
			loop::cancel(periodic);
			periodic = {};
			configuration_step = 0;
			record_error(result);
			return loop::Result::done;
		}
		if (++configuration_step == 3) {
			State::update([](State& state) { state.configured = true; });
		}
		if (!loop::try_submit("exp configure", poll_expander)) {
			record_error(ESP_ERR_NO_MEM);
		}
		return loop::Result::done;
	}
	uint8_t raw = 0;
	const esp_err_t result = read(0, raw);
	if (result != ESP_OK) {
		record_error(result);
		return loop::Result::done;
	}
	uint8_t changed = 0;
	int64_t now = esp_timer_get_time();
	State::update([&](State& state) {
		changed = state.updated_us ? state.raw_input ^ raw : 0xff;
		state.raw_input = raw;
		state.input = decode_input(raw);
		state.updated_us = now;
	});
	// The switches go first, so the mode that system derives from this read
	// is published last.
	const Input input = decode_input(raw);
	if (changed & kMutePin) {
		events::publish(events::Event::mute_switch_changed({input.mute}));
	}
	if (changed & kWifiPin) {
		events::publish(events::Event::wifi_switch_changed({input.wifi}));
	}
	notify(changed);
	return loop::Result::done;
}

void retry_on_poll(PulseStep action, esp_err_t error) {
	retry_pulse = action;
	record_error(error);
}

loop::Result restore_latch(loop::Handle, loop::Value&) {
	const esp_err_t result =
		write(1, State::copy().raw_output | (1u << pulse_pin));
	if (result != ESP_OK) {
		retry_on_poll(restore_latch, result);
	} else {
		retry_pulse = nullptr;
		pulse_pin = 0;
	}
	return loop::Result::done;
}

void finish_release() {
	retry_pulse = nullptr;
	if (pulse_pin < 4) {
		pulse_pin = 0;
	} else if (!loop::try_submit("exp restore latch", restore_latch)) {
		retry_on_poll(restore_latch, ESP_ERR_NO_MEM);
	}
}

loop::Result release_pin(loop::Handle, loop::Value&) {
	const esp_err_t result =
		write(3, State::copy().raw_configuration | (1u << pulse_pin));
	if (result != ESP_OK) {
		retry_on_poll(release_pin, result);
	} else {
		finish_release();
	}
	return loop::Result::done;
}

loop::Result assert_pin(loop::Handle, loop::Value&) {
	const auto release =
		loop::schedule(EXPANDER_PULSE_MS, {"exp release", release_pin});
	if (!release.valid()) {
		record_error(ESP_ERR_NO_MEM);
		finish_release();
		return loop::Result::done;
	}
	const esp_err_t result =
		write(3, State::copy().raw_configuration & ~(1u << pulse_pin));
	// The width starts at the direction write, including one with a lost ACK.
	// Only this loop can dispatch the release and this action still owns it.
	if (!loop::postpone(release, EXPANDER_PULSE_MS)) {
		retry_on_poll(release_pin, ESP_ERR_INVALID_STATE);
	}
	if (result != ESP_OK) {
		record_error(result);
	}
	return loop::Result::done;
}
}

bool subscribe(Handler handler, void* context) {
	return subscribers.add(handler, context);
}

namespace {
Completion init_complete;

void finish_init(esp_err_t result) {
	if (result != ESP_OK && result != ESP_ERR_NOT_FOUND) {
		record_error(result);
	}
	if (init_complete) {
		init_complete(result);
	}
}

loop::Result identify_at_init(loop::Handle, loop::Value&) {
	uint8_t raw = 0;
	const esp_err_t result = read(0, raw);
	if (result != ESP_OK) {
		finish_init(result);
		return loop::Result::done;
	}
	State::update([&](State& state) {
		state.status = Status::ok;
		state.identity_known = true;
		state.raw_input = raw;
	});
	periodic = loop::schedule_periodic(EXPANDER_POLL_PERIOD_MS,
									   {"exp poll", poll_expander});
	finish_init(periodic.valid() ? ESP_OK : ESP_ERR_NO_MEM);
	return loop::Result::done;
}

loop::Result probe(loop::Handle, loop::Value&) {
	esp_err_t result = i2c_bus_add_if_present(&device_config, &device);
	if (result == ESP_ERR_NOT_FOUND) {
		State::update([](State& shared) { shared.status = Status::off; });
	}
	if (result == ESP_OK &&
		!loop::try_submit("exp identity", identify_at_init)) {
		result = ESP_ERR_NO_MEM;
	}
	if (result != ESP_OK) {
		finish_init(result);
	}
	return loop::Result::done;
}
}

void init(Completion complete) {
	if (loop::pending(periodic)) {
		return;
	}
	init_complete = complete;
	if (!loop::try_submit("exp probe", probe)) {
		finish_init(ESP_ERR_NO_MEM);
	}
}

esp_err_t trigger(unsigned pin) {
	if (pin < 2 || pin > 5) {
		return ESP_ERR_INVALID_ARG;
	}
	const bool queued = loop::try_submit(
		"exp trigger", [pin](loop::Handle handle, loop::Value& value) {
			const State current = State::copy();
			if (pulse_pin || !current.configured) {
				record_error(ESP_ERR_INVALID_STATE);
				return loop::Result::done;
			}
			pulse_pin = pin;
			if (pin < 4) {
				return assert_pin(handle, value);
			}
			const esp_err_t result =
				write(1, current.raw_output & ~(1u << pin));
			if (result != ESP_OK) {
				record_error(result);
				finish_release();
			} else if (!loop::try_submit("exp assert", assert_pin)) {
				record_error(ESP_ERR_NO_MEM);
				finish_release();
			}
			return loop::Result::done;
		});
	return queued ? ESP_OK : ESP_ERR_NO_MEM;
}
}
