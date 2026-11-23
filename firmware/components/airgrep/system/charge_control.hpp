#pragma once

#include "charger/charge_target.hpp"
#include "esp_err.h"

// Charge control: computes the charger target after every complete charger
// and USB-C controller Poll and on a 2 s safety run, then reconciles the
// charger with it. AUTO after every start, the mode lives in RAM only.
namespace charge_control {
esp_err_t init();
charger::ControlMode get_charger_mode();
// ESP_ERR_INVALID_STATE while a battery test runs.
esp_err_t set_charger_mode(charger::ControlMode mode);
const char* get_charger_mode_name(charger::ControlMode mode);
}
