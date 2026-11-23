#ifndef AIRGREP_RADAR_TRACK_H
#define AIRGREP_RADAR_TRACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define RADAR_MAX_TAIL_LENGTH_PX 72
#define RADAR_STROKE_WIDTH_PX 3
#define RADAR_TAIL_STEPS 5
#define RADAR_DOT_COUNT 5
#define RADAR_DOT_STEP_S 6
/* The dots begin at the nose of the symbol, this far from its middle: from the nose to the first dot is as far as from dot to
 * dot. */
#define RADAR_TRACK_NOSE 16
#define RADAR_DOT_STEP_ALL 12
#define RADAR_DOT_STEP_SOME 6
#define RADAR_DOT_STEP_ENDS 3
#define RADAR_TRACK_CLEAR 20

/* Pixels from the symbol of the target, x to the right and y down. */
typedef struct {
    float x;
    float y;
} radar_point_t;

/* A straight piece of a tail, drawn at the opacity of its step. Step 0 is the newest. */
typedef struct {
    radar_point_t from;
    radar_point_t to;
    uint8_t step;
} radar_tail_part_t;

/**
 * Cut a path to the tail that is drawn.
 * @param path      the places of the target, the present one first, then the past. Left reduced to its corners.
 * @param count     number of places
 * @param parts     room for count + RADAR_TAIL_STEPS parts
 * @return          number of parts
 */
uint32_t radar_track_tail(radar_point_t * path, uint32_t count, radar_tail_part_t * parts);

typedef void (*radar_tail_run_cb_t)(void * user, int32_t x, int32_t y, int32_t length, uint8_t opa);

/**
 * The pixels of a tail. A line of the renderer ends on a whole pixel, which makes a curved tail of short lines uneven,
 * and two lines blended one after the other are darker where they meet. So each pixel is given once: how much of it
 * the tail covers, at the opacity of the step.
 * @param parts     the parts of the tail, from radar_track_tail()
 * @param count     number of parts
 * @param width     of the tail in pixels
 * @param run       called for each run of pixels, row by row from the top, x and y from the symbol
 * @param user      handed on to run
 */
void radar_track_tail_runs(const radar_tail_part_t * parts, uint32_t count, float width, radar_tail_run_cb_t run,
                           void * user);

/**
 * The dots of a target that keeps its speed and its turn.
 * @param speed     pixels per second, above zero
 * @param track_deg its course, 0 is up, clockwise
 * @param turn_dps  degrees per second, to the right is positive
 * @param dots      room for RADAR_DOT_COUNT dots
 * @return          number of dots, at least one, the nearest first
 */
uint32_t radar_track_dots(float speed, float track_deg, float turn_dps, radar_point_t * dots);

#ifdef __cplusplus
}
#endif

#endif
