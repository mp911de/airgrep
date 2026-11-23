#include "radar_labels.h"

#include <stdbool.h>

#define SIDES 8
#define DIAGONAL_LABEL_GAP_PX 14
#define LABEL_CLEARANCE_PX 3
#define RING_CLEARANCE_PX 6
/* A label over another label, a symbol, a dot or the edge cannot be read. One over a ring can, so two rings weigh less
 * than one of those. */
#define COVERS 8
#define CROSSES_RING 3

static int32_t clamp(int32_t value, int32_t low, int32_t high)
{
    return value < low ? low : value > high ? high : value;
}

static int32_t larger(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static int32_t magnitude(int32_t value)
{
    return value < 0 ? -value : value;
}

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
    int32_t near_x = clamp(center.x, box->x1, box->x2) - center.x;
    int32_t near_y = clamp(center.y, box->y1, box->y2) - center.y;
    int32_t far_x = larger(magnitude(box->x1 - center.x), magnitude(box->x2 - center.x));
    int32_t far_y = larger(magnitude(box->y1 - center.y), magnitude(box->y2 - center.y));
    int32_t reach = radius + RING_CLEARANCE_PX;
    int32_t hole = larger(radius - RING_CLEARANCE_PX, 0);
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

static uint32_t cost(const radar_field_t * field, const radar_spot_t * symbols, uint32_t count,
                     const radar_box_t * labels, uint32_t index, const radar_box_t * label)
{
    radar_box_t padded = {label->x1 - LABEL_CLEARANCE_PX, label->y1 - LABEL_CLEARANCE_PX, label->x2 + LABEL_CLEARANCE_PX, label->y2 + LABEL_CLEARANCE_PX};
    uint32_t sum = inside(label, &field->bounds) ? 0 : COVERS;

    radar_box_t own = around(field->center, RADAR_SYMBOL_RADIUS_PX);
    if(overlap(&padded, &own)) {
        sum += COVERS;
    }
    for(uint32_t i = 0; i < count; i++) {
        radar_box_t symbol = around(symbols[i], RADAR_SYMBOL_RADIUS_PX);
        if(i != index && overlap(&padded, &symbol)) {
            sum += COVERS;
        }
        if(i < index && overlap(&padded, &labels[i])) {
            sum += COVERS;
        }
    }
    for(uint32_t i = 0; i < 2; i++) {
        if(overlap(&padded, &field->ring_labels[i])) {
            sum += COVERS;
        }
    }
    for(uint32_t i = 0; i < field->dot_count; i++) {
        radar_box_t dot = around(field->dots[i], RADAR_DOT_RADIUS_PX);
        if(overlap(&padded, &dot)) {
            sum += COVERS;
        }
    }
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

    for(uint32_t i = 0; i < count; i++) {
        int8_t mirror = symbols[i].x > field->center.x + field->prefer_left_after_x_px ? -1 : 1;
        uint32_t least = UINT32_MAX;
        for(uint32_t tried = 0; tried < SIDES && least > 0; tried++) {
            side_t side = {(int8_t)(ORDER[tried].horizontal_direction * mirror), ORDER[tried].vertical_direction};
            radar_box_t label = beside(symbols[i], sizes[i], side);
            uint32_t costs = cost(field, symbols, count, labels, i, &label);
            if(costs < least) {
                least = costs;
                labels[i] = label;
            }
        }
    }
}
