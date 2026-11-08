#pragma once

#include <cstdint>
#include "device.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

// Time zone from the GNSS position, looked up in the TZL table in the tzdata partition.
// Policy, in one place: tz_detect on -> detected zone's rule, or UTC0 while nothing is detected.
// tz_detect off -> the tz_rule setting. Applied with setenv and tzset.
namespace tz {
#define TZ_PARTITION_LABEL "tzdata"
#define TZ_PARTITION_SUBTYPE 0x40
#define TZ_ZONE_NAME_SIZE 40
#define TZ_RULE_SIZE 64
#define TZ_SOURCE_SIZE 64
// GNSS frames arrive at 1 Hz, a zone border crossing may take a minute to show.
#define TZ_LOOKUP_PERIOD_MS 60000
// A gap longer than this between frames with a fix makes the next one a fresh position.
#define TZ_STALE_MS 5000

	struct State {
		Status status = Status::unknown; // ok: table mapped and valid, off: no partition or table
		uint32_t table_bytes = 0;
		uint16_t zones = 0;
		uint16_t rule_year = 0; // the year the POSIX rules in the table describe
		char source[TZ_SOURCE_SIZE]{}; // <tzdb release>/<dataset> the table was built from
		bool detected = false; // the last position with a gps fix hit a zone
		char zone[TZ_ZONE_NAME_SIZE]{}; // IANA name of the detected zone
		char rule[TZ_RULE_SIZE]{}; // the POSIX rule currently applied, whatever its source
		int64_t detected_us = 0; // monotonic time of the last lookup that produced a zone
		esp_err_t last_error = ESP_OK;

		template <typename F>
		static void update(F f) {
			portENTER_CRITICAL(&mutex_);
			f(storage());
			portEXIT_CRITICAL(&mutex_);
		}

		static State copy();

	private:
		static State& storage();
		static portMUX_TYPE mutex_;
	};

	esp_err_t init();
	void apply();
}
