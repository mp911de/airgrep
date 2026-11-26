#include "radar_outline.h"
#include "airgrep_ui.h"
#include <stdlib.h>

#ifdef ESP_PLATFORM
    #include "esp_heap_caps.h"
#endif
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl_private.h"
#else
    #include "lvgl_private/lvgl_private.h"
#endif

enum { DIRECTIONS = 36, STEP_TENTHS = 100, GROUPS = 18 };

typedef struct {
    lv_image_dsc_t image;
    lv_point_t origin;
} atlas_mask_t;

typedef struct {
    const lv_image_dsc_t * source;
    int32_t scale;
    atlas_mask_t direction[DIRECTIONS];
} symbol_group_t;

static symbol_group_t * groups;
static lv_display_t * owner;
static uint32_t mask_count;
static uint32_t pixel_bytes;
static uint32_t cache_hits, cache_misses;
static void * allocate(size_t size)
{
    const size_t alignment = LV_MAX(LV_DRAW_BUF_ALIGN, sizeof(void *));
#ifdef ESP_PLATFORM
    return heap_caps_aligned_alloc(alignment, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return aligned_alloc(alignment, LV_ROUND_UP(size, alignment));
#endif
}

void radar_symbol_atlas_usage(uint32_t * masks, uint32_t * bytes)
{
    *masks = mask_count;
    *bytes = pixel_bytes;
}

void radar_symbol_atlas_lookups(uint32_t * hits, uint32_t * misses)
{
    *hits = cache_hits;
    *misses = cache_misses;
}

static void drop_mask(atlas_mask_t * entry)
{
    lv_image_cache_drop(&entry->image);
    free((void *)entry->image.data);
    entry->image.data = NULL;
    pixel_bytes -= entry->image.data_size;
    mask_count--;
}

/* Between completed refreshes, no queued image task still holds these descriptors. */
void radar_symbol_atlas_release(void)
{
    if(!groups) {
        return;
    }
    for(size_t g = 0; g < GROUPS; g++) {
        for(size_t d = 0; d < DIRECTIONS; d++) {
            atlas_mask_t * entry = &groups[g].direction[d];
            if(entry->image.data) {
                drop_mask(entry);
            }
        }
    }
    free(groups);
    groups = NULL;
    cache_hits = 0;
    cache_misses = 0;
}

static void display_event(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_DELETE) {
        radar_symbol_atlas_release();
        owner = NULL;
    }
}

static bool attach(lv_display_t * display)
{
    if(!display || (owner && owner != display)) {
        return false;
    }
    if(!owner) {
        if(!lv_display_add_event_cb(display, display_event, LV_EVENT_DELETE, NULL)) {
            return false;
        }
        owner = display;
    }
    if(!groups) {
        size_t bytes = GROUPS * sizeof(*groups);
        groups = allocate(bytes);
        if(!groups) {
            return false;
        }
        lv_memset(groups, 0, bytes);
    }
    return true;
}

static bool rasterize(atlas_mask_t * entry, const lv_draw_image_dsc_t * transform)
{
    const lv_image_dsc_t * src = transform->src;
    lv_area_t bounds;
    lv_image_buf_get_transformed_area(&bounds, (int32_t)src->header.w, (int32_t)src->header.h,
                                      transform->rotation, transform->scale_x, transform->scale_y, &transform->pivot);
    int32_t w = lv_area_get_width(&bounds), h = lv_area_get_height(&bounds);
    if(w <= 0 || h <= 0 || (size_t)w > SIZE_MAX / (size_t)h) {
        return false;
    }
    uint8_t * scratch = allocate((size_t)w * (size_t)h);
    if(!scratch) {
        return false;
    }
    lv_draw_sw_transform(&bounds, src->data, (int32_t)src->header.w, (int32_t)src->header.h, (int32_t)src->header.stride,
                         transform, NULL, LV_COLOR_FORMAT_A8, scratch);
    lv_area_t ink = {w, h, -1, -1};
    for(int32_t y = 0; y < h; y++) {
        const uint8_t * row = scratch + (size_t)y * (size_t)w;
        int32_t left = 0;
        while(left < w && !row[left]) {
            left++;
        }
        if(left == w) {
            continue;
        }
        int32_t right = w - 1;
        while(right > left && !row[right]) {
            right--;
        }
        ink.x1 = LV_MIN(ink.x1, left);
        ink.x2 = LV_MAX(ink.x2, right);
        ink.y1 = LV_MIN(ink.y1, y);
        ink.y2 = y;
    }
    if(ink.y2 < 0) {
        free(scratch);
        return false;
    }
    uint32_t width = (uint32_t)lv_area_get_width(&ink), height = (uint32_t)lv_area_get_height(&ink);
    uint32_t stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_A8);
    uint32_t size = stride * height;
    uint8_t * pixels = allocate(size);
    if(!pixels) {
        free(scratch);
        return false;
    }
    for(uint32_t y = 0; y < height; y++) {
        lv_memcpy(pixels + (size_t)y * stride, scratch + ((size_t)y + (size_t)ink.y1) * (size_t)w + (size_t)ink.x1, width);
    }
    free(scratch);
    entry->origin = (lv_point_t){bounds.x1 + ink.x1, bounds.y1 + ink.y1};
    entry->image = (lv_image_dsc_t){.header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_A8,
                                               .w = width, .h = height, .stride = stride},
                                   .data_size = size, .data = pixels};
    mask_count++;
    pixel_bytes += size;
    return true;
}

