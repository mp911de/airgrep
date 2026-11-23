#include "radar_grow.h"

#include <string.h>

void radar_grow(const uint8_t * from, uint32_t width, uint32_t height, uint32_t from_stride, uint8_t * to,
                uint32_t to_stride, uint32_t radius)
{
    int32_t reach = (int32_t)radius;
    for(uint32_t y = 0; y < height + 2 * radius; y++) {
        memset(to + (size_t)y * to_stride, 0, width + 2 * radius);
    }

    for(uint32_t y = 0; y < height; y++) {
        for(uint32_t x = 0; x < width; x++) {
            uint8_t value = from[y * from_stride + x];
            if(value == 0) {
                continue;
            }
            for(int32_t dy = -reach; dy <= reach; dy++) {
                for(int32_t dx = -reach; dx <= reach; dx++) {
                    /* One more than the radius squared rounds the corners of a small circle. */
                    if(dx * dx + dy * dy > reach * reach + 1) {
                        continue;
                    }
                    uint8_t * grown = to + (size_t)(y + radius + dy) * to_stride + x + radius + dx;
                    if(*grown < value) {
                        *grown = value;
                    }
                }
            }
        }
    }
}
