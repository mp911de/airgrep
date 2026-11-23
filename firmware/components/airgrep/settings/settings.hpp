#pragma once

#include "esp_err.h"
#include "settings_model.hpp"

// Persistent settings: one versioned struct in NVS, a RAM copy for readers on
// any task. Writes happen on the caller's task and block for the NVS commit.
// A display stores a struct of its own under its own key.
namespace settings {
#define SETTINGS_NVS_NAMESPACE "airgrep"

struct Status {
	bool loaded = false;  // the RAM copy came from NVS, otherwise defaults
	uint16_t stored_version =
		0;  // version found in NVS, 0 when absent or unreadable
	esp_err_t last_error = ESP_OK;
};

// Initializes NVS, loads and migrates the blob, applies the time zone.
// Defaults on any failure.
esp_err_t init();
Settings copy();
Status status();
esp_err_t set(const char* name, const char* value);
esp_err_t reset();
// Erases the whole NVS partition and starts over with defaults. Explicit
// console action only.
esp_err_t format_nvs_partition();

// The struct stored under a key of the namespace, for the one who knows its
// layout. Under SETTINGS_NVS_KEY these are the settings in use: read returns
// them, write replaces them.
// ESP_ERR_NOT_FOUND when nothing of that size is stored under the key.
esp_err_t read(const char* key, void* out, size_t size);
esp_err_t write(const char* key, const void* blob, size_t size);
// Removes every key of the namespace. The settings in use are the defaults.
esp_err_t erase_application_settings();
}
