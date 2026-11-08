#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t buzzer_pwm_init(void);
esp_err_t buzzer_pwm_tone(uint32_t hz);
void buzzer_pwm_off(void);

#ifdef __cplusplus
}
#endif
