#pragma once

#include "esp_err.h"
#include "power_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

// Device lifecycle. Derives the operating mode from the POWER slider and the
// charger's VBUS status and acts on each transition: chimes, and clock scaling
// in charger mode. Applies the MUTE slider to the buzzer. Subscribes to the
// expander. Call after buzzer and before expander initialization.
esp_err_t system_init(void);
power_mode_t system_mode(void);

#ifdef __cplusplus
}
#endif
