#ifndef AIRGREP_RADAR_OUTLINE_H
#define AIRGREP_RADAR_OUTLINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#define RADAR_OUTLINE_WIDTH_PX 2
#define RADAR_SYMBOL_SCALE 167
#define RADAR_OWN_SYMBOL_SCALE 211

const lv_draw_buf_t * radar_symbol_outline_mask(const lv_image_dsc_t * plain, int32_t scale);
bool radar_symbol_atlas_draw(lv_layer_t * layer, lv_draw_image_dsc_t * image, const lv_area_t * area);
/* Start at bucket zero. True means that another heading remains for this style. */
bool radar_symbol_atlas_warm_next(lv_display_t * display, bool simple, unsigned bucket);
void radar_symbol_atlas_release(void);
void radar_symbol_atlas_usage(uint32_t * masks, uint32_t * pixel_bytes);
void radar_symbol_atlas_lookups(uint32_t * hits, uint32_t * misses);

void radar_draw_symbol_image(lv_layer_t * layer, lv_draw_image_dsc_t * image, const lv_area_t * area);

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
