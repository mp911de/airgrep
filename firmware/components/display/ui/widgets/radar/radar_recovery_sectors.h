/* Private to radar.c. All coordinates are relative to the radar centre. */
#include <stdlib.h>
#ifdef ESP_PLATFORM
    #include "esp_heap_caps.h"
#endif

typedef struct {
    int16_t x, y;
    uint8_t alpha, degree;
} recovery_pixel_t;

typedef struct {
    recovery_pixel_t * pixels;
    size_t count, capacity;
} recovery_stamp_t;

enum {
    RECOVERY_OCTANTS = 8, RECOVERY_GRID = 8, RECOVERY_ANGLES = 46,
    DASH_PERIOD = RECOVERY_DASH + RECOVERY_GAP, DASH_PHASES = DASH_PERIOD / 2,
    SUBPIXELS = 16, PERIOD_UNITS = DASH_PERIOD * SUBPIXELS,
    STROKE_REACH = (RADAR_STROKE_WIDTH_PX + 1) * SUBPIXELS / 2,
    SPOKE_HALF_ROW = RADAR_STROKE_WIDTH_PX, SPOKE_ROW_WIDTH = 2 * SPOKE_HALF_ROW + 1
};

/* Within one octant the stroke fits in a narrow strip on each scanline. */
typedef struct {
    int16_t x;
    uint8_t alpha[DASH_PHASES][SPOKE_ROW_WIDTH];
} recovery_row_t;

typedef struct {
    recovery_row_t * rows;
    int32_t top, bottom;
} recovery_spoke_t;

struct recovery_sectors {
    int32_t radius, extent, cell_shift;
    recovery_stamp_t rings[RECOVERY_OCTANTS];
    recovery_spoke_t spokes[RECOVERY_ANGLES];
    uint8_t dash_alpha[PERIOD_UNITS][STROKE_REACH + 1];
    unsigned next_angle;
    lv_draw_buf_t * mask;
    lv_area_t occupied[RECOVERY_GRID * RECOVERY_GRID];
    uint8_t visible[360];
    bool prepared;
};

