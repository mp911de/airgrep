#ifndef AIRGREP_RADAR_LABELS_H
#define AIRGREP_RADAR_LABELS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define RADAR_LABEL_GAP_PX 20
#define RADAR_SYMBOL_RADIUS_PX 16
#define RADAR_DOT_RADIUS_PX 3

typedef struct {
    int32_t x;
    int32_t y;
} radar_spot_t;

/* x2 and y2 are the last pixel inside. */
typedef struct {
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
} radar_box_t;

typedef struct {
    radar_spot_t center;
    int32_t inner;
    int32_t outer;
    int32_t prefer_left_after_x_px;
    radar_box_t bounds;
    radar_box_t ring_labels[2];
    const radar_spot_t * dots;
    uint32_t dot_count;
} radar_field_t;

/**
 * Places the label of every target beside, below, above or at a corner of its symbol. The first target chooses
 * first.
 * @param field     what lies on the radar
 * @param symbols   the middle of each symbol
 * @param sizes     width and height of each label
 * @param count     number of targets
 * @param labels    room for count boxes
 */
void radar_labels(const radar_field_t * field, const radar_spot_t * symbols, const radar_spot_t * sizes,
                  uint32_t count, radar_box_t * labels);

#ifdef __cplusplus
}
#endif

#endif
