#pragma once

#include "device.h"
#include "esp_err.h"
#include "i2c_bus.h"
#include "locked_state.hpp"
#include "loop.hpp"
#include "max17048_i2c.hpp"

namespace battery {
#define BATTERY_I2C_ADDR 0x36
#define BATTERY_POLL_PERIOD_MS 2000

const i2c_device_config_t i2c_config = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = BATTERY_I2C_ADDR,
	.scl_speed_hz = I2C_BUS_SPEED,
	.scl_wait_us = 0,
	.flags = {},
};

struct State : StateData, LockedState<State> {};

using Completion = loop::Callable<void(esp_err_t)>;
void init(Completion complete);
}
