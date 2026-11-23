#include "radar_bands.h"
#include <math.h>
#include "navigation/units.h"


static radar_point_t polar(float deg, float radius)
{
    radar_point_t point = {sinf(deg * radians_per_degree) * radius, -cosf(deg * radians_per_degree) * radius};
    return point;
}

void radar_bands_track(const radar_track_band_t * bands, uint32_t count, radar_track_bands_t * found)
{
    uint8_t level_by_bearing_deg[360] = {RADAR_BAND_NONE};
    for(uint32_t i = 0; i < count; i++) {
        int32_t from = (int32_t)lroundf(bands[i].from_deg);
        int32_t sweep = (int32_t)lroundf(bands[i].to_deg) - from;
        if(sweep > 360) {
            sweep = 360;
        }
        for(int32_t deg = 0; deg < sweep; deg++) {
            uint8_t * at = &level_by_bearing_deg[radar_bearing_deg(from + deg)];
            if(bands[i].level == RADAR_BAND_RECOVERY || (*at != RADAR_BAND_RECOVERY && bands[i].level > *at)) {
                *at = (uint8_t)bands[i].level;
            }
        }
    }

    found->wedge_count = 0;
    found->limit_count = 0;
    /* The walk begins where something changes, so no wedge lies across its start. */
    int32_t first = -1;
    for(int32_t deg = 0; deg < 360 && first < 0; deg++) {
        if(level_by_bearing_deg[deg] != level_by_bearing_deg[(deg + 359) % 360]) {
            first = deg;
        }
    }
    if(first < 0) {
        if(level_by_bearing_deg[0]) {
            radar_wedge_t whole = {0, 360, (radar_band_level_t)level_by_bearing_deg[0]};
            found->wedges[found->wedge_count++] = whole;
        }
        return;
    }
    for(int32_t i = 0; i < 360; i++) {
        int32_t deg = (first + i) % 360;
        uint8_t before = level_by_bearing_deg[(deg + 359) % 360];
        uint8_t level = level_by_bearing_deg[deg];
        if(level == before) {
            if(level && found->wedge_count > 0) {
                found->wedges[found->wedge_count - 1].sweep_deg++;
            }
            continue;
        }
        if(level && found->wedge_count < RADAR_MAX_WEDGES) {
            radar_wedge_t wedge = {(int16_t)deg, 1, (radar_band_level_t)level};
            found->wedges[found->wedge_count++] = wedge;
        }
        if(level != RADAR_BAND_RECOVERY && before != RADAR_BAND_RECOVERY && found->limit_count < RADAR_MAX_WEDGES) {
            radar_limit_t limit = {(int16_t)deg, (radar_band_level_t)(level > before ? level : before)};
            found->limits[found->limit_count++] = limit;
        }
    }
}

uint32_t radar_bands_tape(const radar_tape_band_t * bands, uint32_t count, int32_t height, radar_stretch_t * stretches)
{
    uint32_t used = 0;
    for(int32_t level = RADAR_BAND_PREVENTIVE; level <= RADAR_BAND_WARNING; level++) {
        for(uint32_t i = 0; i < count; i++) {
            if(bands[i].level != (radar_band_level_t)level) {
                continue;
            }
            float to = fminf(fmaxf(bands[i].to_fraction, -1), 1);
            float from = fminf(fmaxf(bands[i].from_fraction, -1), 1);
            int32_t top = (int32_t)lroundf((1 - to) * (float)height / 2);
            int32_t bottom = (int32_t)lroundf((1 - from) * (float)height / 2);
            if(bottom <= top) {
                continue;
            }
            radar_stretch_t stretch = {(int16_t)top, (int16_t)(bottom - top), (radar_band_level_t)level};
            stretches[used++] = stretch;
        }
    }
    return used;
}

float radar_bands_edge_length(const radar_wedge_t * wedge, int32_t inner, int32_t outer)
{
    return (float)(2 * (outer - inner)) + (float)wedge->sweep_deg * radians_per_degree * (float)(outer + inner);
}

radar_point_t radar_bands_edge(const radar_wedge_t * wedge, int32_t inner, int32_t outer, float along)
{
    float side = (float)(outer - inner);
    float sweep = (float)wedge->sweep_deg * radians_per_degree;
    float start = (float)wedge->start_deg;
    float end = (float)(wedge->start_deg + wedge->sweep_deg);
    if(along < side) {
        return polar(start, (float)inner + along);
    }
    along -= side;
    if(along < sweep * (float)outer) {
        return polar(start + along / (float)outer / radians_per_degree, (float)outer);
    }
    along -= sweep * (float)outer;
    if(along < side) {
        return polar(end, (float)outer - along);
    }
    along -= side;
    return polar(end - along / (float)inner / radians_per_degree, (float)inner);
}
