#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Piezo BZ1 through U9 and C66. LEDC PWM at the 4 kHz resonance, 50 % duty.
// Silent until the first buzzer_set_mute(false).

esp_err_t buzzer_init(void);
void buzzer_set_mute(bool mute);
void buzzer_on(void);
void buzzer_off(void);
void buzzer_beep_ms(uint32_t ms);
