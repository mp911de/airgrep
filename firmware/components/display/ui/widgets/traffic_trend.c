#include "traffic_trend.h"
#include "radar/radar_outline.h"
#include <math.h>
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl_private.h"
#else
    #include "lvgl_private/lvgl_private.h"
#endif

/* The sketch's 1.65-unit corner radius in a 12-unit view box. The three arc centres
 * form a sharp triangle. Distance from it defines both the rounded fill and its outline. */
static float coverage(float x, float y, float radius)
{
    const float corner = TRAFFIC_TREND_SIZE * (1.65f / 12);
    const float extent = TRAFFIC_TREND_SIZE / 2.0f - corner;
    x = fabsf(x);
    if(y <= extent && 2 * x <= y + extent) {
        return 1;
    }
    float along = LV_CLAMP(0.0f, (x + 2 * (y + extent)) / 5, extent);
    float side = hypotf(x - along, y + extent - 2 * along);
    float base = hypotf(fmaxf(x - extent, 0), y - extent);
    return LV_CLAMP(0.0f, corner + radius + 0.5f - fminf(side, base), 1.0f);
}

static lv_draw_buf_t * masks[2][2];
static lv_display_t * owner;

static void release_masks(lv_event_t * e)
{
    if(lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    for(size_t o = 0; o < 2; o++) {
        for(size_t d = 0; d < 2; d++) {
            if(masks[o][d]) {
                lv_image_cache_drop(masks[o][d]);
                lv_draw_buf_destroy(masks[o][d]);
                masks[o][d] = NULL;
            }
        }
    }
    owner = NULL;
}

static void attach_display(void)
{
    if(owner) {
        return;
    }
    lv_display_t * display = lv_refr_get_disp_refreshing();
    if(!display) {
        display = lv_display_get_default();
    }
    if(display && lv_display_add_event_cb(display, release_masks, LV_EVENT_DELETE, NULL)) {
        owner = display;
    }
}

void traffic_draw_trend(lv_layer_t * layer, lv_point_t center, int32_t trend, lv_color_t color, bool outline)
{
    if(!trend) {
        return;
    }
    lv_draw_buf_t ** mask = &masks[outline][trend < 0];
    int32_t radius = outline ? RADAR_OUTLINE_WIDTH_PX : 0;
    int32_t size = TRAFFIC_TREND_SIZE + 2 * radius;
    if(!*mask) {
        attach_display();
        *mask = lv_draw_buf_create(size, size, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
        if(!*mask) {
            return;
        }
        const float middle = (float)size / 2.0f;
        for(int32_t y = 0; y < size; y++) {
            uint8_t * row = (*mask)->data + (size_t)(trend > 0 ? y : size - 1 - y) * (*mask)->header.stride;
            for(int32_t x = 0; x < size; x++) {
                row[x] = (uint8_t)lroundf(255 * coverage((float)x + 0.5f - middle, (float)y + 0.5f - middle, (float)radius));
            }
        }
        lv_draw_buf_flush_cache(*mask, NULL);
    }
    lv_draw_image_dsc_t image;
    lv_draw_image_dsc_init(&image);
    image.src = *mask;
    image.recolor = color;
    image.recolor_opa = LV_OPA_COVER;
    lv_area_t area = {center.x - size / 2, center.y - size / 2, center.x + size / 2 - 1, center.y + size / 2 - 1};
    lv_draw_image(layer, &image, &area);
}
