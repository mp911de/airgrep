#include "tz_lookup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const uint8_t *p) {
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int32_t micro(double degrees) {
    double scaled = degrees * 1000000;
    return (int32_t)(scaled + (scaled < 0 ? -.5 : .5));
}
static void point(const tz_table *t, double lat, double lon) {
    uint16_t id;
    if (!(lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180)) {
        puts("65535\tinvalid\t"); return;
    }
    id = tz_lookup(t, micro(lat), micro(lon));
    printf("%u\t%s\t%s\n", id, tz_name(t, id), tz_posix(t, id));
}
int main(int argc, char **argv) {
    FILE *f;
    long size;
    uint8_t *data;
    tz_table table = {0};
    int result = 0;
    if (tz_rule_year(&table) || tz_source(&table) || tz_rule_year(NULL) || tz_source(NULL)) return 1;
    if (argc != 2 && argc != 4 && !(argc == 3 && !strcmp(argv[2], "--info"))) {
        fprintf(stderr, "usage: %s table [--info | lat lon | --check raster]\n", argv[0]); return 2;
    }
    f = fopen(argv[1], "rb");
    if (!f) return 2;
    if (fseek(f, 0, SEEK_END)) return 2;
    size = ftell(f);
    if (size < 0 || (unsigned long)size > UINT32_MAX || fseek(f, 0, SEEK_SET)) return 2;
    data = malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, f) != (size_t)size) return 2;
    fclose(f);
    if (!tz_init(&table, data, (uint32_t)size)) {
        result = tz_rule_year(&table) || tz_source(&table) ? 1 : 2;
        fprintf(stderr, "invalid table\n"); free(data); return result;
    }
    if (argc == 3) {
        printf("%u\t%s\n", (unsigned)tz_rule_year(&table), tz_source(&table));
    } else if (argc == 4 && !strcmp(argv[2], "--check")) {
        uint32_t x, y, w = u32(data + 12), h = u32(data + 16), step = u32(data + 8);
        uint8_t *row = malloc(w * 2);
        unsigned long long checked = 0;
        f = fopen(argv[3], "rb");
        if (!f || !row) return 2;
        for (y = 0; y < h; ++y) {
            if (fread(row, 2, w, f) != w) { result = 2; break; }
            for (x = 0; x < w; ++x) {
                uint16_t got = tz_lookup(&table, (int32_t)(y * step + step / 2) - 90000000,
                                        (int32_t)(x * step + step / 2) - 180000000);
                uint16_t expected = (uint16_t)(row[x * 2] | ((uint16_t)row[x * 2 + 1] << 8));
                if (got != expected) { fprintf(stderr, "mismatch at %u,%u\n", x, y); result = 1; goto done; }
                ++checked;
            }
        }
        if (fgetc(f) != EOF) result = 2;
        printf("checked %llu cells, result %d\n", checked, result);
    done:
        fclose(f); free(row);
    } else if (argc == 4) point(&table, strtod(argv[2], 0), strtod(argv[3], 0));
    else {
        double lat, lon;
        while (scanf("%lf %lf", &lat, &lon) == 2) point(&table, lat, lon);
    }
    free(data);
    return result;
}
