#ifndef AIRGREP_TRAFFIC_TREND_H
#define AIRGREP_TRAFFIC_TREND_H

#include "lvgl.h"

#define TRAFFIC_TREND_SIZE 10

void traffic_draw_trend(lv_layer_t * layer, lv_point_t center, int32_t trend, lv_color_t color, bool outline);

#endif
