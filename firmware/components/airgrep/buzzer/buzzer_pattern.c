#include "buzzer_pattern.h"

#include <string.h>

static const buzzer_step_t short_steps[] = {{true, 50, 0}};
static const buzzer_step_t long_steps[] = {{true, 300, 0}};
static const buzzer_step_t double_steps[] = {
	{true, 50, 0}, {false, 50, 0}, {true, 50, 0}};
static const buzzer_step_t alarm_steps[] = {{true, 300, 0}, {false, 300, 0}};
static const buzzer_step_t sweep_steps[] = {
	{true, 50, 3500}, {true, 50, 3600}, {true, 50, 3700}, {true, 50, 3800},
	{true, 50, 3900}, {true, 50, 4000}, {true, 50, 4100}, {true, 50, 4200},
	{true, 50, 4300}, {true, 50, 4400}, {true, 50, 4500},
};
static const buzzer_step_t startup_steps[] = {
	{true, 80, 3200}, {false, 30, 0},   {true, 80, 3600},
	{false, 30, 0},   {true, 80, 4000},
};
static const buzzer_step_t shutdown_steps[] = {
	{true, 80, 4000}, {false, 30, 0},   {true, 80, 3600},
	{false, 30, 0},   {true, 80, 3200},
};
static const buzzer_scheme_t schemes[] = {
	{"short", short_steps, 1, false, 100},
	{"long", long_steps, 1, false, 100},
	{"double", double_steps, 3, false, 100},
	{"alarm", alarm_steps, 2, true, 100},
	{"sweep", sweep_steps, 11, false, 100},
	{"startup", startup_steps, 5, false, 10},
	{"shutdown", shutdown_steps, 5, false, 10},
};
_Static_assert(sizeof schemes / sizeof schemes[0] == BUZZER_SCHEME_COUNT,
			   "scheme count");

const buzzer_scheme_t* buzzer_scheme_find(const char* name) {
	for (size_t i = 0; i < sizeof schemes / sizeof schemes[0]; i++) {
		if (!strcmp(name, schemes[i].name)) {
			return &schemes[i];
		}
	}
	return NULL;
}

const buzzer_scheme_t* buzzer_scheme_at(size_t index) {
	return index < sizeof schemes / sizeof schemes[0] ? &schemes[index] : NULL;
}

uint32_t buzzer_step_frequency(const buzzer_step_t* step,
							   uint32_t requested_hz) {
	return step->hz ? step->hz : requested_hz ? requested_hz : 4000;
}

uint32_t buzzer_volume_duty(uint8_t volume, uint32_t full) {
	return full * (volume > 100 ? 100 : volume) / 100;
}

bool buzzer_gate(buzzer_mute_t mute, bool on) {
	return !on || mute == BUZZER_MUTE_OFF;
}
