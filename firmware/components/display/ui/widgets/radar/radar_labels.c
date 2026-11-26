#include "radar_labels.h"
#include "radar_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif

#define SIDES 8
#define DIAGONAL_LABEL_GAP_PX 14
#define LABEL_CLEARANCE_PX 3
#define RING_CLEARANCE_PX 6
/* A label over another label, a symbol, a dot or the edge cannot be read. One over a ring can, so two rings weigh less
 * than one of those. */
#define COVERS 8
#define CROSSES_RING 3

static bool overlap(const radar_box_t * a, const radar_box_t * b)
{
    return a->x1 <= b->x2 && b->x1 <= a->x2 && a->y1 <= b->y2 && b->y1 <= a->y2;
}

static bool inside(const radar_box_t * box, const radar_box_t * bounds)
{
    return box->x1 >= bounds->x1 && box->x2 <= bounds->x2 && box->y1 >= bounds->y1 && box->y2 <= bounds->y2;
}

static bool crosses(const radar_box_t * box, radar_spot_t center, int32_t radius)
{
    int32_t near_x = radar_clamp_i32(center.x, box->x1, box->x2) - center.x;
    int32_t near_y = radar_clamp_i32(center.y, box->y1, box->y2) - center.y;
    int32_t far_x = radar_max_i32(abs(box->x1 - center.x), abs(box->x2 - center.x));
    int32_t far_y = radar_max_i32(abs(box->y1 - center.y), abs(box->y2 - center.y));
    int32_t reach = radius + RING_CLEARANCE_PX;
    int32_t hole = radar_max_i32(radius - RING_CLEARANCE_PX, 0);
    return near_x * near_x + near_y * near_y <= reach * reach && far_x * far_x + far_y * far_y >= hole * hole;
}

static radar_box_t around(radar_spot_t spot, int32_t reach)
{
    radar_box_t box = {spot.x - reach, spot.y - reach, spot.x + reach, spot.y + reach};
    return box;
}

typedef struct {
    int8_t horizontal_direction;
    int8_t vertical_direction;
} side_t;

static int32_t edge(int32_t symbol, int32_t size, int8_t direction, int32_t gap)
{
    if(direction == 0) {
        return symbol - size / 2;
    }
    return direction > 0 ? symbol + gap : symbol - gap - size;
}

static radar_box_t beside(radar_spot_t symbol, radar_spot_t size, side_t side)
{
    int32_t gap = side.horizontal_direction && side.vertical_direction ? DIAGONAL_LABEL_GAP_PX : RADAR_LABEL_GAP_PX;
    radar_box_t box;
    box.x1 = edge(symbol.x, size.x, side.horizontal_direction, gap);
    box.y1 = edge(symbol.y, size.y, side.vertical_direction, gap);
    box.x2 = box.x1 + size.x - 1;
    box.y2 = box.y1 + size.y - 1;
    return box;
}

#ifdef CONFIG_IDF_TARGET_ESP32S3
/* Each block has eight left edges, then eight right, top and bottom edges.
 * The radar has 32 symbols, 32 placed labels, at most 160 dots, the own ship and two ring labels. */
enum { LANES = 8, RIGHT_EDGES = LANES, TOP_EDGES = 2 * LANES, BOTTOM_EDGES = 3 * LANES,
       BLOCK_WORDS = 4 * LANES, BLOCKS = 29 };

static void store_box(int16_t * blocks, size_t index, radar_box_t box)
{
    int16_t * lane = blocks + index / LANES * BLOCK_WORDS + index % LANES;
    lane[0] = (int16_t)box.x1;
    lane[RIGHT_EDGES] = (int16_t)box.x2;
    lane[TOP_EDGES] = (int16_t)box.y1;
    lane[BOTTOM_EDGES] = (int16_t)box.y2;
}

static const radar_box_t unused_box = {INT16_MAX, INT16_MAX, INT16_MIN, INT16_MIN};

static bool compact(int32_t value)
{
    /* A label origin, dimension, clearance and gap must fit together in a signed 16-bit lane. */
    return value >= -8192 && value <= 8192;
}

static uint32_t pack(int16_t * blocks, const radar_field_t * field, const radar_spot_t * symbols,
                     const radar_spot_t * sizes, uint32_t count)
{
    const unsigned capacity = BLOCKS * LANES;
    if(count > (capacity - 3) / 2 || field->dot_count > capacity - 3 - 2 * count ||
       !compact(field->center.x) || !compact(field->center.y)) {
        return 0;
    }
    for(uint32_t i = 0; i < count; ++i) {
        if(!compact(symbols[i].x) || !compact(symbols[i].y) || !compact(sizes[i].x) || !compact(sizes[i].y)) {
            return 0;
        }
        store_box(blocks, i, around(symbols[i], RADAR_SYMBOL_RADIUS_PX));
    }
    uint32_t used = count;
    store_box(blocks, used++, around(field->center, RADAR_SYMBOL_RADIUS_PX));
    for(unsigned i = 0; i < 2; ++i) {
        radar_box_t r = field->ring_labels[i];
        if(!compact(r.x1) || !compact(r.y1) || !compact(r.x2) || !compact(r.y2)) {
            return 0;
        }
        store_box(blocks, used++, r);
    }
    for(uint32_t i = 0; i < field->dot_count; ++i) {
        if(!compact(field->dots[i].x) || !compact(field->dots[i].y)) {
            return 0;
        }
        store_box(blocks, used++, around(field->dots[i], RADAR_DOT_RADIUS_PX));
    }
    uint32_t end = (used + count + LANES - 1) / LANES * LANES;
    for(uint32_t i = used; i < end; ++i) {
        store_box(blocks, i, unused_box);
    }
    return used;
}