typedef struct {
    struct recovery_sectors * cache;
    recovery_spoke_t * spoke;
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
    for(size_t i = 0; i < RECOVERY_ANGLES; i++) {
        free(cache->spokes[i].rows);
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
    if(pen->spoke) {
        if(y < pen->spoke->top || y > pen->spoke->bottom) {
            return;
        }
        recovery_row_t * row = &pen->spoke->rows[y - pen->spoke->top];
        int32_t start = LV_MAX(0, x - row->x);
        int32_t end = LV_MIN(SPOKE_ROW_WIDTH, x - row->x + length);
        if(start < end) {
            lv_memset(row->alpha[0] + start, alpha, (size_t)(end - start));
        }
        return;
    }
    for(int32_t end = x + length; x < end && !pen->failed; x++) {
        float bearing = atan2f((float)x + 0.5f, -(float)y - 0.5f) / radians_per_degree;
        int32_t degree = radar_bearing_deg((int32_t)floorf(bearing));
        recovery_stamp_t * stamp = &pen->cache->rings[degree / 45];
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

static struct recovery_sectors * recovery_sectors_get(struct recovery_sectors ** owner, int32_t radius)
{
    if(*owner && (*owner)->radius != radius) {
        recovery_sectors_release(owner);
    }
    if(*owner) {
        return *owner;
    }
    struct recovery_sectors * cache = recovery_sectors_allocate(radius);
    if(!cache) {
        return NULL;
    }
    recovery_pen_t pen = {.cache = cache};
    for(int ring = 0; !pen.failed && ring < 2; ring++) {
        float r = ring ? (float)radius : (float)WEDGE_INNER_RADIUS_PX;
        float length = 2 * (float)M_PI * r;
        for(int i = 0; i < (int)ceilf(length / (RECOVERY_DASH + RECOVERY_GAP)); i++) {
            float along = (float)i * (RECOVERY_DASH + RECOVERY_GAP);
            float from = along / r, to = fminf(along + RECOVERY_DASH, length) / r;
            recovery_bake_dash(&pen, (radar_point_t){sinf(from) * r, -cosf(from) * r},
                               (radar_point_t){sinf(to) * r, -cosf(to) * r});
        }
    }
    if(pen.failed) {
        recovery_sectors_release(&cache);
        return NULL;
    }
    /* Distance to the nearest rounded dash, including the next dash across the gap. */
    for(int along = 0; along < PERIOD_UNITS; along++) {
        int cap = RADAR_STROKE_WIDTH_PX * SUBPIXELS / 2;
        int end = RECOVERY_DASH * SUBPIXELS - cap;
        int beyond = LV_MAX(0, LV_MAX(cap - along, LV_MIN(along - end, PERIOD_UNITS + cap - along)));
        for(int across = 0; across <= STROKE_REACH; across++) {
            float distance = sqrtf((float)(beyond * beyond + across * across));
            float opacity = LV_CLAMP(0.0f, ((float)STROKE_REACH - distance) / SUBPIXELS, 1.0f);
            cache->dash_alpha[along][across] = (uint8_t)lrintf(255.0f * opacity);
        }
    }
    *owner = cache;
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

static bool recovery_prepare_spoke(struct recovery_sectors * cache, unsigned angle)
{
    recovery_spoke_t * spoke = &cache->spokes[angle];
    if(spoke->rows) {
        return true;
    }
    float sine = sinf((float)angle * radians_per_degree), cosine = cosf((float)angle * radians_per_degree);
    spoke->top = (int32_t)floorf(-(float)cache->radius * cosine) - SPOKE_HALF_ROW;
    spoke->bottom = (int32_t)ceilf(-WEDGE_INNER_RADIUS_PX * cosine) + SPOKE_HALF_ROW;
    int32_t rows = spoke->bottom - spoke->top + 1;
#ifdef ESP_PLATFORM
    spoke->rows = heap_caps_calloc((size_t)rows, sizeof(*spoke->rows), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    spoke->rows = calloc((size_t)rows, sizeof(*spoke->rows));
#endif
    if(!spoke->rows) {
        return false;
    }
    for(int32_t y = spoke->top; y <= spoke->bottom; y++) {
        recovery_row_t * row = &spoke->rows[y - spoke->top];
        float cy = (float)y + 0.5f;
        row->x = (int16_t)((int32_t)floorf(-cy * sine / cosine - 0.5f) - SPOKE_HALF_ROW);
        for(int col = 0; col < SPOKE_ROW_WIDTH; col++) {
            float cx = (float)(row->x + col) + 0.5f;
            float along = cx * sine - cy * cosine - WEDGE_INNER_RADIUS_PX;
            float across = fabsf(cx * cosine + cy * sine);
            if(along < 0 || along > (float)(cache->radius - WEDGE_INNER_RADIUS_PX) || across >= (float)STROKE_REACH / SUBPIXELS) {
                continue;
            }
            int u = (int)lrintf(along * SUBPIXELS) % PERIOD_UNITS;
            int v = (int)lrintf(across * SUBPIXELS);
            for(int phase = 1; phase < DASH_PHASES; phase++) {
                u = (u + PERIOD_UNITS - 2 * SUBPIXELS) % PERIOD_UNITS;
                row->alpha[phase][col] = cache->dash_alpha[u][v];
            }
        }
    }
    recovery_pen_t pen = {.spoke = spoke};
    for(int32_t from = WEDGE_INNER_RADIUS_PX; from < cache->radius; from += DASH_PERIOD) {
        float to = (float)LV_MIN(cache->radius, from + RECOVERY_DASH);
        recovery_bake_dash(&pen, (radar_point_t){sine * (float)from, -cosine * (float)from},
                           (radar_point_t){sine * to, -cosine * to});
    }
    return true;
}

static bool recovery_stamp_spoke(struct recovery_sectors * cache, int32_t deg, int phase)
{
    int octant = deg / 45;
    unsigned angle = (unsigned)(octant % 2 ? 45 - deg % 45 : deg % 45);
    if(!recovery_prepare_spoke(cache, angle)) {
        return false;
    }
    const recovery_spoke_t * spoke = &cache->spokes[angle];
    bool swap = octant == 1 || octant == 2 || octant == 5 || octant == 6;
    bool flip_x = octant == 1 || octant == 2 || octant == 4 || octant == 7;
    bool flip_y = octant == 1 || octant == 3 || octant == 4 || octant == 6;
    for(int32_t y = spoke->top; y <= spoke->bottom; y++) {
        const recovery_row_t * row = &spoke->rows[y - spoke->top];
        for(int col = 0; col < SPOKE_ROW_WIDTH; col++) {
            uint8_t alpha = row->alpha[phase][col];
            if(alpha) {
                int32_t x = row->x + col, u = swap ? y : x, v = swap ? x : y;
                /* Pixel centres lie half a pixel beyond integer coordinates. */
                recovery_plot(cache, flip_x ? -u - 1 : u, flip_y ? -v - 1 : v, alpha);
            }
        }
    }
    return true;
}

static bool recovery_compose(struct recovery_sectors * cache, const radar_track_bands_t * bands, const uint8_t visible[360])
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
    for(uint32_t i = 0; i < bands->wedge_count; i++) {
        const radar_wedge_t * wedge = &bands->wedges[i];
        if(wedge->level != RADAR_BAND_RECOVERY || wedge->sweep_deg == 360) {
            continue;
        }
        float perimeter = (float)(2 * (cache->radius - WEDGE_INNER_RADIUS_PX) - RECOVERY_DASH) +
                          (float)cache->radius * (float)wedge->sweep_deg * radians_per_degree;
        int units = (int)lroundf(perimeter * SUBPIXELS);
        int phase = ((units + PERIOD_UNITS + SUBPIXELS) / (2 * SUBPIXELS)) % DASH_PHASES;
        if(!recovery_stamp_spoke(cache, wedge->start_deg, 0) ||
           !recovery_stamp_spoke(cache, radar_bearing_deg(wedge->start_deg + wedge->sweep_deg), phase)) {
            return false;
        }
    }
    lv_draw_buf_flush_cache(cache->mask, NULL);
    lv_memcpy(cache->visible, visible, sizeof(cache->visible));
    cache->prepared = true;
    return true;
}

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
        lv_memset(visible + wedge->start_deg, 1, (size_t)first);
        lv_memset(visible, 1, (size_t)(wedge->sweep_deg - first));
    }
    if(!any) {
        return true;
    }
    struct recovery_sectors * cache = recovery_sectors_get(owner, outer);
    if(!cache) {
        return false;
    }
    bool changed = !cache->prepared || lv_memcmp(visible, cache->visible, sizeof(visible));
    if(changed && !recovery_compose(cache, bands, visible)) {
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
