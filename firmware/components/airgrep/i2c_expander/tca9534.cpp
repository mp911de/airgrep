// Copyright 2026 the original author or authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tca9534.hpp"
#include <new>
#include "esp_log.h"
#include "esp_timer.h"
#include "i2c_bus.h"
#include "sdkconfig.h"

namespace expander {
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
		i2c_master_dev_handle_t device;
		loop::Handle periodic;
		unsigned configuration_step;
		bool pulse_active;

		struct Subscriber {
			Handler handler;
			void* context;
		} subscribers[EXPANDER_MAX_SUBSCRIBERS];

		size_t subscriber_count;

		void notify(uint8_t changed) {
			const State state = State::copy();
			for (size_t i = 0; i < subscriber_count; i++) {
				subscribers[i].handler(state, changed, subscribers[i].context);
			}
		}

		void error(esp_err_t result) {
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
			esp_err_t result = i2c_master_transmit(device, data, sizeof data,
													CONFIG_AIRGREP_I2C_TIMEOUT_MS);
			if (result == ESP_OK)
				State::update([&](State& state) {
					if (reg == 1) {
						state.raw_output = value;
						state.output = decode_output(value);
					}
					else if (reg == 2) {
						state.raw_polarity = value;
						state.polarity = decode_polarity(value);
					}
					else if (reg == 3) {
						state.raw_configuration = value;
						state.configuration = decode_configuration(value);
					}
				});
			return result;
		}

		loop::Result poll(loop::Handle, loop::Value&) {
			if (!State::copy().configured) {
				static constexpr uint8_t registers[] = {1, 2, 3};
				static constexpr uint8_t values[] = {0x30, 0x00, 0xff};
				esp_err_t result = write(registers[configuration_step],
										values[configuration_step]);
				if (result != ESP_OK) {
					error(result);
					return loop::Result::done;
				}
				if (++configuration_step == 3)
					State::update([](State& state) {
						state.configured = true;
					});
				if (!loop::hsubmit("exp configure", poll)) error(ESP_ERR_NO_MEM);
				return loop::Result::done;
			}
			uint8_t raw;
			esp_err_t result = read(0, raw);
			if (result != ESP_OK) {
				error(result);
				return loop::Result::done;
			}
			uint8_t changed;
			int64_t now = esp_timer_get_time();
			State::update([&](State& state) {
				changed = state.updated_us ? state.raw_input ^ raw : 0xff;
				state.raw_input = raw;
				state.input = decode_input(raw);
				state.updated_us = now;
			});
			notify(changed);
			return loop::Result::done;
		}

		struct Pulse {
			unsigned pin;
			esp_err_t first_error = ESP_OK;
			bool retried = false;
		};

		void finish(Pulse* pulse, esp_err_t result) {
			if (pulse->first_error != ESP_OK) result = pulse->first_error;
			if (result != ESP_OK) {
				error(result);
				ESP_LOGE("exp", "P%u trigger failed: %s", pulse->pin,
						esp_err_to_name(result));
			}
			pulse_active = false;
			delete pulse;
		}

		void restore(Pulse* pulse) {
			if (pulse->pin < 4) {
				finish(pulse, ESP_OK);
				return;
			}
			if (!loop::hsubmit("exp restore latch", [pulse](loop::Handle, loop::Value&) {
				esp_err_t result =
					write(1, State::copy().raw_output | (1u << pulse->pin));
				finish(pulse, result);
				return loop::Result::done;
			}))
				finish(pulse, ESP_ERR_NO_MEM);
		}

		void release(Pulse* pulse) {
			esp_err_t result = write(
				3, State::copy().raw_configuration | (1u << pulse->pin));
			if (result == ESP_OK) {
				restore(pulse);
				return;
			}
			if (pulse->first_error == ESP_OK) pulse->first_error = result;
			if (!pulse->retried) {
				pulse->retried = true;
				if (loop::hsubmit("exp release retry", [pulse](loop::Handle, loop::Value&) {
					release(pulse);
					return loop::Result::done;
				}))
					return;
			}
			finish(pulse, result);
		}

