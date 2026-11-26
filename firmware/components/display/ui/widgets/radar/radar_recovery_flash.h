#if defined(__has_embed) && !defined(AIRGREP_EXPORT_RECOVERY)
#define RECOVERY_FLASH_AVAILABLE 1
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#endif
static const uint8_t recovery_atlas[] = {
#embed "radar_recovery_atlas.bin"
};
#ifdef __clang__
#pragma clang diagnostic pop
#endif

typedef struct {
    int16_t x, y;
    uint32_t offset;
} recovery_directory_t;

enum { RECOVERY_DIRECTORY_SIZE = 1089 * sizeof(recovery_directory_t) };
_Static_assert(sizeof(recovery_directory_t) == 8 && RECOVERY_DASH == 8 && RECOVERY_GAP == 6 &&
               WEDGE_INNER_RADIUS_PX == 32 && RADAR_STROKE_WIDTH_PX == 3, "Regenerate the recovery atlas");

typedef struct {
    int32_t col, row, origin;
} recovery_basis_t;

static recovery_basis_t recovery_basis(int32_t octant, int32_t stride)
{
    const recovery_basis_t rotations[] = {{1, stride, 0}, {-stride, -1, -stride - 1}, {stride, -1, -1},
        {1, -stride, -stride}, {-1, -stride, -stride - 1}, {stride, 1, 0}, {-stride, 1, -stride}, {-1, stride, -1}};
    return rotations[octant];
}

static void recovery_flash_stamp(struct recovery_sectors * cache, int32_t index, int32_t octant)
{
    recovery_directory_t entries[2];
    memcpy(entries, recovery_atlas + (size_t)index * sizeof(entries[0]), sizeof(entries));
    const uint8_t * data = recovery_atlas + RECOVERY_DIRECTORY_SIZE;
    const uint8_t * p = data + entries[0].offset;
    const uint8_t * end = data + entries[1].offset;
    int32_t stride = (int32_t)cache->mask->header.stride;
    recovery_basis_t basis = recovery_basis(octant, stride);
    int32_t origin = cache->extent * (stride + 1) + entries[0].x * basis.col + entries[0].y * basis.row + basis.origin;
    while(p < end) {
        int32_t offset = origin + p[0] * basis.col + p[1] * basis.row;
        uint8_t * dest = cache->mask->data + offset;
        int32_t count = p[2];
        p += 3;
        for(int32_t i = 0; i < count; i++) {
            uint8_t alpha = *p++;
            if(*dest < alpha) {
                *dest = alpha;
            }
            dest += basis.col;
        }
    }
}

static void recovery_flash_bounds(struct recovery_sectors * cache, float x1, float y1, float x2, float y2)
{
    int32_t left = LV_MAX(0, cache->extent + (int32_t)floorf(fminf(x1, x2)) - 2);
    int32_t top = LV_MAX(0, cache->extent + (int32_t)floorf(fminf(y1, y2)) - 2);
    int32_t right = LV_MIN(2 * cache->extent, cache->extent + (int32_t)ceilf(fmaxf(x1, x2)) + 2);
    int32_t bottom = LV_MIN(2 * cache->extent, cache->extent + (int32_t)ceilf(fmaxf(y1, y2)) + 2);
    int32_t cell = 1 << cache->cell_shift;
    for(int32_t y = top / cell; y <= bottom / cell; y++) {
        for(int32_t x = left / cell; x <= right / cell; x++) {
            lv_area_t * rect = &cache->occupied[y * RECOVERY_GRID + x];
            rect->x1 = LV_MIN(rect->x1, LV_MAX(left, x * cell));
            rect->x2 = LV_MAX(rect->x2, LV_MIN(right, (x + 1) * cell - 1));
            rect->y1 = LV_MIN(rect->y1, LV_MAX(top, y * cell));
            rect->y2 = LV_MAX(rect->y2, LV_MIN(bottom, (y + 1) * cell - 1));
        }
    }
}

