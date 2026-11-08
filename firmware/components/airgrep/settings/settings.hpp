#pragma once

#include "esp_err.h"
#include "settings_model.hpp"

// Persistent settings: one versioned struct in NVS, a RAM copy for readers on any task.
// Writes happen on the caller's task and block for the NVS commit.
namespace settings {
#define SETTINGS_NVS_NAMESPACE "airgrep"
#define SETTINGS_NVS_KEY "settings"

	struct Status {
		bool loaded = false; // the RAM copy came from NVS, otherwise defaults
		uint16_t stored_version = 0; // version found in NVS, 0 when absent or unreadable
		esp_err_t last_error = ESP_OK;
	};

	// Initializes NVS, loads and migrates the blob, applies the time zone. Defaults on any failure.
	esp_err_t init();
	Settings copy();
	Status status();
	esp_err_t set(const char* name, const char* value);
	esp_err_t reset();
	// Erases the whole NVS partition and starts over with defaults. Explicit console action only.
	esp_err_t format();
}
