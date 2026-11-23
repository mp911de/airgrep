#include "radar_track.h"
#include <math.h>
#include "navigation/units.h"
#include <stdbool.h>
#include <string.h>

#define MAX_PATH_ERROR_PX 0.25f

static float distance(radar_point_t from, radar_point_t to)
{
    return hypotf(to.x - from.x, to.y - from.y);
}

static radar_point_t between(radar_point_t from, radar_point_t to, float way)
{
    radar_point_t point = {from.x + (to.x - from.x) * way, from.y + (to.y - from.y) * way};
    return point;
}

static radar_point_t predicted_position_at(float speed_px_per_s, float heading_rad, float turn_rad_per_s, float seconds)
{
    radar_point_t point;
    if(turn_rad_per_s == 0) {
        point.x = speed_px_per_s * seconds * sinf(heading_rad);
        point.y = -speed_px_per_s * seconds * cosf(heading_rad);
    }
    else {
        float radius = speed_px_per_s / turn_rad_per_s;
        point.x = radius * (cosf(heading_rad) - cosf(heading_rad + turn_rad_per_s * seconds));
        point.y = -radius * (sinf(heading_rad + turn_rad_per_s * seconds) - sinf(heading_rad));
    }
    return point;
}

static float distance_to_segment(radar_point_t point, radar_point_t from, radar_point_t to)
{
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float length_squared = dx * dx + dy * dy;
    if(length_squared == 0) {
        return distance(point, from);
    }
    float fraction = ((point.x - from.x) * dx + (point.y - from.y) * dy) / length_squared;
    return distance(point, between(from, to, fminf(fmaxf(fraction, 0), 1)));
}

static uint32_t straighten(radar_point_t * path, uint32_t count)
{
    if(count < 3) {
        return count;
    }
    uint32_t kept = 1;
    uint32_t from = 0;
    for(uint32_t to = 2; to < count; to++) {
        bool straight = true;
        for(uint32_t i = from + 1; straight && i < to; i++) {
            straight = distance_to_segment(path[i], path[from], path[to]) <= MAX_PATH_ERROR_PX;
        }
        if(!straight) {
            from = to - 1;
            path[kept++] = path[from];
        }
    }
    path[kept++] = path[count - 1];
    return kept;
}

uint32_t radar_track_tail(radar_point_t * path, uint32_t count, radar_tail_part_t * parts)
{
    count = straighten(path, count);
    float length = 0;
    for(uint32_t i = 1; i < count; i++) {
        length += distance(path[i - 1], path[i]);
    }
    if(length > RADAR_MAX_TAIL_LENGTH_PX) {
        length = RADAR_MAX_TAIL_LENGTH_PX;
    }

    uint32_t part_count = 0;
    float segment_start_px = 0;
    for(uint32_t i = 1; i < count && segment_start_px < length; i++) {
        float piece = distance(path[i - 1], path[i]);
        if(piece <= 0) {
            continue;
        }
        float end = fminf(segment_start_px + piece, length);
        uint32_t step = (uint32_t)(segment_start_px * RADAR_TAIL_STEPS / length);
        float from = segment_start_px;
        while(from < end && step < RADAR_TAIL_STEPS) {
            float to = fminf(length * (float)(step + 1) / RADAR_TAIL_STEPS, end);
            if(step == RADAR_TAIL_STEPS - 1) {
                to = end;
            }
            if(to > from) {
                parts[part_count].from = between(path[i - 1], path[i], (from - segment_start_px) / piece);
                parts[part_count].to = between(path[i - 1], path[i], (to - segment_start_px) / piece);
                parts[part_count].step = (uint8_t)step;
                part_count++;
            }
            from = to;
            step++;
        }
        segment_start_px += piece;
    }
    return part_count;
}

