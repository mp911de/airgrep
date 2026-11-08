#ifndef TZ_LOOKUP_H
#define TZ_LOOKUP_H
#include <stdint.h>

#define TZ_INVALID UINT16_MAX

typedef struct { const uint8_t *data; } tz_table;

/* Validates a complete immutable TZL2 blob. Returns 1 on success, 0 on error. */
int tz_init(tz_table *table, const void *data, uint32_t bytes);
/* Integer microdegrees. +180 wraps to -180. +90 uses the northernmost row. */
uint16_t tz_lookup(const tz_table *table, int32_t latitude, int32_t longitude);
/* Returned strings point into the blob. NULL for invalid zone IDs. */
const char *tz_name(const tz_table *table, uint16_t zone);
/* Empty rule for none or a zone not representable by one POSIX rule. */
const char *tz_posix(const tz_table *table, uint16_t zone);
/* Informational rule year, 2000..2200. Zero for an empty or NULL context. */
uint32_t tz_rule_year(const tz_table *table);
/* Source comment in the blob. NULL for an empty or NULL context. */
const char *tz_source(const tz_table *table);
#endif
