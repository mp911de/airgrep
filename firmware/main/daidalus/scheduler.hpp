#pragma once

#include "daidalus/monitor.hpp"
#include "esp_err.h"
#include "locked_state.hpp"

namespace conflicts {
struct State : StateData, LockedState<State> {};

// Start after the event loop, settings and traffic table.
esp_err_t start();
}