extern unsigned radar_label_intersections_s3(const int16_t * blocks, const int16_t * query, unsigned count);

static unsigned packed_intersections(const int16_t * blocks, uint32_t used, const radar_box_t * padded)
{
    const int16_t query[] = {(int16_t)padded->x1, (int16_t)padded->x2, (int16_t)padded->y1, (int16_t)padded->y2};
    unsigned count = (used + LANES - 1) / LANES;
    return radar_label_intersections_s3(blocks, query, count);
}
#endif

static unsigned intersections(const radar_field_t * field, const radar_spot_t * symbols, uint32_t count,
                              const radar_box_t * labels, uint32_t index, const radar_box_t * padded)
{
    radar_box_t own = around(field->center, RADAR_SYMBOL_RADIUS_PX);
    unsigned hits = overlap(padded, &own);
    for(uint32_t i = 0; i < count; i++) {
        radar_box_t symbol = around(symbols[i], RADAR_SYMBOL_RADIUS_PX);
        hits += i != index && overlap(padded, &symbol);
        hits += i < index && overlap(padded, &labels[i]);
    }
    for(uint32_t i = 0; i < 2; i++) {
        hits += overlap(padded, &field->ring_labels[i]);
    }
    for(uint32_t i = 0; i < field->dot_count; i++) {
        radar_box_t dot = around(field->dots[i], RADAR_DOT_RADIUS_PX);
        hits += overlap(padded, &dot);
    }
    return hits;
}

static uint32_t cost(const radar_field_t * field, const radar_box_t * label, unsigned hits)
{
    uint32_t sum = (hits + !inside(label, &field->bounds)) * COVERS;
    if(crosses(label, field->center, field->inner)) {
        sum += CROSSES_RING;
    }
    if(crosses(label, field->center, field->outer)) {
        sum += CROSSES_RING;
    }
    return sum;
}

void radar_labels(const radar_field_t * field, const radar_spot_t * symbols, const radar_spot_t * sizes,
                  uint32_t count, radar_box_t * labels)
{
    static const side_t ORDER[SIDES] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

    if(!count) {
        return;
    }
#ifdef CONFIG_IDF_TARGET_ESP32S3
    int16_t storage[BLOCKS * BLOCK_WORDS + LANES - 1];
    uintptr_t address = (uintptr_t)storage;
    /* The vector load rounds down. Keep alignment relative to the actual stack address. */
    __asm__ volatile("" : "+r"(address));
    int16_t * blocks = (int16_t *)((address + 15u) & ~(uintptr_t)15u);
    uint32_t used = pack(blocks, field, symbols, sizes, count);
#endif
    for(uint32_t i = 0; i < count; i++) {
#ifdef CONFIG_IDF_TARGET_ESP32S3
        if(used) {
            store_box(blocks, i, unused_box);
        }
#endif
        int8_t mirror = symbols[i].x > field->center.x + field->prefer_left_after_x_px ? -1 : 1;
        uint32_t least = UINT32_MAX;
        for(uint32_t tried = 0; tried < SIDES && least > 0; tried++) {
            side_t side = {(int8_t)(ORDER[tried].horizontal_direction * mirror), ORDER[tried].vertical_direction};
            radar_box_t label = beside(symbols[i], sizes[i], side);
            radar_box_t padded = {label.x1 - LABEL_CLEARANCE_PX, label.y1 - LABEL_CLEARANCE_PX,
                                  label.x2 + LABEL_CLEARANCE_PX, label.y2 + LABEL_CLEARANCE_PX};
#ifdef CONFIG_IDF_TARGET_ESP32S3
            unsigned hits = used ? packed_intersections(blocks, used, &padded)
                                 : intersections(field, symbols, count, labels, i, &padded);
#else
            unsigned hits = intersections(field, symbols, count, labels, i, &padded);
#endif
            uint32_t costs = cost(field, &label, hits);
            if(costs < least) {
                least = costs;
                labels[i] = label;
            }
        }
#ifdef CONFIG_IDF_TARGET_ESP32S3
        if(used) {
            store_box(blocks, i, around(symbols[i], RADAR_SYMBOL_RADIUS_PX));
            store_box(blocks, used++, labels[i]);
        }
#endif
    }
}
