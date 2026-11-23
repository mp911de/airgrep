#ifndef AIRGREP_RADAR_SECTIONS_H
#define AIRGREP_RADAR_SECTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define RADAR_SECTOR_WIDTH_DEG 20
#define RADAR_MIN_SECTOR_GAP_DEG 15
/* More do not fit a ring: each takes a sector and a gap. */
#define RADAR_MAX_SECTIONS (360 / (RADAR_SECTOR_WIDTH_DEG + RADAR_MIN_SECTOR_GAP_DEG))

/* From start_deg, 0 to 359 with 0 up, clockwise over sweep_deg. A sweep of 360 is the whole ring. */
typedef struct {
    int16_t start_deg;
    int16_t sweep_deg;
} radar_section_t;

/**
 * The sections of targets at these bearings.
 * @param bearings_deg  0 to 359 each. Left sorted.
 * @param count         number of bearings
 * @param sections      room for RADAR_MAX_SECTIONS
 * @return              number of sections
 */
uint32_t radar_sections(int16_t * bearings_deg, uint32_t count, radar_section_t * sections);

#ifdef __cplusplus
}
#endif

#endif
