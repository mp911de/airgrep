#pragma once

#include "hwtest.hpp"
#include "lvgl.h"

namespace hwtest::render {
void periodic(Output& output, lv_display_t* panel, void (*wait_one_tick)());
void single(Output& output, lv_display_t* panel);
#ifdef CONFIG_AIRGREP_PROFILER
void profile(Output& output, lv_display_t* panel, void (*wait_one_tick)());
#endif
}
