#ifndef AIRGREP_RADAR_BANDS_H
#define AIRGREP_RADAR_BANDS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "radar_track.h"

#define RADAR_MAX_BANDS 32

static inline int32_t radar_bearing_deg(int32_t degrees)
{
    return (degrees % 360 + 360) % 360;
}
/* Each Band begins and ends at most one wedge and one limit. */
#define RADAR_MAX_WEDGES (2 * RADAR_MAX_BANDS)

typedef enum {
    RADAR_BAND_NONE = 0,
    RADAR_BAND_PREVENTIVE = 1,
    RADAR_BAND_CORRECTIVE = 2,
    RADAR_BAND_WARNING = 3,
    RADAR_BAND_RECOVERY = 4,
} radar_band_level_t;

typedef struct {
    float from_deg;
    float to_deg;
    radar_band_level_t level;
} radar_track_band_t;

typedef struct {
    float from_fraction;
    float to_fraction;
    radar_band_level_t level;
} radar_tape_band_t;

/* From start_deg, 0 to 359 with 0 the own track, clockwise over sweep_deg. A sweep of 360 is the whole disc. */
typedef struct {
    int16_t start_deg;
    int16_t sweep_deg;
    radar_band_level_t level;
} radar_wedge_t;

typedef struct {
    int16_t deg;
    radar_band_level_t level;
} radar_limit_t;

typedef struct {
    radar_wedge_t wedges[RADAR_MAX_WEDGES];
    uint32_t wedge_count;
    radar_limit_t limits[RADAR_MAX_WEDGES];
    uint32_t limit_count;
} radar_track_bands_t;

/* A part of a tape, y from its upper end. */
typedef struct {
    int16_t y;
    int16_t height;
    radar_band_level_t level;
} radar_stretch_t;

/**
 * The wedges and the limits of track Bands. No wedge lies on another one: where two Bands hold, the graver one is
 * drawn, and the way out cuts through all. Two Bands that meet share one limit in the colour of the graver. A Band
 * all around has no limit, and the way out has none, its edge is drawn instead.
 * @param bands     degrees clockwise from the own track, in any turn. Whole degrees are drawn.
 * @param count     number of Bands
 * @param found     what to draw
 */
void radar_bands_track(const radar_track_band_t * bands, uint32_t count, radar_track_bands_t * found);

/**
 * The stretches of a tape, the gravest last, so it lies on top. The way out is no stretch.
 * @param bands     shares of what the tape spans to either side of its middle: -1 is its lower end, 1 its upper end.
 *                  What lies beyond ends at the end of the tape.
 * @param count     number of Bands
 * @param height    of the tape in pixels
 * @param stretches room for count stretches
 * @return          number of stretches
 */
uint32_t radar_bands_tape(const radar_tape_band_t * bands, uint32_t count, int32_t height, radar_stretch_t * stretches);

float radar_bands_edge_length(const radar_wedge_t * wedge, int32_t inner, int32_t outer);

/**
 * A point on the edge of a wedge, from the centre of the radar. The edge begins on the inner ring where the wedge
 * starts, goes out, along the outer ring, in again and back along the inner ring.
 * @param along     pixels from where the edge begins
 */
radar_point_t radar_bands_edge(const radar_wedge_t * wedge, int32_t inner, int32_t outer, float along);

#ifdef __cplusplus
}
#endif

#endif
