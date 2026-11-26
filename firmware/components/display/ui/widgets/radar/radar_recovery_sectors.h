/* Private to radar.c. All coordinates are relative to the radar centre. */
typedef struct {
    int16_t x, y;
    uint8_t alpha, degree;
} recovery_pixel_t;

typedef struct {
    recovery_pixel_t * pixels;
    size_t count, capacity;
} recovery_stamp_t;

enum { RECOVERY_OCTANTS = 8, RECOVERY_GRID = 8 };

struct recovery_sectors {
    int32_t radius, extent, cell_shift;
    recovery_stamp_t rings[RECOVERY_OCTANTS];
    recovery_stamp_t spokes[46];
    lv_draw_buf_t * mask;
    lv_area_t occupied[RECOVERY_GRID * RECOVERY_GRID];
    uint8_t visible[360];
    bool prepared;
};

typedef struct {
    struct recovery_sectors * cache;
    recovery_stamp_t * spoke;
    int32_t x, y;
    bool failed;
} recovery_pen_t;

static void recovery_sectors_release(struct recovery_sectors ** owner)
{
    struct recovery_sectors * cache = *owner;
    if(!cache) {
        return;
    }
    for(size_t i = 0; i < RECOVERY_OCTANTS; i++) {
        lv_free(cache->rings[i].pixels);
    }
    for(size_t i = 0; i < 46; i++) {
        lv_free(cache->spokes[i].pixels);
    }
    if(cache->mask) {
        lv_image_cache_drop(cache->mask);
        lv_draw_buf_destroy(cache->mask);
    }
    lv_free(cache);
    *owner = NULL;
}

static void recovery_store_pixels(void * user, int32_t x, int32_t y, int32_t length, uint8_t alpha)
{
    recovery_pen_t * pen = user;
    x += pen->x;
    y += pen->y;
    for(int32_t end = x + length; x < end && !pen->failed; x++) {
        int32_t degree = 0;
        recovery_stamp_t * stamp = pen->spoke;
        if(!stamp) {
            float bearing = atan2f((float)x + 0.5f, -(float)y - 0.5f) / radians_per_degree;
            degree = radar_bearing_deg((int32_t)floorf(bearing));
            stamp = &pen->cache->rings[degree / 45];
        }
        if(stamp->count == stamp->capacity) {
            size_t capacity = stamp->capacity ? stamp->capacity * 2 : 128;
            recovery_pixel_t * pixels = lv_realloc(stamp->pixels, capacity * sizeof(*pixels));
            if(!pixels) {
                pen->failed = true;
                return;
            }
            stamp->pixels = pixels;
            stamp->capacity = capacity;
        }
        stamp->pixels[stamp->count++] = (recovery_pixel_t){(int16_t)x, (int16_t)y, alpha, (uint8_t)(degree % 45)};
    }
}

static void recovery_bake_dash(recovery_pen_t * pen, radar_point_t from, radar_point_t to)
{
    float dx = to.x - from.x, dy = to.y - from.y;
    float length = hypotf(dx, dy);
    if(length <= RADAR_STROKE_WIDTH_PX) {
        return;
    }
    float cut = RADAR_STROKE_WIDTH_PX / 2.0f / length;
    pen->x = (int32_t)roundf(from.x);
    pen->y = (int32_t)roundf(from.y);
    radar_tail_part_t part = {{from.x + dx * cut - (float)pen->x, from.y + dy * cut - (float)pen->y},
                              {to.x - dx * cut - (float)pen->x, to.y - dy * cut - (float)pen->y},
                              0};
    radar_track_tail_runs(&part, 1, RADAR_STROKE_WIDTH_PX, recovery_store_pixels, pen);
}

