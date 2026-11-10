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

// Mute gates each tone, the scheme keeps stepping silently so unmuting resumes
// it.
esp_err_t step() {
	const auto& tone = current->steps[index];
	esp_err_t result = ESP_OK;
	if (tone.on && buzzer_gate(buzzer_state().mute, true)) {
		result = buzzer_pwm_tone(buzzer_step_frequency(&tone, frequency),
								 current->volume);
	} else {
		buzzer_pwm_off();
	}
	if (result != ESP_OK) {
		buzzer_off();
		return result;
	}
	uint32_t expected = generation;
	next = loop::schedule(
		tone.ms, {"buzzer step", [expected](loop::Handle, loop::Value&) {
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
				  }});
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
	current = scheme;
	frequency = hz;
	index = 0;
	playing(true);
	return step();
}
}  // namespace

buzzer_state_t buzzer_state(void) {
	portENTER_CRITICAL(&mutex);
	buzzer_state_t result = state;
	portEXIT_CRITICAL(&mutex);
	return result;
}

namespace {
void hello() { buzzer_play("startup", 0); }
void goodbye() { buzzer_play("shutdown", 0); }

// P7 is the POWER slider. The first read reports every pin as changed, so a
// boot with the slider already on says hello too.
void power_changed(bool on) {
	if (on) {
		hello();
	} else {
		goodbye();
	}
}
}  // namespace

esp_err_t buzzer_init(void) {
	bool subscribed = expander::subscribe(
		[](const expander::State& input, uint8_t changed, void*) {
			if (!input.updated_us || input.updated_us < input.error_us) {
				return;
			}
			buzzer_set_mute(input.input.mute);
			if (changed & 0x80) {
				power_changed(input.input.power);
			}
		});
	if (!subscribed) {
		return ESP_ERR_NO_MEM;
	}
	esp_err_t result = buzzer_pwm_init();
	ready = result == ESP_OK;
	return result;
}

void buzzer_set_mute(bool mute) {
	portENTER_CRITICAL(&mutex);
	state.mute = mute ? BUZZER_MUTE_ON : BUZZER_MUTE_OFF;
	portEXIT_CRITICAL(&mutex);
	// Cut a sounding tone at once, the scheme itself continues muted.
	if (mute && ready) {
		buzzer_pwm_off();
	}
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
