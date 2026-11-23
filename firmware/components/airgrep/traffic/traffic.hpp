#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "esp_err.h"
#include "traffic_protocol.hpp"
#include "traffic_table.hpp"

// The traffic picture: every target the ADSBee reports, kept
// TRAFFIC_MAX_AGE_MS past its last line, and where each was in its last
// kHistorySeconds seconds. Both live in PSRAM behind one mutex. CRC-checked
// lines enter through ingest(), decoded batches through update().
namespace traffic {
#define TRAFFIC_MAX_TARGETS 512
#define TRAFFIC_MAX_AGE_MS 60000
#define TRAFFIC_EVICT_PERIOD_MS 1150
#define TRAFFIC_SAMPLE_PERIOD_MS 1000

struct Counters {
	uint32_t lines = 0, aircraft = 0, flarm = 0, rejected = 0, evicted = 0;
	int64_t last_stats_report_us = 0;
	Stats stats;
};

// Starts the table, eviction and sampling together. A failed start can be
// retried.
esp_err_t start();
// One CSBEE line without the line end. Lines other than #A:, #F: and #S: are
// ignored.
void ingest(const char* text, size_t length, int64_t received_us);
// Applies 1 to 32 reports together, stamping local receipt time.
bool update(const Target* targets, size_t n, int64_t received_us);
bool read_slots(std::span<uint8_t> out);
bool write_slots(size_t offset, std::span<const uint8_t> bytes);
// Visits every target under the lock. Keep the visitor short.
void each(void (*visit)(const Target&, void*), void* context);
// The targets nearest to the reference first, each with its past, at most
// limit and at most kListed. Without a reference the one heard last comes
// first. Returns how many were written and the total present in the same
// snapshot, or nullopt before the table has started.
std::optional<size_t> nearest(const Reference* reference, Tracked* out,
							  size_t limit, uint16_t& present);
// Targets with this address, any source. Returns how many were written, at
// most count.
size_t find(uint32_t address, Target* out, size_t n);
size_t size();
Counters counters();
}
