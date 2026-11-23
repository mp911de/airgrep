#pragma once

#include <cstdint>

#include "adsbee_protocol.hpp"
#include "esp_err.h"
#include "locked_state.hpp"

// Sets the system clock from the ADSBee's PPS-disciplined time with
// AT+TIME_PING bursts. The first time is published as clock_set.
namespace adsbee::timesync {
struct State : StateData, LockedState<State> {};

esp_err_t start();
// Schedules a ping burst. ESP_ERR_INVALID_STATE while one is due or the link
// is down.
esp_err_t schedule_gnss_time_sync();
}
