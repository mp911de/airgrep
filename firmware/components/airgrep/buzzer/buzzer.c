#include "board.h"
#include "buzzer_pattern.h"
#include "buzzer_pwm.h"
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

esp_err_t buzzer_pwm_tone(uint32_t hz, uint8_t volume) {
	esp_err_t result = ledc_set_freq(MODE, TIMER, hz);
	if (result == ESP_OK) {
		result =
			ledc_set_duty(MODE, CHANNEL, buzzer_volume_duty(volume, DUTY_50));
	}
	if (result == ESP_OK) {
		result = ledc_update_duty(MODE, CHANNEL);
	}
	return result;
}

void buzzer_pwm_off(void) { ledc_stop(MODE, CHANNEL, 0); }
