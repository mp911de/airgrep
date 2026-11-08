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

#include "buzzer_pwm.h"
#include "board.h"
#include "driver/ledc.h"

#define MODE LEDC_LOW_SPEED_MODE
#define TIMER LEDC_TIMER_0
#define CHANNEL LEDC_CHANNEL_0
#define FREQ_HZ 4000
#define RESOLUTION LEDC_TIMER_10_BIT
#define DUTY_50 (1 << 9)

esp_err_t buzzer_pwm_init(void) {
	const ledc_timer_config_t timer = {
		.speed_mode = MODE,
		.timer_num = TIMER,
		.duty_resolution = RESOLUTION,
		.freq_hz = FREQ_HZ,
		.clk_cfg = LEDC_AUTO_CLK,
	};
	esp_err_t err = ledc_timer_config(&timer);
	if (err != ESP_OK) {
		return err;
	}
	const ledc_channel_config_t channel = {
		.gpio_num = BOARD_PIN_BUZZER,
		.speed_mode = MODE,
		.channel = CHANNEL,
		.timer_sel = TIMER,
		.duty = 0,
		.hpoint = 0,
	};
	return ledc_channel_config(&channel);
}

esp_err_t buzzer_pwm_tone(uint32_t hz) {
	esp_err_t result = ledc_set_freq(MODE, TIMER, hz);
	if (result == ESP_OK) result = ledc_set_duty(MODE, CHANNEL, DUTY_50);
	if (result == ESP_OK) result = ledc_update_duty(MODE, CHANNEL);
	return result;
}

void buzzer_pwm_off(void) {
	ledc_stop(MODE, CHANNEL, 0);
}
