#ifndef AIRGREP_RADAR_GRID_H
#define AIRGREP_RADAR_GRID_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct radar_grid_cache radar_grid_cache_t;
typedef lv_draw_buf_t *(*radar_grid_capture_t)(lv_layer_t *layer, const lv_area_t *area, lv_point_t center,
                                               int32_t outer, int32_t label_width, lv_opa_t opacity);

/* Return false when the caller must submit the native grid primitives. */
bool radar_grid_draw(radar_grid_cache_t **cache, lv_layer_t *layer, const lv_area_t *area, lv_point_t center,
                     int32_t outer, int32_t label_width, lv_color_t color, lv_opa_t opacity,
                     radar_grid_capture_t capture);
void radar_grid_release(radar_grid_cache_t **cache);

#ifdef __cplusplus
}
#endif
#endif
