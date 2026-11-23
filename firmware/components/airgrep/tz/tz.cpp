#include "tz.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>

#include "adsbee/adsbee_link.hpp"
#include "esp_log.h"
#include "esp_partition.h"
#include "settings/settings.hpp"
extern "C" {
#include "tz_lookup.h"
}

namespace tz {
namespace {
constexpr const char* tag = "tz";
constexpr const char* kUtc = "UTC0";
constexpr int32_t kMicrodegrees = 1000000;
tz_table table;
bool ready;
std::mutex rule_mutex;
uint16_t detected_zone = TZ_INVALID;
int64_t last_lookup_us;

void set_rule(const char* rule) {
	setenv("TZ", rule, 1);
	tzset();
	State::update([&](State& state) {
		std::strncpy(state.applied_posix_rule, rule,
					 sizeof state.applied_posix_rule - 1);
		state.applied_posix_rule[sizeof state.applied_posix_rule - 1] = 0;
	});
}

const char* rule_of(const char* name) {
	if (!ready) {
		return nullptr;
	}
	for (uint16_t zone = 0; const char* known = tz_name(&table, zone); zone++) {
		if (std::strcmp(known, name) == 0) {
			return tz_posix(&table, zone);
		}
	}
	return nullptr;
}
void apply_locked() {
	settings::Settings current = settings::copy();
	// The chosen zone is stored by name and its rule is read from the table,
	// so a table of a newer release changes the rule of a zone already
	// chosen. Zones without one POSIX rule (Casablanca, El Aaiun) come back
	// empty and run on UTC, as an unknown or no zone does.
	const char* rule = !current.tz_detect ? rule_of(current.zone)
					   : ready && detected_zone != TZ_INVALID
						   ? tz_posix(&table, detected_zone)
						   : nullptr;
	set_rule(rule && *rule ? rule : kUtc);
}
}

void apply() {
	const std::scoped_lock lock(rule_mutex);
	apply_locked();
}

static void on_gnss(const adsbee::Gnss& gnss, int64_t received_us,
					bool acquired, void*) {
	if (!ready || gnss.fix == adsbee::Fix::none ||
		!std::isfinite(gnss.latitude) || !std::isfinite(gnss.longitude)) {
		return;
	}
	// The period only thins out a healthy 1 Hz stream. A fix regained or a
	// stream that paused is looked up at once.
	if (!acquired && last_lookup_us &&
		received_us - last_lookup_us < int64_t(TZ_LOOKUP_PERIOD_MS) * 1000) {
		return;
	}
	last_lookup_us = received_us;
	const uint16_t zone =
		tz_lookup(&table, int32_t(std::lround(gnss.latitude * kMicrodegrees)),
				  int32_t(std::lround(gnss.longitude * kMicrodegrees)));
	if (zone == TZ_INVALID || zone == 0) {
		return;  // ocean or outside the table, keep the last zone
	}
	const std::scoped_lock lock(rule_mutex);
	const bool changed = zone != detected_zone;
	detected_zone = zone;
	const char* name = tz_name(&table, zone);
	State::update([&](State& state) {
		state.has_detected_zone = true;
		state.last_zone_match_us = received_us;
		std::strncpy(state.detected_iana_zone, name ? name : "",
					 sizeof state.detected_iana_zone - 1);
		state.detected_iana_zone[sizeof state.detected_iana_zone - 1] = 0;
	});
	if (changed) {
		ESP_LOGI(tag, "zone %s", name ? name : "?");
		apply_locked();
	}
}

esp_err_t init() {
	const esp_partition_t* partition = esp_partition_find_first(
		ESP_PARTITION_TYPE_DATA, esp_partition_subtype_t(TZ_PARTITION_SUBTYPE),
		TZ_PARTITION_LABEL);
	esp_err_t result = ESP_OK;
	const void* data = nullptr;
	esp_partition_mmap_handle_t handle = 0;
	if (!partition) {
		result = ESP_ERR_NOT_FOUND;
	}
	if (result == ESP_OK) {
		result = esp_partition_mmap(partition, 0, partition->size,
									ESP_PARTITION_MMAP_DATA, &data, &handle);
	}
	if (result == ESP_OK) {
		if (!tz_mount(&table, data, partition->size)) {
			result = ESP_ERR_NOT_FOUND;
		} else {
			ready = true;
			if (!adsbee::subscribe_gnss(on_gnss, nullptr)) {
				result = ESP_ERR_NO_MEM;
			}
			uint16_t zones = tz_zone_count(&table);
			uint16_t year = uint16_t(tz_rule_year(&table));
			const char* source = tz_source(&table);
			State::update([&](State& state) {
				state.table_status = TableStatus::available;
				state.table_bytes = tz_table_size(&table);
				state.zones = zones;
				state.rule_year = year;
				std::strncpy(state.source, source ? source : "",
							 sizeof state.source - 1);
			});
		}
	}
	if (result != ESP_OK) {
		State::update([&](State& state) {
			state.table_status = TableStatus::unavailable;
			state.last_error = result;
		});
	}
	apply();
	return result;
}
}