static void recovery_flash_spoke(struct recovery_sectors * cache, int32_t deg, int32_t phase, bool dense)
{
    int32_t octant = deg / 45;
    int32_t canonical = octant % 2 ? 45 - deg % 45 : deg % 45;
    int32_t base = 720;
    if(phase >= 0) {
        /* Seven evenly spaced phases place the edge of a dash within one pixel of its requested phase. */
        int32_t bank = ((phase + 16) / 32) % 7;
        base = bank ? 812 + (bank - 1) * 46 : 720;
    }
    recovery_flash_stamp(cache, base + canonical, octant);
    if(!dense) {
        float x = sinf((float)deg * radians_per_degree), y = -cosf((float)deg * radians_per_degree);
        recovery_flash_bounds(cache, x * WEDGE_INNER_RADIUS_PX, y * WEDGE_INNER_RADIUS_PX,
                              x * (float)cache->radius, y * (float)cache->radius);
    }
}

static bool recovery_flash_compose(struct recovery_sectors * cache, const radar_track_bands_t * bands,
                                   const uint8_t visible[360])
{
    for(int32_t i = 0; i < RECOVERY_GRID * RECOVERY_GRID; i++) {
        cache->occupied[i] = (lv_area_t){INT32_MAX, INT32_MAX, -1, -1};
    }
    uint32_t wedges = 0;
    for(uint32_t i = 0; i < bands->wedge_count; i++) {
        wedges += bands->wedges[i].level == RADAR_BAND_RECOVERY;
    }
    bool dense = wedges > 4;
    lv_draw_buf_clear(cache->mask, NULL);
    if(dense) {
        cache->occupied[0] = (lv_area_t){0, 0, 2 * cache->extent, 2 * cache->extent};
    }
    for(int32_t start = 0; start < 360; start += 45) {
        int32_t first = start + 45, last = start;
        for(int32_t deg = start; deg < start + 45; deg++) {
            if(visible[deg]) {
                first = LV_MIN(first, deg);
                last = deg + 1;
                recovery_flash_stamp(cache, deg, 0);
                recovery_flash_stamp(cache, 360 + deg, 0);
            }
        }
        if(first < last && !dense) {
            float x1 = sinf((float)first * radians_per_degree), y1 = -cosf((float)first * radians_per_degree);
            float x2 = sinf((float)last * radians_per_degree), y2 = -cosf((float)last * radians_per_degree);
            recovery_flash_bounds(cache, x1 * WEDGE_INNER_RADIUS_PX, y1 * WEDGE_INNER_RADIUS_PX,
                                  x2 * WEDGE_INNER_RADIUS_PX, y2 * WEDGE_INNER_RADIUS_PX);
            recovery_flash_bounds(cache, x1 * (float)cache->radius, y1 * (float)cache->radius,
                                  x2 * (float)cache->radius, y2 * (float)cache->radius);
        }
    }
    for(uint32_t i = 0; i < bands->wedge_count; i++) {
        const radar_wedge_t * w = &bands->wedges[i];
        if(w->level != RADAR_BAND_RECOVERY || w->sweep_deg == 360) {
            continue;
        }
        recovery_flash_spoke(cache, w->start_deg, -1, dense);
        float perimeter = (float)(2 * (cache->radius - WEDGE_INNER_RADIUS_PX) - RECOVERY_DASH) +
                          (float)cache->radius * (float)w->sweep_deg * radians_per_degree;
        int32_t phase = (int32_t)lroundf(perimeter * 16) % 224;
        recovery_flash_spoke(cache, radar_bearing_deg(w->start_deg + w->sweep_deg), phase, dense);
    }
    lv_draw_buf_flush_cache(cache->mask, NULL);
    memcpy(cache->visible, visible, sizeof(cache->visible));
    cache->prepared = true;
    return true;
}
#else
#define RECOVERY_FLASH_AVAILABLE 0
static bool recovery_flash_compose(struct recovery_sectors * cache, const radar_track_bands_t * bands,
                                   const uint8_t visible[360])
{
    (void)bands;
    return recovery_compose(cache, visible);
}
#endif
