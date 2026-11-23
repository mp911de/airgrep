#pragma once

#include "display/view_state.hpp"
#include "lvgl.h"
#include "panel.hpp"

// User interface of the selected display variant. The caller initialises
// LVGL, owns the panel and input drivers and calls from the LVGL thread.

namespace display {
void create(lv_display_t* panel, const Actions& actions);
void update(const ViewState& model);
}