static int32_t larger(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static int32_t smaller(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

void radar_track_tail_runs(const radar_tail_part_t * parts, uint32_t count, float width, radar_tail_run_cb_t run,
                           void * user)
{
    enum { REACH = RADAR_MAX_TAIL_LENGTH_PX + (RADAR_STROKE_WIDTH_PX + 1) / 2 + 1 };
    uint8_t row[2 * REACH];
    float reach = width / 2 + 0.5f;

    float top = REACH;
    float bottom = -REACH;
    for(uint32_t i = 0; i < count; i++) {
        top = fminf(top, fminf(parts[i].from.y, parts[i].to.y));
        bottom = fmaxf(bottom, fmaxf(parts[i].from.y, parts[i].to.y));
    }
    int32_t first_row = larger((int32_t)floorf(top - reach), -REACH);
    int32_t last_row = smaller((int32_t)ceilf(bottom + reach), REACH - 1);

    for(int32_t y = first_row; y <= last_row; y++) {
        float middle = (float)y + 0.5f;
        memset(row, 0, sizeof(row));
        for(uint32_t i = 0; i < count; i++) {
            const radar_tail_part_t * part = &parts[i];
            if(middle < fminf(part->from.y, part->to.y) - reach || middle > fmaxf(part->from.y, part->to.y) + reach) {
                continue;
            }
            int32_t from = larger((int32_t)floorf(fminf(part->from.x, part->to.x) - reach), -REACH);
            int32_t to = smaller((int32_t)ceilf(fmaxf(part->from.x, part->to.x) + reach), REACH - 1);
            float opa = 255.0f * (float)(RADAR_TAIL_STEPS - part->step) / RADAR_TAIL_STEPS;
            for(int32_t x = from; x <= to; x++) {
                radar_point_t pixel_center = {(float)x + 0.5f, middle};
                float covered = fminf(reach - distance_to_segment(pixel_center, part->from, part->to), 1);
                if(covered <= 0) {
                    continue;
                }
                uint8_t value = (uint8_t)lroundf(covered * opa);
                /* Where two parts meet the pixel is the more covered of the two, not their sum. */
                if(value > row[x + REACH]) {
                    row[x + REACH] = value;
                }
            }
        }
        for(int32_t x = -REACH; x < REACH;) {
            int32_t end = x + 1;
            while(end < REACH && row[end + REACH] == row[x + REACH]) {
                end++;
            }
            if(row[x + REACH]) {
                run(user, x, y, end - x, row[x + REACH]);
            }
            x = end;
        }
    }
}

uint32_t radar_track_dots(float speed, float track_deg, float turn_dps, radar_point_t * dots)
{
    float heading = track_deg * radians_per_degree;
    float turn = turn_dps * radians_per_degree;
    float step = speed * RADAR_DOT_STEP_S;
    uint32_t every = step >= RADAR_DOT_STEP_ALL ? 1 : step >= RADAR_DOT_STEP_SOME ? 2
                     : step >= RADAR_DOT_STEP_ENDS ? RADAR_DOT_COUNT - 1 : RADAR_DOT_COUNT;
    radar_point_t symbol = {0, 0};

    /* How far along the path the clearance lies. A turn bends the path, so there it is further along. A turn too tight
     * to ever get that far has no such place. */
    float diameter = turn == 0 ? 0 : 2 * speed / fabsf(turn);
    bool leaves = turn == 0 || diameter >= RADAR_TRACK_CLEAR;
    float clear = turn == 0 || !leaves ? RADAR_TRACK_CLEAR : diameter * asinf(RADAR_TRACK_CLEAR / diameter);

    uint32_t dot_count = 0;
    /* Counted from the last dot, so the end of the prediction always shows. Each dot lies as far along the path as the
     * target flies to it, measured from the nose. */
    for(uint32_t i = 1; leaves && i <= RADAR_DOT_COUNT; i++) {
        if((RADAR_DOT_COUNT - i) % every) {
            continue;
        }
        radar_point_t dot = predicted_position_at(speed, heading, turn, fmaxf(clear, RADAR_TRACK_NOSE + (float)i * step) / speed);
        if(distance(symbol, dot) >= RADAR_TRACK_CLEAR - 0.01f) {
            dots[dot_count++] = dot;
        }
    }
    if(dot_count) {
        return dot_count;
    }

    if(leaves) {
        dots[0] = predicted_position_at(speed, heading, turn, clear / speed);
    }
    else {
        dots[0].x = RADAR_TRACK_CLEAR * sinf(heading);
        dots[0].y = -RADAR_TRACK_CLEAR * cosf(heading);
    }
    return 1;
}
