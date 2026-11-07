#include "buzzer.h"
#include "esp_log.h"
#include "loop.hpp"

static const char* TAG = "buzzer";

void buzzer_beep_ms(uint32_t ms) {
	if (!loop::post("buzzer on", [](loop::Handle, loop::Value&) {
		buzzer_on();
		return loop::Result::done;
	}).valid()) {
		ESP_LOGW(TAG, "beep dropped");
		return;
	}
	loop::Event off{
		"buzzer off", [](loop::Handle, loop::Value&) {
			buzzer_off();
			return loop::Result::done;
		}
	};
	if (!loop::schedule(ms, off).valid()) {
		ESP_LOGW(TAG, "beep off not scheduled, silencing now");
		buzzer_off();
	}
}
