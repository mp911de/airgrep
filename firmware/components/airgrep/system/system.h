#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "power_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

// Device lifecycle. Derives the operating mode from the POWER slider and the
// charger's VBUS status and acts on each transition: chimes, and clock scaling
// in charger mode. Applies the MUTE slider to the buzzer. Subscribes to the
// expander and starts charge control and the system watchdog, which every
// 10 s starts each driver again that stopped or never showed up. Call last,
// it acts on every other component.
esp_err_t system_init(void);
power_mode_t system_mode(void);
// The health check for the current mode, one bit per component that is not
// usable. On a profile with a USR LED a 2 s task also runs it and lights the
// LED while any bit is set. The CPU bit comes from that task's last run.
uint16_t system_health(void);

#ifdef __cplusplus
}
#endif
