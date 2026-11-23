#include "traffic.hpp"

#include <cstring>
#include <mutex>
#include <new>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "loop.hpp"
#include "traffic_table.hpp"

namespace traffic {
namespace {
constexpr const char* tag = "traffic";
Target* slots;
Table* table;
Counters count;
std::mutex mutex;
using Guard = std::scoped_lock<std::mutex>;

loop::Result evict(loop::Handle handle, loop::Value&) {
	const int64_t now = esp_timer_get_time();
	const Guard guard(mutex);
	if (table && loop::pending(handle)) {
		count.evicted += table->evict(now, TRAFFIC_MAX_AGE_MS);
	}
	return loop::Result::done;
}

loop::Result sample(loop::Handle, loop::Value&) {
	const int64_t now = esp_timer_get_time();
	const Guard guard(mutex);
	table->sample(now);
	return loop::Result::done;
}
}

esp_err_t start() {
	const Guard guard(mutex);
	if (table) {
		return ESP_OK;
	}
	auto* targets = static_cast<Target*>(
		heap_caps_malloc(sizeof(Target) * TRAFFIC_MAX_TARGETS,
						 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	if (!targets) {
		return ESP_ERR_NO_MEM;
	}
	const size_t positions = size_t{TRAFFIC_MAX_TARGETS} * kHistorySeconds;
	auto* history = static_cast<Position*>(heap_caps_malloc(
		sizeof(Position) * positions, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	if (!history) {
		heap_caps_free(targets);
		return ESP_ERR_NO_MEM;
	}
	for (size_t i = 0; i < TRAFFIC_MAX_TARGETS; i++) {
		new (targets + i) Target{};
	}
	for (size_t i = 0; i < positions; i++) {
		new (history + i) Position{};
	}
	auto eviction = loop::schedule_periodic(TRAFFIC_EVICT_PERIOD_MS,
											{"traffic evict", evict});
	if (!eviction.valid() ||
		!loop::schedule_periodic(TRAFFIC_SAMPLE_PERIOD_MS,
								 {"traffic sample", sample})
			 .valid()) {
		loop::cancel(eviction);
		heap_caps_free(history);
		heap_caps_free(targets);
		return ESP_ERR_NO_MEM;
	}
	static Table storage(targets, TRAFFIC_MAX_TARGETS, history);
	slots = targets;
	table = &storage;
	ESP_LOGI(tag, "%u targets, %u KB, %u s of past, %u KB",
			 unsigned(TRAFFIC_MAX_TARGETS),
			 unsigned(sizeof(Target) * TRAFFIC_MAX_TARGETS / 1024),
			 unsigned(kHistorySeconds),
			 unsigned(sizeof(Position) * positions / 1024));
	return ESP_OK;
}

void ingest(const char* text, size_t length, int64_t received_us) {
	Target target;
	Stats stats;
	const Line line = classify(text, length);
	bool decoded = false;
	switch (line) {
		case Line::aircraft:
			decoded = decode_aircraft(text, length, target);
			break;
		case Line::flarm:
			decoded = decode_flarm(text, length, target);
			break;
		case Line::stats:
			decoded = decode_stats(text, length, stats);
			break;
		default:
			return;
	}
	const Guard guard(mutex);
	if (!table) {
		return;
	}
	count.lines++;
	if (!decoded) {
		count.rejected++;
	} else if (line == Line::stats) {
		count.stats = stats;
		count.last_stats_report_us = received_us;
	} else {
		if (line == Line::aircraft) {
			count.aircraft++;
		} else {
			count.flarm++;
		}
		table->upsert(target, received_us);
	}
}

void each(void (*visit)(const Target&, void*), void* context) {
	const Guard guard(mutex);
	if (!table) {
		return;
	}
	table->each([&](const Target& target) { visit(target, context); });
}

bool update(const Target* targets, size_t n, int64_t received_us) {
	const Guard guard(mutex);
	if (!table) {
		return false;
	}
	return table->upsert_batch(targets, n, received_us);
}

bool read_slots(std::span<uint8_t> out) {
	const Guard guard(mutex);
	if (!table || out.size() != sizeof(Target) * TRAFFIC_MAX_TARGETS) {
		return false;
	}
	std::memcpy(out.data(), slots, out.size());
	return true;
}

bool write_slots(size_t offset, std::span<const uint8_t> bytes) {
	const Guard guard(mutex);
	return table && table->write_bytes(offset, bytes);
}

std::optional<size_t> nearest(const Reference* reference, Tracked* out,
							  size_t limit, uint16_t& present) {
	const Guard guard(mutex);
	if (!table) {
		return std::nullopt;
	}
	static_assert(TRAFFIC_MAX_TARGETS <= UINT16_MAX);
	present = static_cast<uint16_t>(table->size());
	return limit ? table->nearest(reference, out, limit) : 0;
}

size_t find(uint32_t address, Target* out, size_t n) {
	const Guard guard(mutex);
	if (!table) {
		return 0;
	}
	return table->find(address, out, n);
}

size_t size() {
	const Guard guard(mutex);
	if (!table) {
		return 0;
	}
	return table->size();
}

Counters counters() {
	if (!table) {
		return Counters{};
	}
	const Guard guard(mutex);
	return count;
}
}
