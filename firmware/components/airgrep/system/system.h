#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "power_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

// Device lifecycle. Derives the operating mode from the POWER slider and the
// charger's VBUS status and publishes each transition as mode_changed.
// Subscribes to the expander and starts charge control and the system
// watchdog, which every 10 s starts each driver again that stopped or never
// showed up. Call last, it acts on every other component.
esp_err_t system_init(void);
// Subscribes the clock scaling of charger mode to mode_changed.
esp_err_t system_prepare(void);
power_mode_t system_mode(void);
// The health check for the current mode, one bit per component that is not
// usable. Every 2 s the loop also runs it, publishes health_changed when the
// mask differs and, on a profile with a USR LED, lights the LED while any bit
// is set. The CPU bit comes from that run and stays 0 without
// CONFIG_AIRGREP_CPU_METRICS.
uint16_t system_health(void);

#ifdef __cplusplus
}
#endif
