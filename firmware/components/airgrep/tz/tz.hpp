#pragma once

#include <cstdint>

#include "device.h"
#include "esp_err.h"
#include "locked_state.hpp"

// Time zone from the GNSS position, looked up in the TZL table in the tzdata
// partition. Policy, in one place: tz_detect on -> detected zone's rule, or
// UTC0 while nothing is detected. tz_detect off -> the rule the table holds
// for the zone setting, UTC0 for a zone it does not know.
// Applied with setenv and tzset.
namespace tz {
#define TZ_PARTITION_LABEL "tzdata"
#define TZ_PARTITION_SUBTYPE 0x40
#define TZ_ZONE_NAME_SIZE 40
#define TZ_RULE_SIZE 64
#define TZ_SOURCE_SIZE 64
// GNSS frames arrive at 1 Hz, a zone border crossing may take a minute to
// show.
#define TZ_LOOKUP_PERIOD_MS 60000

enum class TableStatus : uint8_t { unknown, available, unavailable };

struct State : LockedState<State> {
	TableStatus table_status = TableStatus::unknown;
	uint32_t table_bytes = 0;
	uint16_t zones = 0;
	uint16_t rule_year = 0;  // the year the POSIX rules in the table describe
	char source[TZ_SOURCE_SIZE]{};  // <tzdb release>/<dataset> the table was
									// built from
	bool has_detected_zone = false;
	char detected_iana_zone[TZ_ZONE_NAME_SIZE]{};
	char applied_posix_rule[TZ_RULE_SIZE]{};
	int64_t last_zone_match_us = 0;
	esp_err_t last_error = ESP_OK;
};

esp_err_t init();
void apply();
}