static struct recovery_sectors * recovery_sectors_allocate(int32_t radius)
{
    if(radius <= WEDGE_INNER_RADIUS_PX || radius > INT16_MAX - 4) {
        return NULL;
    }
    struct recovery_sectors * cache = lv_malloc_zeroed(sizeof(*cache));
    if(!cache) {
        return NULL;
    }
    cache->radius = radius;
    cache->extent = radius + 2;
    int32_t size = cache->extent * 2 + 1;
    while((size - 1) >> cache->cell_shift >= RECOVERY_GRID) {
        cache->cell_shift++;
    }
    cache->mask = lv_draw_buf_create(size, size, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
    if(!cache->mask) {
        recovery_sectors_release(&cache);
    }
    return cache;
}

static struct recovery_sectors * recovery_sectors_create(int32_t radius)
{
    struct recovery_sectors * cache = recovery_sectors_allocate(radius);
    if(!cache) {
        return NULL;
    }
    recovery_pen_t pen = {.cache = cache};
    for(int ring = 0; cache->mask && !pen.failed && ring < 2; ring++) {
        float r = ring ? (float)radius : (float)WEDGE_INNER_RADIUS_PX;
        float length = 2 * (float)M_PI * r;
        for(int i = 0; i < (int)ceilf(length / (RECOVERY_DASH + RECOVERY_GAP)); i++) {
            float along = (float)i * (RECOVERY_DASH + RECOVERY_GAP);
            float from = along / r, to = fminf(along + RECOVERY_DASH, length) / r;
            recovery_bake_dash(&pen, (radar_point_t){sinf(from) * r, -cosf(from) * r},
                               (radar_point_t){sinf(to) * r, -cosf(to) * r});
        }
    }
    if(!cache->mask || pen.failed) {
        recovery_sectors_release(&cache);
    }
    return cache;
}

static void recovery_plot(struct recovery_sectors * cache, int32_t x, int32_t y, uint8_t alpha)
{
    x += cache->extent;
    y += cache->extent;
    uint8_t * dest = cache->mask->data + (size_t)y * cache->mask->header.stride + x;
    if(*dest < alpha) {
        *dest = alpha;
    }
    lv_area_t * rect = &cache->occupied[(y >> cache->cell_shift) * RECOVERY_GRID + (x >> cache->cell_shift)];
    rect->x1 = LV_MIN(rect->x1, x);
    rect->x2 = LV_MAX(rect->x2, x);
    rect->y1 = LV_MIN(rect->y1, y);
    rect->y2 = LV_MAX(rect->y2, y);
}

static bool recovery_stamp_spoke(struct recovery_sectors * cache, int32_t deg)
{
    int32_t octant = deg / 45;
    int32_t canonical = octant % 2 ? 45 - deg % 45 : deg % 45;
    recovery_stamp_t * stamp = &cache->spokes[canonical];
    if(!stamp->count) {
        float dx = sinf((float)canonical * radians_per_degree), dy = -cosf((float)canonical * radians_per_degree);
        recovery_pen_t pen = {.spoke = stamp};
        for(int32_t from = WEDGE_INNER_RADIUS_PX; from < cache->radius; from += RECOVERY_DASH + RECOVERY_GAP) {
            float to = (float)LV_MIN(cache->radius, from + RECOVERY_DASH);
            recovery_bake_dash(&pen, (radar_point_t){dx * (float)from, dy * (float)from}, (radar_point_t){dx * to, dy * to});
        }
        if(pen.failed) {
            return false;
        }
    }
    for(size_t i = 0; i < stamp->count; i++) {
        const recovery_pixel_t * p = &stamp->pixels[i];
        int32_t x = p->x, y = p->y;
        /* Reflect pixel centres, which sit half a pixel beyond integer coordinates. */
        switch(octant) {
        case 0:
            recovery_plot(cache, x, y, p->alpha);
            break;
        case 1:
            recovery_plot(cache, -y - 1, -x - 1, p->alpha);
            break;
        case 2:
            recovery_plot(cache, -y - 1, x, p->alpha);
            break;
        case 3:
            recovery_plot(cache, x, -y - 1, p->alpha);
            break;
        case 4:
            recovery_plot(cache, -x - 1, -y - 1, p->alpha);
            break;
        case 5:
            recovery_plot(cache, y, x, p->alpha);
            break;
        case 6:
            recovery_plot(cache, y, -x - 1, p->alpha);
            break;
        default:
            recovery_plot(cache, -x - 1, y, p->alpha);
            break;
        }
    }
    return true;
}

static bool recovery_compose(struct recovery_sectors * cache, const uint8_t visible[360])
{
    lv_draw_buf_clear(cache->mask, NULL);
    for(int32_t i = 0; i < RECOVERY_GRID * RECOVERY_GRID; i++) {
        cache->occupied[i] = (lv_area_t){INT32_MAX, INT32_MAX, -1, -1};
    }
    for(size_t octant = 0; octant < RECOVERY_OCTANTS; octant++) {
        const uint8_t * active = visible + octant * 45;
        if(!memchr(active, 1, 45)) {
            continue;
        }
        bool whole = !memchr(active, 0, 45);
        const recovery_stamp_t * stamp = &cache->rings[octant];
        for(size_t i = 0; i < stamp->count; i++) {
            const recovery_pixel_t * p = &stamp->pixels[i];
            if(whole || active[p->degree]) {
                recovery_plot(cache, p->x, p->y, p->alpha);
            }
        }
    }
    for(int32_t deg = 0; deg < 360; deg++) {
        if(visible[deg] != visible[(deg + 359) % 360] && !recovery_stamp_spoke(cache, deg)) {
            return false;
        }
    }
    lv_draw_buf_flush_cache(cache->mask, NULL);
    memcpy(cache->visible, visible, sizeof(cache->visible));
    cache->prepared = true;
    return true;
}

#include "radar_recovery_flash.h"

static bool recovery_sectors_draw(struct recovery_sectors ** owner, lv_layer_t * layer,
                                  const radar_track_bands_t * bands, lv_point_t center, int32_t outer, lv_color_t color)
{
    uint8_t visible[360] = {0};
    bool any = false;
    for(size_t i = 0; i < bands->wedge_count; i++) {
        const radar_wedge_t * wedge = &bands->wedges[i];
        if(wedge->level != RADAR_BAND_RECOVERY) {
            continue;
        }
        any = true;
        int32_t first = LV_MIN(wedge->sweep_deg, 360 - wedge->start_deg);
        memset(visible + wedge->start_deg, 1, (size_t)first);
        memset(visible, 1, (size_t)(wedge->sweep_deg - first));
    }
    if(!any) {
        return true;
    }
    bool flash = RECOVERY_FLASH_AVAILABLE && outer == 176;
    if(*owner && (*owner)->radius != outer) {
        recovery_sectors_release(owner);
    }
    if(!*owner) {
        *owner = flash ? recovery_sectors_allocate(outer) : recovery_sectors_create(outer);
    }
    struct recovery_sectors * cache = *owner;
    if(!cache) {
        return false;
    }
    bool changed = !cache->prepared || memcmp(visible, cache->visible, sizeof(visible));
    if(changed && !(flash ? recovery_flash_compose(cache, bands, visible) : recovery_compose(cache, visible))) {
        recovery_sectors_release(owner);
        return false;
    }
    lv_draw_image_dsc_t image;
    lv_draw_image_dsc_init(&image);
    image.src = cache->mask;
    image.recolor = color;
    image.recolor_opa = LV_OPA_COVER;
    lv_area_t area = {center.x - cache->extent, center.y - cache->extent, center.x + cache->extent,
                      center.y + cache->extent};
    lv_area_t original_clip = layer->_clip_area;
    for(int32_t i = 0; i < RECOVERY_GRID * RECOVERY_GRID; i++) {
        lv_area_t rect = cache->occupied[i];
        if(rect.x2 < rect.x1) {
            continue;
        }
        lv_area_move(&rect, area.x1, area.y1);
        if(lv_area_intersect(&layer->_clip_area, &original_clip, &rect)) {
            lv_draw_image(layer, &image, &area);
        }
    }
    layer->_clip_area = original_clip;
    return true;
}