		void assert_pin(Pulse* pulse) {
			esp_err_t result = write(
				3, State::copy().raw_configuration & ~(1u << pulse->pin));
			if (result != ESP_OK) {
				pulse->first_error = result;
			}
			// Even an unacknowledged write may have reached the chip. Always release.
			loop::Event event{
				"exp release", [pulse](loop::Handle, loop::Value&) {
					release(pulse);
					return loop::Result::done;
				}
			};
			if (!loop::schedule(EXPANDER_PULSE_MS, event).valid()) {
				if (pulse->first_error == ESP_OK) pulse->first_error = ESP_ERR_NO_MEM;
				if (!loop::hsubmit("exp emergency release", event.action))
					finish(pulse, ESP_ERR_NO_MEM);
			}
		}
	}

	bool subscribe(Handler handler, void* context) {
		if (!handler || subscriber_count == EXPANDER_MAX_SUBSCRIBERS) {
			return false;
		}
		subscribers[subscriber_count++] = {handler, context};
		return true;
	}

	loop::Handle poll_handle() { return periodic; }

	Completion init_complete;

	void finish_init(esp_err_t result) {
		if (result != ESP_OK && result != ESP_ERR_NOT_FOUND) error(result);
		if (init_complete) init_complete(result);
	}

	loop::Result identify(loop::Handle, loop::Value&) {
		uint8_t raw;
		esp_err_t result = read(0, raw);
		if (result != ESP_OK) {
			finish_init(result);
			return loop::Result::done;
		}
		State::update([&](State& state) {
			state.status = Status::ok;
			state.identity_known = true;
			state.raw_input = raw;
		});
		periodic = loop::schedule_periodic(EXPANDER_POLL_PERIOD_MS, {"exp poll", poll});
		finish_init(periodic.valid() ? ESP_OK : ESP_ERR_NO_MEM);
		return loop::Result::done;
	}

	loop::Result probe(loop::Handle, loop::Value&) {
		esp_err_t result = i2c_bus_add_if_present(&device_config, &device);
		if (result == ESP_ERR_NOT_FOUND) State::update([](State& shared) { shared.status = Status::off; });
		if (result == ESP_OK && !loop::hsubmit("exp identity", identify)) result = ESP_ERR_NO_MEM;
		if (result != ESP_OK) finish_init(result);
		return loop::Result::done;
	}

	void init(Completion complete) {
		init_complete = complete;
		if (!loop::hsubmit("exp probe", probe)) finish_init(ESP_ERR_NO_MEM);
	}

	esp_err_t trigger(unsigned pin) {
		if (pin < 2 || pin > 5) return ESP_ERR_INVALID_ARG;
		Pulse* pulse = new(std::nothrow) Pulse{pin};
		if (!pulse) return ESP_ERR_NO_MEM;
		if (!loop::hsubmit("exp trigger", [pulse](loop::Handle, loop::Value&) {
			if (pulse_active || !State::copy().configured) {
				error(ESP_ERR_INVALID_STATE);
				delete pulse;
				return loop::Result::done;
			}
			pulse_active = true;
			if (pulse->pin < 4) {
				assert_pin(pulse);
				return loop::Result::done;
			}
			esp_err_t result = write(1, State::copy().raw_output & ~(1u << pulse->pin));
			if (result != ESP_OK) {
				finish(pulse, result);
				return loop::Result::done;
			}
			if (!loop::hsubmit("exp assert", [pulse](loop::Handle, loop::Value&) {
				assert_pin(pulse);
				return loop::Result::done;
			})) {
				pulse->first_error = ESP_ERR_NO_MEM;
				restore(pulse);
			}
			return loop::Result::done;
		})) {
			delete pulse;
			return ESP_ERR_NO_MEM;
		}
		return ESP_OK;
	}
}
