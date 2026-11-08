#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "buzzer_pattern.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Piezo BZ1 through U9 and C66. LEDC PWM at the 4 kHz resonance, 50 % duty.
// Mutations run on the event loop. State copies may be taken anywhere.
// Silent until the first buzzer_set_mute(false).

typedef struct { buzzer_mute_t mute; bool playing; } buzzer_state_t;
buzzer_state_t buzzer_state(void);
// Subscribes to the expander mute pin. Call before expander initialization.
esp_err_t buzzer_init(void);
esp_err_t buzzer_play(const char* scheme, uint32_t hz);
void buzzer_set_mute(bool mute);
void buzzer_off(void);

#ifdef __cplusplus
}
#endif
