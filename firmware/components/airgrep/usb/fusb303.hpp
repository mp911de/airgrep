#pragma once

#include "device.h"
#include "esp_err.h"
#include "fusb303_i2c.hpp"
#include "i2c_bus.h"
#include "locked_state.hpp"
#include "loop.hpp"

namespace fusb {
#define FUSB_I2C_ADDR 0x21
#define FUSB_POLL_PERIOD_MS 250
#define FUSB_MAX_SUBSCRIBERS 2
// tEN: no I2C access within 100 ms after a soft reset. At power-up it ran out
// before boot.
#define FUSB_RESET_DELAY_MS 100

const i2c_device_config_t i2c_config = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = FUSB_I2C_ADDR,
	.scl_speed_hz = I2C_BUS_SPEED,
	.scl_wait_us = 0,
	.flags = {},
};

struct State : StateData, LockedState<State> {};

using Completion = loop::Callable<void(esp_err_t)>;
// Called on the loop after every complete Poll. An attach, a detach and a
// changed CC advertisement are also published as usb_plugged, usb_unplugged
// and usb_power_changed.
using Handler = void (*)(const State&, void*);
bool subscribe(Handler handler, void* context = nullptr);
void init(Completion complete);
// Posts the soft reset. The outcome shows in State on the next poll.
esp_err_t reset();
}
