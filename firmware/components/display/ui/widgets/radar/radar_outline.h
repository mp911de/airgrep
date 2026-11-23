#ifndef AIRGREP_RADAR_OUTLINE_H
#define AIRGREP_RADAR_OUTLINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#define RADAR_OUTLINE_WIDTH_PX 2

void radar_outline_text(lv_layer_t * layer, const char * text, const lv_font_t * font, lv_color_t color,
                        const lv_area_t * area);

/**
 * The outline of a square image of one byte per pixel, drawn around its middle.
 * @param at            the middle on the screen
 * @param scale         256 is the image as it is
 * @param rotation_deg  clockwise
 */
void radar_outline_symbol(lv_layer_t * layer, const void * symbol, lv_point_t at, lv_color_t color, int32_t scale,
                          int32_t rotation_deg);

#ifdef __cplusplus
}
#endif

#endif
