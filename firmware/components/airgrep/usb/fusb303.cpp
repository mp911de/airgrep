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

#include "fusb303.hpp"
#include <new>
#include "esp_timer.h"
#include "i2c_bus.h"
#include "sdkconfig.h"

namespace fusb {
	namespace {
		// FUSB303B datasheet Table 11 register map, Tables 12 and 18.
		constexpr uint8_t kRegDeviceId = 0x01;
		constexpr uint8_t kRegControl1 = 0x05;
		constexpr uint8_t kRegReset = 0x0a;
		constexpr uint8_t kRegStatus = 0x11;
		constexpr uint8_t kRegType = 0x13;
		constexpr unsigned kVersionFusb303b = 1; // DEVICE_ID VER_ID
		constexpr uint8_t kSwReset = 1 << 0; // RESET SW_RES
	}

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

	i2c_master_dev_handle_t device;
	loop::Handle periodic;
	State state;
	bool busy;
	unsigned step, generation;
	int64_t ready_us;

	void error(esp_err_t result) {
		int64_t now = esp_timer_get_time();
		State::update([&](State& shared) {
			shared.last_error = result;
			shared.error_us = now;
		});
		busy = false;
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

	void next(const char* name, loop::Result (*action)(loop::Handle, loop::Value&)) {
		unsigned current = generation;
		if (!loop::hsubmit(
			name, [current, action](loop::Handle handle, loop::Value& value) {
				if (current != generation) return loop::Result::done;
				return action(handle, value);
			}))
			error(ESP_ERR_NO_MEM);
	}

	loop::Result live(loop::Handle, loop::Value&) {
		uint8_t data[kRegType - kRegStatus + 1];
		esp_err_t result = read(kRegStatus, data, sizeof data);
		if (result != ESP_OK) {
			error(result);
			return loop::Result::done;
		}
		state.raw_status = data[0];
		state.raw_type = data[kRegType - kRegStatus];
		state.port = decode_status(state.raw_status);
		state.role = decode_type(state.raw_type);
		state.cc_limit_ma = cc_limit(state.port, state.role);
		state.updated_us = esp_timer_get_time();
		State::update([](State& shared) { shared = state; });
		busy = false;
		return loop::Result::done;
	}

	loop::Result configure(loop::Handle, loop::Value&) {
		esp_err_t result;
		switch (step++) {
		case 0: result = read(kRegControl1, &state.raw_control1);
			break;
		case 1: result = write(kRegControl1, state.raw_control1 | kControl1Enable);
			break;
		default: result = read(kRegControl1, &state.raw_control1);
			break;
		}
		if (result != ESP_OK) {
			error(result);
			return loop::Result::done;
		}
		if (step < 3) {
			next("fusb configure", configure);
			return loop::Result::done;
		}
		state.control = decode_control1(state.raw_control1);
		if (!state.control.enabled) {
			error(ESP_ERR_INVALID_RESPONSE);
			return loop::Result::done;
		}
		state.configured = true;
		State::update([](State& shared) { shared = state; });
		next("fusb sample", live);
		return loop::Result::done;
	}

	// Reads DEVICE_ID into the working copy and accepts only a FUSB303B.
	esp_err_t read_identity() {
		esp_err_t result = read(kRegDeviceId, &state.raw_identity);
		if (result != ESP_OK) return result;
		state.identity = decode_identity(state.raw_identity);
		if (state.identity.version != kVersionFusb303b) return ESP_ERR_INVALID_RESPONSE;
		state.identity_known = true;
		state.status = Status::ok;
		return ESP_OK;
	}

	loop::Result identity(loop::Handle, loop::Value&) {
		esp_err_t result = read_identity();
		if (result != ESP_OK) {
			error(result);
			return loop::Result::done;
		}
		State::update([](State& shared) { shared = state; });
		next("fusb configure", configure);
		return loop::Result::done;
	}

	loop::Result poll(loop::Handle, loop::Value&) {
		if (busy || esp_timer_get_time() < ready_us) return loop::Result::done;
		busy = true;
		step = 0;
		state = State::copy();
		next("fusb poll",
			!state.identity_known ? identity : state.configured ? live : configure);
		return loop::Result::done;
	}

	loop::Handle poll_handle() { return periodic; }

	Completion init_complete;

	void finish_init(esp_err_t result) {
		if (result != ESP_OK && result != ESP_ERR_NOT_FOUND) error(result);
		if (init_complete) init_complete(result);
	}

	loop::Result identify(loop::Handle, loop::Value&) {
		state = State::copy();
		esp_err_t result = read_identity();
		if (result != ESP_OK) {
			finish_init(result);
			return loop::Result::done;
		}
		State::update([](State& shared) { shared = state; });
		periodic = loop::schedule_periodic(FUSB_POLL_PERIOD_MS, {"fusb poll", poll});
		finish_init(periodic.valid() ? ESP_OK : ESP_ERR_NO_MEM);
		return loop::Result::done;
	}

	loop::Result probe(loop::Handle, loop::Value&) {
		esp_err_t result = i2c_bus_add_if_present(&i2c_config, &device);
		if (result == ESP_ERR_NOT_FOUND) State::update([](State& shared) { shared.status = Status::off; });
		if (result == ESP_OK && !loop::hsubmit("fusb identity", identify)) result = ESP_ERR_NO_MEM;
		if (result != ESP_OK) finish_init(result);
		return loop::Result::done;
	}

	void init(Completion complete) {
		init_complete = complete;
		// tEN applies before the first I2C access, including the probe.
		if (!loop::schedule(FUSB_ENABLE_DELAY_MS, {"fusb probe", probe}).valid())
			finish_init(ESP_ERR_NO_MEM);
	}

	esp_err_t reset() {
		bool queued = loop::hsubmit("fusb reset", [](loop::Handle, loop::Value&) {
			if (!device || esp_timer_get_time() < ready_us) {
				error(ESP_ERR_INVALID_STATE);
				return loop::Result::done;
			}
			esp_err_t result = write(kRegReset, kSwReset);
			++generation;
			busy = false;
			ready_us = esp_timer_get_time() + FUSB_ENABLE_DELAY_MS * 1000;
			// tEN restarts on SW_RES
			if (result == ESP_OK) {
				State::update([](State& shared) { shared = State{}; });
				if (!periodic.valid()) {
					periodic = loop::schedule_periodic(
						FUSB_POLL_PERIOD_MS, {"fusb poll", poll});
					if (!periodic.valid()) {
						result = ESP_ERR_NO_MEM;
						error(result);
					}
				}
			}
			else {
				State::update([](State& shared) {
					shared.identity_known = false;
					shared.configured = false;
				});
				error(result);
			}
			return loop::Result::done;
		});
		return queued ? ESP_OK : ESP_ERR_NO_MEM;
	}
}
