#include "radar_wedge.h"
#include "lvgl.h"
#include "src/draw/lv_draw_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_private.h"
#include "src/draw/sw/lv_draw_sw.h"
#include "src/misc/lv_area_private.h"
#include <string.h>

char radar_wedge_tag;

void radar_wedge_mix_blocks(uint16_t *dest, int blocks, const uint16_t *constants, uint32_t background_pair);

/* LVGL RGB565 mixes three packed channels at five bits of opacity precision. */
static uint32_t expand565(uint16_t color)
{
    return (color | ((uint32_t)color << 16)) & 0x07e0f81fu;
}

static uint16_t mix565(uint32_t foreground, uint16_t background, unsigned weight)
{
    uint32_t bg = expand565(background);
    uint32_t mixed = ((((foreground - bg) * weight) >> 5) + bg) & 0x07e0f81fu;
    return (uint16_t)(mixed | (mixed >> 16));
}

static int mask_run(const uint8_t *mask, int width)
{
    int length = 1;
    while(length < width && ((uintptr_t)(mask + length) & 3u) && mask[length] == mask[0]) {
        ++length;
    }
    if(((uintptr_t)(mask + length) & 3u) == 0) {
        uint32_t repeated = mask[0] * 0x01010101u;
        for(; length + 4 <= width; length += 4) {
            uint32_t word = 0;
            memcpy(&word, __builtin_assume_aligned(mask + length, 4), sizeof(word));
            if(word != repeated) {
                break;
            }
        }
    }
    while(length < width && mask[length] == mask[0]) {
        ++length;
    }
    return length;
}

static void blend_span(uint16_t *dest, int length, uint16_t color, uint32_t foreground, unsigned weight)
{
    if(weight == 0) {
        return;
    }
    while(length && ((uintptr_t)dest & 15u)) {
        *dest = mix565(foreground, *dest, weight);
        ++dest;
        --length;
    }
    int blocks = length / 8;
    if(blocks) {
        uint16_t bg = *dest;
        const uint16_t constants[] = {31, 63, (uint16_t)weight, color & 31, (color >> 5) & 63, color >> 11,
                                      mix565(foreground, bg, weight)};
        radar_wedge_mix_blocks(dest, blocks, constants, bg | ((uint32_t)bg << 16));
        dest += blocks * 8;
        length -= blocks * 8;
    }
    for(int i = 0; i < length; ++i) {
        dest[i] = mix565(foreground, dest[i], weight);
    }
}

void radar_wedge_blend_rows(uint16_t *dest, int dest_stride, const uint8_t *mask, int mask_stride,
                            int width, int height, uint16_t color, uint8_t opacity)
{
    uint32_t foreground = expand565(color);
    /* A covered byte inside a partial mask uses LV_OPA_MIX2. An unmasked row uses opacity directly. */
    unsigned full_weight = ((mask ? LV_OPA_MIX2(255u, opacity) : opacity) + 4u) >> 3;
    for(int y = 0; y < height; ++y) {
        if(mask == NULL) {
            blend_span(dest, width, color, foreground, full_weight);
        }
        else {
            for(int x = 0; x < width;) {
                uint8_t coverage = mask[x];
                if(coverage == 0 || coverage == 255) {
                    int length = mask_run(mask + x, width - x);
                    if(coverage) {
                        blend_span(dest + x, length, color, foreground, full_weight);
                    }
                    x += length;
                }
                else {
                    unsigned weight = (LV_OPA_MIX2(coverage, opacity) + 4u) >> 3;
                    dest[x] = mix565(foreground, dest[x], weight);
                    ++x;
                }
            }
            mask += mask_stride;
        }
        dest = (uint16_t *)((uint8_t *)dest + dest_stride);
    }
}

/* LVGL routes a complete circle through its border rasterizer with the original arc task. */
void radar_wedge_blend(lv_draw_task_t *task, const lv_draw_sw_blend_dsc_t *dsc)
{
    lv_layer_t *layer = task->target_layer;
    const lv_draw_dsc_base_t *base = task->draw_dsc;
    if(task->type != LV_DRAW_TASK_TYPE_ARC || base->user_data != &radar_wedge_tag ||
       layer->color_format != LV_COLOR_FORMAT_RGB565 || dsc->src_buf != NULL ||
       dsc->blend_mode != LV_BLEND_MODE_NORMAL || dsc->opa >= LV_OPA_MAX ||
       lv_draw_sw_get_blend_handler(layer->color_format) != NULL) {
        lv_draw_sw_blend(task, dsc);
        return;
    }
    lv_area_t area;
    if(dsc->opa <= LV_OPA_MIN || (dsc->mask_buf && dsc->mask_res == LV_DRAW_SW_MASK_RES_TRANSP) ||
       !lv_area_intersect(&area, dsc->blend_area, &task->clip_area)) {
        return;
    }

    LV_PROFILER_DRAW_BEGIN;
    const uint8_t *mask = NULL;
    int mask_stride = 0;
    if(dsc->mask_buf && dsc->mask_res != LV_DRAW_SW_MASK_RES_FULL_COVER) {
        mask_stride = dsc->mask_stride ? dsc->mask_stride : lv_area_get_width(dsc->mask_area);
        mask = dsc->mask_buf + mask_stride * (area.y1 - dsc->mask_area->y1) + (area.x1 - dsc->mask_area->x1);
    }
    uint16_t *dest = lv_draw_layer_go_to_xy(layer, area.x1 - layer->buf_area.x1, area.y1 - layer->buf_area.y1);
    radar_wedge_blend_rows(dest, layer->draw_buf->header.stride, mask, mask_stride,
                          lv_area_get_width(&area), lv_area_get_height(&area), lv_color_to_u16(dsc->color), dsc->opa);
    LV_PROFILER_DRAW_END;
}
