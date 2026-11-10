#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Operating modes of docs/firmware.md. ON is the POWER slider on, with or
// without USB. OFF is the slider off without USB: the carrier feeds nothing
// and only a supply of its own keeps the ESP running.
typedef enum {
	POWER_MODE_UNKNOWN,
	POWER_MODE_ON,
	POWER_MODE_CHARGER,
	POWER_MODE_OFF,
} power_mode_t;

typedef struct {
	bool known, value;
} power_input_t;

power_mode_t power_mode_decode(power_input_t slider_on, power_input_t vbus);
const char* power_mode_name(power_mode_t mode);
// The buzzer scheme a transition plays, NULL for a silent one. Leaving ON says
// goodbye, so a boot with the slider off stays silent.
const char* power_mode_chime(power_mode_t from, power_mode_t to);

#ifdef __cplusplus
}
#endif
