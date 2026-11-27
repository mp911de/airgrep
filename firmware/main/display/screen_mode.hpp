#pragma once

#include "display/view_state.hpp"
#include "system/power_mode.h"

namespace display {
constexpr Mode screen_mode(power_mode_t power, bool starting) {
	if (starting && power != POWER_MODE_OFF) {
		return Mode::starting;
	}
	return power == POWER_MODE_CHARGER ? Mode::charger : Mode::on;
}
}
