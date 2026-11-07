#include "buzzer.h"
#include "board.h"
#include "driver/ledc.h"

#define MODE LEDC_LOW_SPEED_MODE
#define TIMER LEDC_TIMER_0
#define CHANNEL LEDC_CHANNEL_0
#define FREQ_HZ 4000
#define RESOLUTION LEDC_TIMER_10_BIT
#define DUTY_50 (1 << 9)

static bool s_muted = true;

esp_err_t buzzer_init(void) {
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

void buzzer_set_mute(bool mute) {
	s_muted = mute;
	if (mute) {
		buzzer_off();
	}
}

void buzzer_on(void) {
	if (s_muted) {
		return;
	}
	ledc_set_duty(MODE, CHANNEL, DUTY_50);
	ledc_update_duty(MODE, CHANNEL);
}

void buzzer_off(void) {
	ledc_stop(MODE, CHANNEL, 0);
}
