#include "buzzer.h"
#include "buzzer_pwm.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "events/events.hpp"
#include "freertos/FreeRTOS.h"
#include "loop.hpp"

namespace {
portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
buzzer_state_t state{BUZZER_MUTE_UNKNOWN, false};
bool ready;
uint32_t generation, frequency;
size_t index;
const buzzer_scheme_t* current;
loop::Handle next;

esp_pm_lock_handle_t clock_lock;

// A playing scheme holds the clock: scaling can detune the tone and light sleep
// stops it.
void playing(bool value) {
	portENTER_CRITICAL(&mutex);
	bool changed = state.playing != value;
	state.playing = value;
	portEXIT_CRITICAL(&mutex);
	if (changed && clock_lock) {
		value ? esp_pm_lock_acquire(clock_lock)
			  : esp_pm_lock_release(clock_lock);
	}
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

esp_err_t buzzer_prepare(void) {
	const bool subscribed =
		events::subscribe(events::of(events::Type::mute_switch_changed),
						  [](const events::Event& event, void*) {
							  buzzer_set_mute(event.position.on);
						  }) &&
		events::subscribe(events::of(events::Type::mode_changed),
						  [](const events::Event& event, void*) {
							  if (const char* chime = power_mode_chime(
									  event.mode.from, event.mode.to)) {
								  buzzer_play(chime, 0);
							  }
						  });
	return subscribed ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t buzzer_init(void) {
	esp_err_t result =
		esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "buzzer", &clock_lock);
	if (result == ESP_OK || result == ESP_ERR_NOT_SUPPORTED) {
		result = buzzer_pwm_init();
	}
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
