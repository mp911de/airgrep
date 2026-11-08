#include "tz.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "settings/settings.hpp"
extern "C" {
#include "tz_lookup.h"
}

namespace tz {
	portMUX_TYPE State::mutex_ = portMUX_INITIALIZER_UNLOCKED;

	State& State::storage() {
		static State state;
		return state;
	}

	State State::copy() {
		State result;
		update([&](State& state) { result = state; });
		return result;
	}

	namespace {
		constexpr const char* tag = "tz";
		constexpr const char* kUtc = "UTC0";
		constexpr int32_t kMicrodegrees = 1000000;
		tz_table table;
		bool ready;
		uint16_t detected_zone = TZ_INVALID;
		int64_t lookup_us; // 0 until the first lookup
		int64_t frame_us; // last frame with a fix

		uint32_t little32(const uint8_t* p) {
			return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
		}

		void set_rule(const char* rule) {
			setenv("TZ", rule, 1);
			tzset();
			State::update([&](State& state) {
				std::strncpy(state.rule, rule, sizeof state.rule - 1);
				state.rule[sizeof state.rule - 1] = 0;
			});
		}
	}

	void apply() {
		settings::Settings current = settings::copy();
		if (!current.tz_detect) {
			set_rule(current.tz_rule);
			return;
		}
		// Zones without one POSIX rule (Casablanca, El Aaiun) come back empty and run on UTC.
		const char* rule = ready && detected_zone != TZ_INVALID ? tz_posix(&table, detected_zone) : nullptr;
		set_rule(rule && *rule ? rule : kUtc);
	}

	void position(bool fix, float latitude, float longitude) {
		if (!ready) return;
		int64_t now = esp_timer_get_time();
		if (!fix || !std::isfinite(latitude) || !std::isfinite(longitude)) {
			frame_us = 0;
			return;
		}
		// The period only thins out a healthy 1 Hz stream. A fix regained or a stream that
		// paused is looked up at once.
		bool fresh = !frame_us || now - frame_us > int64_t(TZ_STALE_MS) * 1000;
		frame_us = now;
		if (!fresh && lookup_us && now - lookup_us < int64_t(TZ_LOOKUP_PERIOD_MS) * 1000) return;
		lookup_us = now;
		uint16_t zone = tz_lookup(&table, int32_t(std::lround(latitude * kMicrodegrees)),
								int32_t(std::lround(longitude * kMicrodegrees)));
		if (zone == TZ_INVALID || zone == 0) return; // ocean or outside the table, keep the last zone
		bool changed = zone != detected_zone;
		detected_zone = zone;
		const char* name = tz_name(&table, zone);
		State::update([&](State& state) {
			state.detected = true;
			state.detected_us = now;
			std::strncpy(state.zone, name ? name : "", sizeof state.zone - 1);
			state.zone[sizeof state.zone - 1] = 0;
		});
		if (changed) {
			ESP_LOGI(tag, "zone %s", name ? name : "?");
			apply();
		}
	}

	esp_err_t init() {
		const esp_partition_t* partition = esp_partition_find_first(
			ESP_PARTITION_TYPE_DATA, esp_partition_subtype_t(TZ_PARTITION_SUBTYPE), TZ_PARTITION_LABEL);
		esp_err_t result = ESP_OK;
		const void* data = nullptr;
		esp_partition_mmap_handle_t handle;
		if (!partition) result = ESP_ERR_NOT_FOUND;
		if (result == ESP_OK)
			result = esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &data, &handle);
		if (result == ESP_OK) {
			// The blob states its own length at offset 40, the partition is larger than the table.
			const uint8_t* bytes = static_cast<const uint8_t*>(data);
			uint32_t length = little32(bytes + 40);
			if (length > partition->size || !tz_init(&table, data, length)) result = ESP_ERR_NOT_FOUND;
			else {
				ready = true;
				uint16_t zones = uint16_t(little32(bytes + 20));
				uint16_t year = uint16_t(tz_rule_year(&table));
				const char* source = tz_source(&table);
				State::update([&](State& state) {
					state.status = Status::ok;
					state.table_bytes = length;
					state.zones = zones;
					state.rule_year = year;
					std::strncpy(state.source, source ? source : "", sizeof state.source - 1);
				});
			}
		}
		if (result != ESP_OK) {
			State::update([&](State& state) {
				state.status = Status::off;
				state.last_error = result;
			});
		}
		apply();
		return result;
	}
}
