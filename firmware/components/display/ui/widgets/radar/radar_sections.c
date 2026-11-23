#include "radar_sections.h"

#define MIN_SEPARATE_BEARING_GAP_DEG (RADAR_SECTOR_WIDTH_DEG + RADAR_MIN_SECTOR_GAP_DEG)

static void sort(int16_t * values, uint32_t count)
{
    for(uint32_t i = 1; i < count; i++) {
        int16_t value = values[i];
        uint32_t at = i;
        for(; at > 0 && values[at - 1] > value; at--) {
            values[at] = values[at - 1];
        }
        values[at] = value;
    }
}

static int16_t clockwise_gap_deg(const int16_t * bearings_deg, uint32_t count, uint32_t from)
{
    uint32_t to = (from + 1) % count;
    return (int16_t)(bearings_deg[to] - bearings_deg[from] + (to ? 0 : 360));
}

uint32_t radar_sections(int16_t * bearings_deg, uint32_t count, radar_section_t * sections)
{
    if(count == 0) {
        return 0;
    }
    sort(bearings_deg, count);

    /* The walk begins behind the widest clockwise_gap_deg, so no section lies across its start. */
    uint32_t widest = 0;
    for(uint32_t i = 1; i < count; i++) {
        if(clockwise_gap_deg(bearings_deg, count, i) > clockwise_gap_deg(bearings_deg, count, widest)) {
            widest = i;
        }
    }
    if(clockwise_gap_deg(bearings_deg, count, widest) < MIN_SEPARATE_BEARING_GAP_DEG) {
        sections[0].start_deg = 0;
        sections[0].sweep_deg = 360;
        return 1;
    }

    uint32_t used = 0;
    for(uint32_t i = 0; i < count; i++) {
        uint32_t at = (widest + 1 + i) % count;
        uint32_t before = (at + count - 1) % count;
        if(i > 0 && clockwise_gap_deg(bearings_deg, count, before) < MIN_SEPARATE_BEARING_GAP_DEG) {
            sections[used - 1].sweep_deg =
                (int16_t)(sections[used - 1].sweep_deg + clockwise_gap_deg(bearings_deg, count, before));
            continue;
        }
        sections[used].start_deg = (int16_t)((bearings_deg[at] - RADAR_SECTOR_WIDTH_DEG / 2 + 360) % 360);
        sections[used].sweep_deg = RADAR_SECTOR_WIDTH_DEG;
        used++;
    }
    return used;
}
