#ifndef AIRGREP_RADAR_MATH_H
#define AIRGREP_RADAR_MATH_H

#include <stdint.h>

static inline int32_t radar_min_i32(int32_t a, int32_t b)
{
    return a < b ? a : b;
}

static inline int32_t radar_max_i32(int32_t a, int32_t b)
{
    return a > b ? a : b;
}

static inline int32_t radar_clamp_i32(int32_t value, int32_t low, int32_t high)
{
    return value < low ? low : value > high ? high : value;
}

#endif
