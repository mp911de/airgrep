#include "settings.hpp"
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "tz/tz.hpp"

namespace settings {
	namespace {
		constexpr const char* tag = "settings";
		portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
		Settings current;
		Status state;
		bool nvs_ready;

		void publish(const Settings& settings, const Status& status) {
			portENTER_CRITICAL(&mutex);
			current = settings;
			state = status;
			portEXIT_CRITICAL(&mutex);
		}

		// The time zone policy lives in tz, which reads the settings back.
		void apply(const Settings&) { tz::apply(); }

		esp_err_t save(const Settings& settings) {
			if (!nvs_ready) return ESP_ERR_INVALID_STATE;
			nvs_handle_t handle;
			esp_err_t result = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
			if (result != ESP_OK) return result;
			result = nvs_set_blob(handle, SETTINGS_NVS_KEY, &settings, sizeof settings);
			if (result == ESP_OK) result = nvs_commit(handle);
			nvs_close(handle);
			return result;
		}

		// Loads the blob into settings. ESP_ERR_NOT_FOUND when NVS holds none,
		// ESP_ERR_INVALID_VERSION when it is not a layout this firmware can migrate.
		esp_err_t load(Settings& settings, uint16_t& stored_version) {
			nvs_handle_t handle;
			esp_err_t result = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READONLY, &handle);
			if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_ERR_NOT_FOUND;
			if (result != ESP_OK) return result;
			size_t size = 0;
			result = nvs_get_blob(handle, SETTINGS_NVS_KEY, nullptr, &size);
			if (result == ESP_ERR_NVS_NOT_FOUND) {
				nvs_close(handle);
				return ESP_ERR_NOT_FOUND;
			}
			if (result != ESP_OK || size < sizeof(uint16_t) || size > 4096) {
				nvs_close(handle);
				return result == ESP_OK ? ESP_ERR_INVALID_SIZE : result;
			}
			void* blob = std::malloc(size);
			if (!blob) {
				nvs_close(handle);
				return ESP_ERR_NO_MEM;
			}
			result = nvs_get_blob(handle, SETTINGS_NVS_KEY, blob, &size);
			nvs_close(handle);
			if (result == ESP_OK) {
				std::memcpy(&stored_version, blob, sizeof stored_version);
				if (!migrate(blob, size, settings)) result = ESP_ERR_INVALID_VERSION;
			}
			std::free(blob);
			return result;
		}

		esp_err_t commit(const Settings& settings) {
			esp_err_t result = save(settings);
			Status status = settings::status();
			status.last_error = result;
			if (result == ESP_OK) {
				status.loaded = true;
				status.stored_version = settings.version;
				publish(settings, status);
				apply(settings);
			}
			else publish(settings::copy(), status);
			return result;
		}

		// An unreadable or newer blob stays in NVS until reset or format replace it.
		bool blob_kept;
	}

	esp_err_t init() {
		Settings settings = defaults();
		Status status;
		blob_kept = false;
		esp_err_t result = nvs_flash_init();
		nvs_ready = result == ESP_OK;
		if (nvs_ready) {
			result = load(settings, status.stored_version);
			if (result == ESP_OK) status.loaded = true;
			else if (result == ESP_ERR_NOT_FOUND) result = save(settings);
			else {
				// Unknown layout or a damaged blob: run on defaults, keep the blob for a
				// downgrade or a later firmware, settings --reset or --format overwrite it.
				ESP_LOGW(tag, "stored settings version %u unusable: %s", status.stored_version,
						esp_err_to_name(result));
				settings = defaults();
				blob_kept = true;
			}
		}
		status.last_error = result;
		if (result == ESP_OK && !status.loaded) status.stored_version = settings.version;
		publish(settings, status);
		apply(settings);
		return result;
	}

	Settings copy() {
		portENTER_CRITICAL(&mutex);
		Settings result = current;
		portEXIT_CRITICAL(&mutex);
		return result;
	}

	Status status() {
		portENTER_CRITICAL(&mutex);
		Status result = state;
		portEXIT_CRITICAL(&mutex);
		return result;
	}

	esp_err_t set(const char* name, const char* value) {
		if (blob_kept) return ESP_ERR_INVALID_STATE;
		Settings settings = copy();
		if (!assign(settings, name, value)) return ESP_ERR_INVALID_ARG;
		return commit(settings);
	}

	esp_err_t reset() {
		blob_kept = false;
		return commit(defaults());
	}

	esp_err_t format() {
		esp_err_t result = nvs_flash_deinit();
		if (result == ESP_OK || result == ESP_ERR_NVS_NOT_INITIALIZED) result = nvs_flash_erase();
		if (result != ESP_OK) return result;
		return init();
	}
}
