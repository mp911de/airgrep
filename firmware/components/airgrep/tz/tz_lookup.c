#include "tz_lookup.h"
#include <string.h>

static uint32_t u16(const uint8_t *p) { return p[0] | ((uint32_t)p[1] << 8); }
static uint32_t u24(const uint8_t *p) { return u16(p) | ((uint32_t)p[2] << 16); }
static uint32_t u32(const uint8_t *p) { return u24(p) | ((uint32_t)p[3] << 24); }
static uint32_t record(const uint8_t *p, uint32_t i, uint32_t bits) {
    uint32_t bit = i * bits;
    const uint8_t *v = p + 2 + bit / 8;
    uint64_t value = u32(v) | ((uint64_t)v[4] << 32);
    return (uint32_t)((value >> (bit % 8)) & ((UINT64_C(1) << bits) - 1));
}

int tz_init(tz_table *t, const void *blob, uint32_t bytes) {
    const uint8_t *d = blob;
    uint32_t mode, step, w, h, zones, index, payload, zi, strings, i, j, ref;
    t->data = 0;
    if (!d || bytes < 128 || memcmp(d, "TZL2", 4) || u32(d + 40) != bytes ||
        !d[64] || !memchr(d + 64, 0, 64)) return 0;
    mode = u32(d + 4); step = u32(d + 8); w = u32(d + 12); h = u32(d + 16);
    zones = u32(d + 20); index = u32(d + 24); payload = u32(d + 28);
    zi = u32(d + 32); strings = u32(d + 36);
    if (!step || 360000000u % step || 180000000u % step ||
        w != 360000000u / step || h != 180000000u / step || w > 65535 ||
        zones < 2 || zones > 65535 || index != 128 || payload < index || zi < payload ||
        strings < zi || strings > bytes || strings - zi != zones * 8 ||
        u32(d + 52) < 2000 || u32(d + 52) > 2200) return 0;
    for (i = 0; i < zones * 2; ++i) {
        ref = u32(d + zi + i * 4);
        if (ref >= bytes - strings || !memchr(d + strings + ref, 0, bytes - strings - ref)) return 0;
    }
    if (mode == 1) {
        uint32_t xb = u32(d + 56), zb = u32(d + 60), bits = xb + zb;
        if (!xb || xb > 16 || !zb || zb > 16 || w > (1u << xb) ||
            zones > (1u << zb) || (w - 1) < (1u << (xb - 1)) ||
            (zones - 1) < (1u << (zb - 1))) return 0;
        if (payload - index != h * 3 || u32(d + 44) || u32(d + 48)) return 0;
        /* Every row's record block must lie inside the payload. The records themselves are
           not decoded here: that walk over the whole payload costs about 450 ms from
           memory-mapped flash, and a mis-sorted run only yields a wrong zone, never an
           out-of-bounds read. */
        for (i = 0; i < h; ++i) {
            uint32_t offset = u24(d + index + i * 3), n;
            if (zi - payload < 2 || offset > zi - payload - 2) return 0;
            ref = payload + offset; n = u16(d + ref);
            if (!n || (n * bits + 7) / 8 > zi - ref - 2) return 0;
        }
    } else if (mode == 2) {
        uint32_t nodes = (zi - payload) / 12, side = u32(d + 48);
        if (payload != index || (zi - payload) % 12 || !side || (side & (side - 1)) ||
            side < w || side < h || side > 65536 || u32(d + 44) >= zones + nodes ||
            u32(d + 56) || u32(d + 60)) return 0;
        /* Backward references make cycles impossible. */
        for (i = 0; i < nodes; ++i)
            for (j = 0; j < 4; ++j)
                if (u24(d + payload + i * 12 + j * 3) >= zones + i) return 0;
    } else return 0;
    t->data = d;
    return 1;
}

uint16_t tz_lookup(const tz_table *t, int32_t lat, int32_t lon) {
    const uint8_t *d = t->data;
    uint32_t x, y, step, ref, span, zones, base, lo, hi, mid;
    if (!d || lat < -90000000 || lat > 90000000 || lon < -180000000 || lon > 180000000) return TZ_INVALID;
    step = u32(d + 8); zones = u32(d + 20); base = u32(d + 28);
    x = lon == 180000000 ? 0 : (uint32_t)(lon + 180000000) / step;
    y = lat == 90000000 ? u32(d + 16) - 1 : (uint32_t)(lat + 90000000) / step;
    if (u32(d + 4) == 1) {
        uint32_t xb = u32(d + 56), bits = xb + u32(d + 60);
        ref = base + u24(d + u32(d + 24) + y * 3);
        lo = 0; hi = u16(d + ref);
        while (lo + 1 < hi) {
            mid = (lo + hi) / 2;
            if ((record(d + ref, mid, bits) & ((1u << xb) - 1)) <= x) lo = mid;
            else hi = mid;
        }
        return (uint16_t)(record(d + ref, lo, bits) >> xb);
    }
    ref = u32(d + 44); span = u32(d + 48);
    while (ref >= zones) {
        uint32_t child;
        if (span < 2) return TZ_INVALID;
        span /= 2;
        child = (y >= span ? 2u : 0u) | (x >= span ? 1u : 0u);
        x %= span; y %= span;
        ref = u24(d + base + (ref - zones) * 12 + child * 3);
    }
    return (uint16_t)ref;
}

static const char *text(const tz_table *t, uint16_t zone, uint32_t field) {
    const uint8_t *d = t->data;
    if (!d || zone >= u32(d + 20)) return 0;
    return (const char *)(d + u32(d + 36) + u32(d + u32(d + 32) + zone * 8 + field));
}
const char *tz_name(const tz_table *t, uint16_t zone) { return text(t, zone, 0); }
const char *tz_posix(const tz_table *t, uint16_t zone) { return text(t, zone, 4); }
uint32_t tz_rule_year(const tz_table *t) { return t && t->data ? u32(t->data + 52) : 0; }
const char *tz_source(const tz_table *t) { return t && t->data ? (const char *)(t->data + 64) : 0; }
