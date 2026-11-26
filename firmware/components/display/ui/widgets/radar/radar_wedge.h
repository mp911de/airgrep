#ifndef AIRGREP_RADAR_WEDGE_H
#define AIRGREP_RADAR_WEDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern char radar_wedge_tag;

/* The radar's partially opaque RGB565 wedge. Strides are in bytes. */
void radar_wedge_blend_rows(uint16_t *dest, int dest_stride, const uint8_t *mask, int mask_stride,
                            int width, int height, uint16_t color, uint8_t opacity);

#ifdef __cplusplus
}
#endif

#endif
