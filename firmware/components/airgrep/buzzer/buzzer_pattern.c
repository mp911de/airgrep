// Copyright 2026 the original author or authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "buzzer_pattern.h"
#include <string.h>

static const buzzer_step_t short_steps[] = {{true, 50, 0}};
static const buzzer_step_t long_steps[] = {{true, 300, 0}};
static const buzzer_step_t double_steps[] = {
	{true, 50, 0}, {false, 50, 0}, {true, 50, 0}
};
static const buzzer_step_t alarm_steps[] = {{true, 300, 0}, {false, 300, 0}};
static const buzzer_step_t sweep_steps[] = {
	{true, 50, 3500}, {true, 50, 3600}, {true, 50, 3700}, {true, 50, 3800},
	{true, 50, 3900}, {true, 50, 4000}, {true, 50, 4100}, {true, 50, 4200},
	{true, 50, 4300}, {true, 50, 4400}, {true, 50, 4500},
};
static const buzzer_scheme_t schemes[] = {
	{"short", short_steps, 1, false}, {"long", long_steps, 1, false},
	{"double", double_steps, 3, false}, {"alarm", alarm_steps, 2, true},
	{"sweep", sweep_steps, 11, false},
};

const buzzer_scheme_t* buzzer_scheme_find(const char* name) {
	for (size_t i = 0; i < sizeof schemes / sizeof schemes[0]; i++)
		if (!strcmp(name, schemes[i].name)) return &schemes[i];
	return NULL;
}

const buzzer_scheme_t* buzzer_scheme_at(size_t index) {
	return index < sizeof schemes / sizeof schemes[0] ? &schemes[index] : NULL;
}

uint32_t buzzer_step_frequency(const buzzer_step_t* step, uint32_t requested_hz) {
	return step->hz ? step->hz : requested_hz ? requested_hz : 4000;
}

bool buzzer_gate(buzzer_mute_t mute, bool on) {
	return !on || mute == BUZZER_MUTE_OFF;
}
