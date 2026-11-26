/* Export the analytic ring and spoke masks used by the fixed-radius atlas. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "radar_private_gen.h"
#include "radar_track.h"
#include "navigation/units.h"
#define WEDGE_INNER_RADIUS_PX 32
#define RECOVERY_DASH 8
#define RECOVERY_GAP 6
#define AIRGREP_EXPORT_RECOVERY
#include "radar_recovery_sectors.h"

int main(void)
{
    lv_init();
    struct recovery_sectors * cache = recovery_sectors_create(176);
    if(!cache) {
        return 1;
    }
    for(int degree = 0; degree <= 45; degree++) {
        if(!recovery_stamp_spoke(cache, degree)) {
            return 1;
        }
    }
    for(int index = 0; index < 54; index++) {
        const recovery_stamp_t * stamp = index < 8 ? &cache->rings[index] : &cache->spokes[index - 8];
        for(size_t i = 0; i < stamp->count; i++) {
            const recovery_pixel_t * p = &stamp->pixels[i];
            printf("%d,%d,%d,%u,%u\n", index, p->x, p->y, p->alpha, p->degree);
        }
    }
    recovery_sectors_release(&cache);
    return 0;
}
