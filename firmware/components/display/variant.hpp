#pragma once

#include "display/view_state.hpp"
#include "lvgl.h"

namespace display::variant {
void build(lv_obj_t* screen, const Actions& handlers);
}
