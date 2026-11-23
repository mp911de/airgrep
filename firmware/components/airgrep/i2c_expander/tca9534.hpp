#pragma once

#include "device.h"
#include "esp_err.h"
#include "i2c_bus.h"
#include "locked_state.hpp"
#include "loop.hpp"
#include "tca9534_i2c.hpp"

namespace expander {
#define EXPANDER_MAX_SUBSCRIBERS 8
#define EXPANDER_I2C_ADDR 0x20
#define EXPANDER_POLL_PERIOD_MS 250
#define EXPANDER_PULSE_MS 10

const i2c_device_config_t device_config = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = EXPANDER_I2C_ADDR,
	.scl_speed_hz = I2C_BUS_SPEED,
	.scl_wait_us = 0,
	.flags = {},
};

struct State : StateData, LockedState<State> {
	static bool is_valid();
};

using Completion = loop::Callable<void(esp_err_t)>;
// Called on the loop after every Poll. A flipped MUTE or Wi-Fi slider, and the
// first read of each, is also published as mute_switch_changed and
// wifi_switch_changed.
using Handler = void (*)(const State&, uint8_t changed, void*);
bool subscribe(Handler handler, void* context = nullptr);
void init(Completion complete);
// Posts a 10 ms pulse on P2 to P5. Failures land in State and the log.
esp_err_t trigger(unsigned pin);
}
