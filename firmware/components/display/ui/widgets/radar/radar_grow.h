#ifndef AIRGREP_RADAR_GROW_H
#define AIRGREP_RADAR_GROW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * Grows a mask of one byte per pixel: each pixel takes the strongest one within the radius.
 * @param from          the mask, width times height
 * @param width         its width
 * @param height        its height
 * @param from_stride   bytes from one of its rows to the next
 * @param to            room for a mask that is two radii wider and higher
 * @param to_stride     bytes from one of its rows to the next
 * @param radius        how far the mask grows to every side
 */
void radar_grow(const uint8_t * from, uint32_t width, uint32_t height, uint32_t from_stride, uint8_t * to,
                uint32_t to_stride, uint32_t radius);

#ifdef __cplusplus
}
#endif

#endif
