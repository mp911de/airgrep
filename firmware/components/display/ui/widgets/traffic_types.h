#ifndef AIRGREP_TRAFFIC_TYPES_H
#define AIRGREP_TRAFFIC_TYPES_H

#include "airgrep_ui_gen.h"

typedef enum {
    TRAFFIC_LEVEL_OTHER,
    TRAFFIC_LEVEL_PROXIMATE,
    TRAFFIC_LEVEL_ADVISORY,
    TRAFFIC_LEVEL_ALERT,
} traffic_level_t;

#define TRAFFIC_COLOR_OTHER lv_color_hex(0x8e8e93)
#define TRAFFIC_COLOR_ON_RED lv_color_hex(0xffffff)
#define TRAFFIC_COLOR_ON_YELLOW lv_color_hex(0x000000)

typedef int8_t traffic_vertical_trend_t;
enum { TRAFFIC_DESCENDING = -1, TRAFFIC_LEVEL_FLIGHT = 0, TRAFFIC_CLIMBING = 1 };

#define TRAFFIC_MINUS "−"

static inline void traffic_format_altitude(char * text, size_t size, int32_t relative_altitude_ft, bool unknown)
{
    if(unknown) {
        lv_snprintf(text, size, "–");
        return;
    }
    const int32_t hundreds = LV_ABS(relative_altitude_ft) / 100;
    lv_snprintf(text, size, "%s%02d", relative_altitude_ft < 0 ? TRAFFIC_MINUS : "+", (int)hundreds);
}

#endif
