#pragma once

#include <cstddef>
#include <cstdint>
#include "esp_err.h"
#include "traffic_protocol.hpp"

// The traffic picture: every target the ADSBee reports, kept TRAFFIC_MAX_AGE_MS past its last
// line. The table lives in PSRAM behind one mutex. Lines enter through ingest(), today from the
// ADSBee link, later from a mock feed over TCP as well, both already CRC checked.
namespace traffic {
#define TRAFFIC_MAX_TARGETS 512
#define TRAFFIC_MAX_AGE_MS 60000
#define TRAFFIC_EVICT_PERIOD_MS 1150

	struct Counters {
		uint32_t lines = 0, aircraft = 0, flarm = 0, rejected = 0, evicted = 0;
		int64_t stats_us = 0; // last #S: line
		Stats stats;
	};

	// Allocates the table and starts eviction. ESP_ERR_NO_MEM without PSRAM.
	esp_err_t start();
	// One CSBEE line without the line end. Lines other than #A:, #F: and #S: are ignored.
	void ingest(const char* text, size_t length, int64_t received_us);
	// Visits every target under the lock. Keep the visitor short.
	void each(void(*visit)(const Target &, void*), void*context);
	// Targets with this address, any source. Returns how many were written, at most count.
	size_t find(uint32_t address, Target* out, size_t count);
	size_t size();
	Counters counters();
}
