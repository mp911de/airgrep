#pragma once

#include "bq25798_i2c.hpp"
#include "charge_target.hpp"
#include "device.h"
#include "esp_err.h"
#include "i2c_bus.h"
#include "locked_state.hpp"
#include "loop.hpp"

namespace charger {
#define CHARGER_I2C_ADDR 0x6b
#define CHARGER_POLL_PERIOD_MS 250
#define CHARGER_WATCHDOG_MS 20000
#define CHARGER_MAX_SUBSCRIBERS 2

const i2c_device_config_t i2c_config = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = CHARGER_I2C_ADDR,
	.scl_speed_hz = I2C_BUS_SPEED,
	.scl_wait_us = 0,
	.flags = {},
};

struct State : StateData, LockedState<State> {};

using Completion = loop::Callable<void(esp_err_t)>;
struct ChargeResult {
	esp_err_t error = ESP_OK, restore_error = ESP_OK;
	bool initial_enabled = false, before_valid = false, after_valid = false;
	uint16_t vbat_before = 0, vbat_after = 0;
	int ibat_after_ma = 0;  // taken with vbat_after, negative while discharging
};

using ChargeCompletion = loop::Callable<void(ChargeResult)>;
esp_err_t set_charge_enabled(bool enabled, ChargeCompletion complete);
esp_err_t battery_test(ChargeCompletion complete);
esp_err_t verify_battery(ChargeCompletion complete);
bool battery_test_running();
// Writes ICHG or IINDPM in mA with a read-back.
esp_err_t set_charge_limit(unsigned ma, ChargeCompletion complete);
esp_err_t set_input_limit(unsigned ma, ChargeCompletion complete);
// Called on the loop after every complete Poll. A change of charging or of the
// chip's protection is also published as charging_started, charging_stopped
// and charger_protection_changed.
using Handler = void (*)(const State&, void*);
bool subscribe(Handler handler, void* context = nullptr);
// On the loop: writes the registers the last Poll read back differently from
// target. The hardware input limit is set before IINDPM is lowered and
// cleared only after IINDPM was raised. A failed write waits for the next
// call.
void set_charger_target(const TargetState& target);
void init(Completion complete);
}
