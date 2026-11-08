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

#include "buzzer.h"
#include "buzzer_pwm.h"
#include "esp_log.h"
#include "i2c_expander/tca9534.hpp"
#include "loop.hpp"

namespace {
	portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
	buzzer_state_t state{BUZZER_MUTE_UNKNOWN, false};
	bool ready;
	uint32_t generation, frequency;
	size_t index;
	const buzzer_scheme_t* current;
	loop::Handle next;

	void playing(bool value) {
		portENTER_CRITICAL(&mutex);
		state.playing = value;
		portEXIT_CRITICAL(&mutex);
	}

	esp_err_t step() {
		const auto& tone = current->steps[index];
		if (!buzzer_gate(buzzer_state().mute, tone.on)) {
			buzzer_off();
			return ESP_OK;
		}
		esp_err_t result = ESP_OK;
		if (tone.on) result = buzzer_pwm_tone(buzzer_step_frequency(&tone, frequency));
		else buzzer_pwm_off();
		if (result != ESP_OK) {
			buzzer_off();
			return result;
		}
		uint32_t expected = generation;
		next = loop::schedule(tone.ms, {
								"buzzer step", [expected](loop::Handle, loop::Value&) {
									if (expected != generation) {
										return loop::Result::done;
									}
									if (++index == current->count) {
										if (!current->repeat) {
											buzzer_off();
											return loop::Result::done;
										}
										index = 0;
									}
									esp_err_t stepped = step();
									if (stepped != ESP_OK) {
										ESP_LOGE("buzzer", "scheme stopped: %s",
												esp_err_to_name(stepped));
									}
									return loop::Result::done;
								}
							});
		if (!next.valid()) {
			buzzer_off();
			return ESP_ERR_NO_MEM;
		}
		return ESP_OK;
	}

	esp_err_t start(const buzzer_scheme_t* scheme, uint32_t hz) {
		if (!ready) {
			return ESP_ERR_INVALID_STATE;
		}
		buzzer_off();
		if (!buzzer_gate(buzzer_state().mute, true)) {
			return ESP_OK;
		}
		current = scheme;
		frequency = hz;
		index = 0;
		playing(true);
		return step();
	}
}

buzzer_state_t buzzer_state(void) {
	portENTER_CRITICAL(&mutex);
	buzzer_state_t result = state;
	portEXIT_CRITICAL(&mutex);
	return result;
}

esp_err_t buzzer_init(void) {
	bool subscribed = expander::subscribe([](const expander::State& input, uint8_t, void*) {
		if (input.updated_us && input.updated_us >= input.error_us) {
			buzzer_set_mute(input.input.mute);
		}
	});
	if (!subscribed) return ESP_ERR_NO_MEM;
	esp_err_t result = buzzer_pwm_init();
	ready = result == ESP_OK;
	return result;
}

void buzzer_set_mute(bool mute) {
	portENTER_CRITICAL(&mutex);
	state.mute = mute ? BUZZER_MUTE_ON : BUZZER_MUTE_OFF;
	portEXIT_CRITICAL(&mutex);
	if (mute) buzzer_off();
}

esp_err_t buzzer_play(const char* name, uint32_t hz) {
	const auto* scheme = buzzer_scheme_find(name);
	return scheme ? start(scheme, hz) : ESP_ERR_INVALID_ARG;
}

void buzzer_off(void) {
	++generation;
	loop::cancel(next);
	next = {};
	if (ready) {
		buzzer_pwm_off();
	}
	playing(false);
}

