#include "traffic.hpp"
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
		StaticSemaphore_t lock_storage;
		SemaphoreHandle_t lock;

		struct Guard {
			Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
			~Guard() { xSemaphoreGive(lock); }
		};

		loop::Result evict(loop::Handle, loop::Value&) {
			int64_t now = esp_timer_get_time();
			Guard guard;
			count.evicted += table->evict(now, TRAFFIC_MAX_AGE_MS);
			return loop::Result::done;
		}
	}

	esp_err_t start() {
		if (table) return ESP_OK;
		slots = static_cast<Target*>(heap_caps_malloc(
			sizeof(Target) * TRAFFIC_MAX_TARGETS,
			MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
		if (!slots) return ESP_ERR_NO_MEM;
		for (size_t i = 0; i < TRAFFIC_MAX_TARGETS; i++) new(slots + i) Target{};
		lock = xSemaphoreCreateMutexStatic(&lock_storage);
		static Table storage(slots, TRAFFIC_MAX_TARGETS);
		if (!loop::schedule_periodic(TRAFFIC_EVICT_PERIOD_MS, {"traffic evict", evict}).
			valid())
			return ESP_ERR_NO_MEM;
		table = &storage;
		ESP_LOGI(tag, "%u targets, %u KB", unsigned(TRAFFIC_MAX_TARGETS),
				unsigned(sizeof(Target) * TRAFFIC_MAX_TARGETS / 1024));
		return ESP_OK;
	}

	void ingest(const char* text, size_t length, int64_t received_us) {
		if (!table) return;
		Target target;
		Stats stats;
		switch (classify(text, length)) {
		case Line::aircraft: {
			bool ok = decode_aircraft(text, length, target);
			Guard guard;
			count.lines++;
			if (!ok) {
				count.rejected++;
				return;
			}
			count.aircraft++;
			table->upsert(target, received_us);
			return;
		}
		case Line::flarm: {
			bool ok = decode_flarm(text, length, target);
			Guard guard;
			count.lines++;
			if (!ok) {
				count.rejected++;
				return;
			}
			count.flarm++;
			table->upsert(target, received_us);
			return;
		}
		case Line::stats: {
			bool ok = decode_stats(text, length, stats);
			Guard guard;
			count.lines++;
			if (!ok) {
				count.rejected++;
				return;
			}
			count.stats = stats;
			count.stats_us = received_us;
			return;
		}
		default: return;
		}
	}

	void each(void(*visit)(const Target &, void*), void*context) {
		if (!table) return;
		Guard guard;
		table->each([&](const Target& target) { visit(target, context); });
	}

	size_t find(uint32_t address, Target* out, size_t n) {
		if (!table) return 0;
		Guard guard;
		return table->find(address, out, n);
	}

	size_t size() {
		if (!table) return 0;
		Guard guard;
		return table->size();
	}

	Counters counters() {
		if (!table) return Counters{};
		Guard guard;
		return count;
	}
}
