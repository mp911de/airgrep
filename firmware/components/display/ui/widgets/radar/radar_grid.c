#include "radar_grid.h"
#include "src/draw/lv_draw_private.h"
#include "src/draw/sw/blend/lv_draw_sw_blend_private.h"
#include "src/draw/sw/lv_draw_sw.h"
#include "src/misc/lv_area_private.h"

struct radar_grid_cache {
    int32_t label_width;
    uint16_t width;
    lv_opa_t opacity;
    uint16_t row_offsets[];
};

void radar_grid_release(radar_grid_cache_t **cache)
{
    if(*cache) {
        lv_free(*cache);
        *cache = NULL;
    }
}

static bool layer_ready(lv_layer_t *layer)
{
    /* With no draw threads, software and the blocking PPA both finish in dispatch. */
    const lv_draw_buf_t *buf = layer->draw_buf;
    if(LV_USE_OS != LV_OS_NONE || LV_USE_DRAW_SW_ASM != LV_DRAW_SW_ASM_NONE ||
       layer->color_format != LV_COLOR_FORMAT_RGB565 || layer->opa != LV_OPA_COVER || layer->recolor.alpha ||
       !buf || !buf->data || buf->header.cf != LV_COLOR_FORMAT_RGB565 ||
       ((uintptr_t)buf->data & 1u) || (buf->header.stride & 1u) ||
       lv_draw_get_unit_count() != 1u + LV_USE_PPA || lv_draw_sw_get_blend_handler(LV_COLOR_FORMAT_RGB565) ||
       lv_draw_sw_get_blend_handler(LV_COLOR_FORMAT_A8)) {
        return false;
    }
#if LV_DRAW_TRANSFORM_USE_MATRIX
    if(!lv_matrix_is_identity(&layer->matrix)) {
        return false;
    }
#endif
    /* Finish earlier work in paint order. A task being configured by a draw event has no unit yet. */
    unsigned waiting = 0;
    for(const lv_draw_task_t *task = layer->draw_task_head; task; task = task->next) {
        if(task->state == LV_DRAW_TASK_STATE_FINISHED || task->state == LV_DRAW_TASK_STATE_FAILED) {
            continue;
        }
        if(task->state != LV_DRAW_TASK_STATE_WAITING || task->preferred_draw_unit_id == LV_DRAW_UNIT_NONE) {
            return false;
        }
        ++waiting;
    }
    if(waiting) {
        for(unsigned i = 0; i < waiting; ++i) {
            if(!lv_draw_dispatch_layer(layer->display, layer)) {
                return false;
            }
        }
        for(const lv_draw_task_t *task = layer->draw_task_head; task; task = task->next) {
            if(task->state != LV_DRAW_TASK_STATE_FINISHED && task->state != LV_DRAW_TASK_STATE_FAILED) {
                return false;
            }
        }
    }
    return true;
}

/* Each row holds runs of nonzero RGB565 blend weights: x (little endian), length, weights. */
static size_t encode(const lv_draw_buf_t *mask, radar_grid_cache_t *cache)
{
    unsigned width = mask->header.w;
    uint8_t *data = cache ? (uint8_t *)&cache->row_offsets[width + 1u] : NULL;
    size_t size = 0;
    for(unsigned y = 0; y < width; ++y) {
        const uint8_t *row = mask->data + (size_t)y * mask->header.stride;
        if(cache) {
            cache->row_offsets[y] = (uint16_t)size;
        }
        for(unsigned x = 0; x < width;) {
            if(row[x] < 4u) {
                ++x;
                continue;
            }
            unsigned start = x;
            while(x < width && x - start < UINT8_MAX && row[x] >= 4u) {
                if(data) {
                    data[size + 3u + x - start] = (uint8_t)(((unsigned)row[x] + 4u) >> 3);
                }
                ++x;
            }
            unsigned count = x - start;
            if(data) {
                data[size] = (uint8_t)start;
                data[size + 1] = (uint8_t)(start >> 8);
                data[size + 2] = (uint8_t)count;
            }
            size += 3u + count;
        }
    }
    if(cache) {
        cache->row_offsets[width] = (uint16_t)size;
    }
    return size;
}

static radar_grid_cache_t *pack(const lv_draw_buf_t *mask, int32_t label_width, lv_opa_t opacity)
{
    size_t size = encode(mask, NULL);
    if(size > UINT16_MAX) {
        return NULL;
    }
    unsigned width = mask->header.w;
    radar_grid_cache_t *cache = lv_malloc(sizeof(*cache) + (width + 1u) * sizeof(uint16_t) + size);
    if(cache) {
        cache->width = (uint16_t)width;
        cache->label_width = label_width;
        cache->opacity = opacity;
        encode(mask, cache);
    }
    return cache;
}

static void replay(const radar_grid_cache_t *cache, const lv_layer_t *layer, const lv_area_t *area,
                   const lv_area_t *clip, uint16_t color)
{
    const uint8_t *data = (const uint8_t *)&cache->row_offsets[cache->width + 1u];
    uint32_t fg = LV_COLOR_MIX_16_PREPARE(color);
    for(int32_t y = clip->y1; y <= clip->y2; ++y) {
        unsigned row = (unsigned)(y - area->y1);
        const uint8_t *span = data + cache->row_offsets[row];
        const uint8_t *end = data + cache->row_offsets[row + 1u];
        uint16_t *pixels = (uint16_t *)(layer->draw_buf->data +
                           (size_t)(y - layer->buf_area.y1) * layer->draw_buf->header.stride);
        while(span < end) {
            int32_t x = area->x1 + span[0] + ((int32_t)span[1] << 8);
            unsigned count = span[2];
            span += 3;
            int32_t first = LV_MAX(x, clip->x1);
            int32_t last = LV_MIN(x + (int32_t)count - 1, clip->x2);
            for(int32_t i = first; i <= last; ++i) {
                uint16_t *dest = &pixels[i - layer->buf_area.x1];
                unsigned weight = span[i - x];
                if(weight == 32u) {
                    *dest = color;
                }
                else {
                    uint32_t bg = LV_COLOR_MIX_16_PREPARE(*dest);
                    uint32_t mixed = ((((fg - bg) * weight) >> 5) + bg) & 0x07e0f81fu;
                    *dest = (uint16_t)(mixed | (mixed >> 16));
                }
            }
            span += count;
        }
    }
}

bool radar_grid_draw(radar_grid_cache_t **cache, lv_layer_t *layer, const lv_area_t *area, lv_point_t center,
                     int32_t outer, int32_t label_width, lv_color_t color, lv_opa_t opacity,
                     radar_grid_capture_t capture)
{
    int32_t width = lv_area_get_width(area);
    if(width <= 0 || width > 1024 || width != lv_area_get_height(area) || !layer_ready(layer)) {
        return false;
    }
    lv_area_t clip;
    if(!lv_area_intersect(&clip, area, &layer->_clip_area) ||
       !lv_area_intersect(&clip, &clip, &layer->buf_area)) {
        return true;
    }
    if(*cache && ((*cache)->width != width || (*cache)->label_width != label_width || (*cache)->opacity != opacity)) {
        radar_grid_release(cache);
    }
    if(!*cache) {
        lv_draw_buf_t *mask = capture(layer, area, center, outer, label_width, opacity);
        if(!mask) {
            return false;
        }
        *cache = pack(mask, label_width, opacity);
        lv_draw_buf_destroy(mask);
        if(!*cache) {
            return false;
        }
    }
    replay(*cache, layer, area, &clip, lv_color_to_u16(color));
    return true;
}
