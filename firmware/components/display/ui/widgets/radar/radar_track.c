#include "radar_track.h"
#include "radar_math.h"
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

/* With a monotone major coordinate and all samples within e of the final line, every
 * intermediate sample is within 2e of its prefix chord. The old prefix search therefore
 * keeps just the two ends. Leave room for float rounding at ordinary screen coordinates. */
static bool straight_prefixes(const radar_point_t * path, uint32_t count)
{
    radar_point_t end = path[count - 1];
    bool vertical = fabsf(end.y) > fabsf(end.x);
    float major = vertical ? end.y : end.x;
    if(path[0].x != 0 || path[0].y != 0 || major == 0 || fabsf(major) > 2048) {
        return false;
    }
    float slope = (vertical ? end.x : end.y) / major;
    float previous = 0;
    for(uint32_t i = 1; i < count; ++i) {
        float along = vertical ? path[i].y : path[i].x;
        float across = vertical ? path[i].x : path[i].y;
        if((major > 0 ? along < previous : along > previous) || fabsf(across - along * slope) > 0.1f) {
            return false;
        }
        previous = along;
    }
    return true;
}

static uint32_t straighten(radar_point_t * path, uint32_t count)
{
    if(count < 3) {
        return count;
    }
    if(straight_prefixes(path, count)) {
        path[1] = path[count - 1];
        return 2;
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

/* Scan-convert the three-pixel ribbon into one cleared A8 mask per trail. */
void radar_track_ribbon(uint8_t * data, int stride, int left, int top, int right, int bottom,
                        const radar_tail_part_t * parts, unsigned count)
{
    enum { limit = RADAR_MAX_TAIL_LENGTH_PX + (RADAR_STROKE_WIDTH_PX + 1) / 2 + 1 };
    _Static_assert(RADAR_STROKE_WIDTH_PX == 3, "ribbon has one opaque and one antialias pixel per side");
    const int xmin = radar_max_i32(left, -limit), xmax = radar_min_i32(right, limit - 1);
    const int ymin = radar_max_i32(top, -limit), ymax = radar_min_i32(bottom, limit - 1);
    for(unsigned i = 0; i < count; ++i) {
        const radar_tail_part_t * part = &parts[i];
        float dx = part->to.x - part->from.x, dy = part->to.y - part->from.y;
        float length2 = dx * dx + dy * dy;
        /* Scan across the short axis v. The two long quad edges then have slope at most one. */
        bool vertical = fabsf(dy) > fabsf(dx);
        float u0 = vertical ? part->from.y : part->from.x, v0 = vertical ? part->from.x : part->from.y;
        float u1 = vertical ? part->to.y : part->to.x, v1 = vertical ? part->to.x : part->to.y;
        float du = u1 - u0, dv = v1 - v0;
        float slope = du != 0 ? dv / du : 0.0f;
        float normal = length2 > 0 ? fabsf(du) / sqrtf(length2) : 1.0f;
        float half_span = 2.0f / normal;
        float low = fminf(u0, u1), high = fmaxf(u0, u1);
        /* Centers lie at n + 0.5. Expand the two-pixel reach by 0.001 for float roundoff. */
        int umin = vertical ? ymin : xmin, umax = vertical ? ymax : xmax;
        int vmin = vertical ? xmin : ymin, vmax = vertical ? xmax : ymax;
        int first = radar_max_i32((int)ceilf(low - 2.501f), umin);
        int last = radar_min_i32((int)floorf(high + 1.501f), umax);
        int opacity = 255 * (RADAR_TAIL_STEPS - part->step) / RADAR_TAIL_STEPS;
        for(int u = first; u <= last; ++u) {
            float uc = (float)u + 0.5f, offset = uc - u0;
            /* The quad edges are parallel. Pixel centers in this interval span the entire strip. */
            float center = v0 + offset * slope;
            int start = radar_max_i32((int)ceilf(center - half_span - 0.501f), vmin);
            int end = radar_min_i32((int)floorf(center + half_span - 0.499f), vmax);
            bool near_cap = uc < low + 2.001f || uc > high - 2.001f;
            for(int v = start; v <= end; ++v) {
                float vc = (float)v + 0.5f;
                float distance = fabsf(vc - center) * normal;
                if(near_cap) {
                    float along = offset * du + (vc - v0) * dv;
                    if(along <= 0.0f || along >= length2) {
                        bool far = along > 0.0f;
                        float eu = uc - (far ? u1 : u0), ev = vc - (far ? v1 : v0);
                        float d2 = eu * eu + ev * ev;
                        if(d2 >= 4.0f) {
                            continue;
                        }
                        distance = d2 <= 1.0f ? 0.0f : sqrtf(d2);
                    }
                }
                if(distance >= 2.0f) {
                    continue;
                }
                unsigned value = (unsigned)opacity;
                if(distance > 1.0f) {
                    float rounded = (2.0f - distance) * (float)opacity + 0.5f;
                    value = (unsigned)rounded;
                    /* At a byte-rounding boundary, reproduce the original float operations. */
                    float remainder = rounded - (float)value;
                    if(remainder < 0.006f || remainder > 0.994f) {
                        float cx = vertical ? vc : uc, cy = vertical ? uc : vc;
                        float projection = (cx - part->from.x) * dx + (cy - part->from.y) * dy;
                        float t = length2 > 0 ? fminf(fmaxf(projection / length2, 0), 1) : 0;
                        float ex = part->from.x + dx * t - cx, ey = part->from.y + dy * t - cy;
                        float coverage = fminf(fmaxf(2.0f - hypotf(ex, ey), 0), 1);
                        value = (unsigned)lroundf(coverage * (float)opacity);
                    }
                }
                int x = vertical ? v : u, y = vertical ? u : v;
                uint8_t * pixel = data + (size_t)(y - top) * stride + (x - left);
                /* Meeting caps and self-crossings share coverage, including at a change of fade level. */
                if(value > *pixel) {
                    *pixel = (uint8_t)value;
                }
            }
        }
    }
}

void radar_track_tail_runs(const radar_tail_part_t * parts, uint32_t count, float width, radar_tail_run_cb_t run,
                          void * user)
{
    enum { REACH = RADAR_MAX_TAIL_LENGTH_PX + (RADAR_STROKE_WIDTH_PX + 1) / 2 + 1, ROWS = 4 };
    uint8_t pixels[ROWS * 2 * REACH];
    float reach = width * 0.5f + 0.5f;
    int32_t left = REACH, right = -REACH, top = REACH, bottom = -REACH;
    for(uint32_t i = 0; i < count; i++) {
        const radar_tail_part_t * p = &parts[i];
        left = radar_min_i32(left, (int32_t)floorf(fminf(p->from.x, p->to.x) - reach));
        right = radar_max_i32(right, (int32_t)ceilf(fmaxf(p->from.x, p->to.x) + reach));
        top = radar_min_i32(top, (int32_t)floorf(fminf(p->from.y, p->to.y) - reach));
        bottom = radar_max_i32(bottom, (int32_t)ceilf(fmaxf(p->from.y, p->to.y) + reach));
    }
    top = radar_max_i32(top, -REACH);
    bottom = radar_min_i32(bottom, REACH - 1);
    left = radar_max_i32(left, -REACH);
    right = radar_min_i32(right, REACH - 1);
    if(right < left) {
        return;
    }
    int32_t stride = right - left + 1;
    for(int32_t y0 = top; y0 <= bottom; y0 += ROWS) {
        int32_t y1 = radar_min_i32(y0 + ROWS - 1, bottom);
        memset(pixels, 0, (size_t)((y1 - y0 + 1) * stride));
        for(uint32_t i = 0; i < count; ++i) {
            const radar_tail_part_t * p = &parts[i];
            int32_t first = radar_max_i32(y0, (int32_t)ceilf(fminf(p->from.y, p->to.y) - reach - 0.5f));
            int32_t last = radar_min_i32(y1, (int32_t)floorf(fmaxf(p->from.y, p->to.y) + reach - 0.5f));
            if(last < first) {
                continue;
            }
            int32_t x0 = radar_max_i32(left, (int32_t)floorf(fminf(p->from.x, p->to.x) - reach));
            int32_t x1 = radar_min_i32(right, (int32_t)ceilf(fmaxf(p->from.x, p->to.x) + reach));
            float dx = p->to.x - p->from.x, dy = p->to.y - p->from.y;
            float len2 = dx * dx + dy * dy;
            float opa = 255.0f * (float)(RADAR_TAIL_STEPS - p->step) / RADAR_TAIL_STEPS;
            for(int32_t y = first; y <= last; ++y) {
                uint8_t * row = pixels + (size_t)(y - y0) * (size_t)stride;
                float cy = (float)y + 0.5f, py = cy - p->from.y;
                for(int32_t x = x0; x <= x1; ++x) {
                    float cx = (float)x + 0.5f, px = cx - p->from.x;
                    float projection = px * dx + py * dy;
                    radar_point_t q = p->from;
                    if(len2 > 0) {
                        q = between(p->from, p->to, fminf(fmaxf(projection / len2, 0), 1));
                    }
                    float ex = q.x - cx, ey = q.y - cy;
                    float d2 = ex * ex + ey * ey;
                    if(d2 > reach * reach + 0.0001f) {
                        continue;
                    }
                    float inside = reach - 1;
                    float distance_px = inside >= 0 && d2 < inside * inside - 0.0001f ? 0 : hypotf(ex, ey);
                    float coverage = fminf(reach - distance_px, 1);
                    if(coverage <= 0) {
                        continue;
                    }
                    uint8_t value = (uint8_t)lroundf(coverage * opa);
                    if(value > row[x - left]) {
                        row[x - left] = value;
                    }
                }
            }
        }
        for(int32_t y = y0; y <= y1; ++y) {
            const uint8_t * row = pixels + (size_t)(y - y0) * (size_t)stride;
            for(int32_t x = 0; x < stride;) {
                int32_t end = x + 1;
                while(end < stride && row[x] == row[end]) {
                    ++end;
                }
                if(row[x]) {
                    run(user, x + left, y, end - x, row[x]);
                }
                x = end;
            }
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
