#include "power_mode.h"

#include <stddef.h>

power_mode_t power_mode_decode(power_input_t slider_on, power_input_t vbus) {
	if (!slider_on.known) {
		return POWER_MODE_UNKNOWN;
	}
	if (slider_on.value) {
		return POWER_MODE_ON;
	}
	if (!vbus.known) {
		return POWER_MODE_UNKNOWN;
	}
	return vbus.value ? POWER_MODE_CHARGER : POWER_MODE_OFF;
}

const char* power_mode_name(power_mode_t mode) {
	static const char* names[] = {"UNKNOWN", "ON", "CHARGER", "OFF"};
	return names[mode];
}

const char* power_mode_chime(power_mode_t from, power_mode_t to) {
	if (from == to) {
		return NULL;
	}
	if (to == POWER_MODE_ON) {
		return "startup";
	}
	return from == POWER_MODE_ON ? "shutdown" : NULL;
}
