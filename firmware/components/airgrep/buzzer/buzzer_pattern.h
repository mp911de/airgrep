#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { BUZZER_MUTE_UNKNOWN, BUZZER_MUTE_ON, BUZZER_MUTE_OFF } buzzer_mute_t;
typedef struct { bool on; uint32_t ms, hz; } buzzer_step_t;
typedef struct {
	const char* name;
	const buzzer_step_t* steps;
	size_t count;
	bool repeat;
} buzzer_scheme_t;

const buzzer_scheme_t* buzzer_scheme_find(const char* name);
const buzzer_scheme_t* buzzer_scheme_at(size_t index);
uint32_t buzzer_step_frequency(const buzzer_step_t* step, uint32_t requested_hz);
bool buzzer_gate(buzzer_mute_t mute, bool on);

#ifdef __cplusplus
}
#endif