static atlas_mask_t * get_mask(const lv_draw_image_dsc_t * transform)
{
    for(size_t i = 0; i < GROUPS; i++) {
        symbol_group_t * group = &groups[i];
        if(group->source && (group->source != transform->src || group->scale != transform->scale_x)) {
            continue;
        }
        group->source = transform->src;
        group->scale = transform->scale_x;
        atlas_mask_t * entry = &group->direction[transform->rotation / STEP_TENTHS];
        if(!entry->image.data) {
            cache_misses++;
            if(!rasterize(entry, transform)) {
                return NULL;
            }
        }
        else {
            cache_hits++;
        }
        return entry;
    }
    return NULL;
}

bool radar_symbol_atlas_draw(lv_layer_t * layer, lv_draw_image_dsc_t * image, const lv_area_t * area)
{
    const lv_image_dsc_t * src = image->src;
    if(!src || image->scale_y != image->scale_x ||
       image->pivot.x != (int32_t)(src->header.w / 2) || image->pivot.y != (int32_t)(src->header.h / 2) ||
       !attach(lv_refr_get_disp_refreshing())) {
        return false;
    }
    int32_t angle = (image->rotation % 3600 + 3600) % 3600;
    lv_draw_image_dsc_t lookup = *image;
    lookup.rotation = ((angle + STEP_TENTHS / 2) / STEP_TENTHS % DIRECTIONS) * STEP_TENTHS;
    atlas_mask_t * entry = get_mask(&lookup);
    if(!entry) {
        return false;
    }
    lv_area_t placed;
    lv_area_set(&placed, 0, 0, (int32_t)entry->image.header.w - 1, (int32_t)entry->image.header.h - 1);
    lv_area_move(&placed, area->x1 + entry->origin.x, area->y1 + entry->origin.y);
    image->src = &entry->image;
    image->rotation = 0;
    image->scale_x = LV_SCALE_NONE;
    image->scale_y = LV_SCALE_NONE;
    image->pivot = (lv_point_t){0, 0};
    lv_draw_image(layer, image, &placed);
    return true;
}

enum { DETAILED_SYMBOLS = 6 };

static const void * detailed_symbol(unsigned index)
{
    const void * symbols[DETAILED_SYMBOLS] = {
        symbol_light, symbol_airliner, symbol_glider, symbol_helicopter, symbol_balloon, symbol_unknown
    };
    return symbols[index];
}

static bool warm_symbol(const lv_image_dsc_t * symbol, int32_t scale, unsigned direction)
{
    if(!symbol) {
        return false;
    }
    const lv_draw_buf_t * outline = radar_symbol_outline_mask(symbol, scale);
    if(!outline) {
        return false;
    }
    const void * sources[] = {symbol, outline};
    for(size_t s = 0; s < sizeof(sources) / sizeof(sources[0]); s++) {
        const lv_image_dsc_t * src = sources[s];
        lv_draw_image_dsc_t transform;
        lv_draw_image_dsc_init(&transform);
        transform.src = src;
        transform.scale_x = scale;
        transform.scale_y = scale;
        transform.pivot = (lv_point_t){(int32_t)(src->header.w / 2), (int32_t)(src->header.h / 2)};
        transform.rotation = (int32_t)direction * STEP_TENTHS;
        if(!get_mask(&transform)) {
            return false;
        }
    }
    return true;
}

bool radar_symbol_atlas_warm_next(lv_display_t * display, bool simple, unsigned bucket)
{
    unsigned last = simple ? DIRECTIONS + 1 : DETAILED_SYMBOLS * DIRECTIONS;
    if(bucket > last || !attach(display)) {
        return false;
    }
    if(bucket == 0) {
        return warm_symbol(simple ? symbol_paper_plane : symbol_light, RADAR_OWN_SYMBOL_SCALE, 0);
    }
    const unsigned index = bucket - 1;
    const void * symbol = NULL;
    if(simple) {
        /* Targets without a track use the unknown symbol at zero degrees. */
        symbol = index == DIRECTIONS ? symbol_unknown : symbol_simple;
    }
    else {
        symbol = detailed_symbol(index / DIRECTIONS);
    }
    return warm_symbol(symbol, RADAR_SYMBOL_SCALE, index % DIRECTIONS) && bucket < last;
}
