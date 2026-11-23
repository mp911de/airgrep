#include "settings.hpp"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <new>

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
std::mutex nvs_mutex;
bool nvs_ready;
bool preserve_stored_blob;

void publish(const Settings& settings, const Status& status) {
	portENTER_CRITICAL(&mutex);
	current = settings;
	state = status;
	portEXIT_CRITICAL(&mutex);
}

static_assert(sizeof SETTINGS_NVS_KEY <= NVS_KEY_NAME_MAX_SIZE);

esp_err_t update_namespace(auto change) {
	if (!nvs_ready) {
		return ESP_ERR_INVALID_STATE;
	}
	nvs_handle_t handle = 0;
	esp_err_t result = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
	if (result != ESP_OK) {
		return result;
	}
	result = change(handle);
	if (result == ESP_OK) {
		result = nvs_commit(handle);
	}
	nvs_close(handle);
	return result;
}

esp_err_t save(const char* key, const void* blob, size_t size) {
	return update_namespace([&](nvs_handle_t handle) {
		return nvs_set_blob(handle, key, blob, size);
	});
}

esp_err_t save(const Settings& settings) {
	return save(SETTINGS_NVS_KEY, &settings, sizeof settings);
}

// Loads the blob into settings. ESP_ERR_NOT_FOUND when NVS holds none,
// ESP_ERR_INVALID_VERSION when it is not a layout this firmware can migrate.
esp_err_t load(Settings& settings, uint16_t& stored_version) {
	nvs_handle_t handle = 0;
	esp_err_t result = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READONLY, &handle);
	if (result == ESP_ERR_NVS_NOT_FOUND) {
		return ESP_ERR_NOT_FOUND;
	}
	if (result != ESP_OK) {
		return result;
	}
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
	const std::unique_ptr<uint8_t[]> blob(new (std::nothrow) uint8_t[size]);
	if (!blob) {
		nvs_close(handle);
		return ESP_ERR_NO_MEM;
	}
	result = nvs_get_blob(handle, SETTINGS_NVS_KEY, blob.get(), &size);
	nvs_close(handle);
	if (result == ESP_OK) {
		std::memcpy(&stored_version, blob.get(), sizeof stored_version);
		if (!migrate(blob.get(), size, settings)) {
			result = ESP_ERR_INVALID_VERSION;
		}
	}
	return result;
}

esp_err_t commit(const Settings& settings) {
	const esp_err_t result = save(settings);
	Status status = settings::status();
	status.last_error = result;
	if (result == ESP_OK) {
		preserve_stored_blob = false;
		status.loaded = true;
		status.stored_version = settings.version;
		publish(settings, status);
		tz::apply();
	} else {
		publish(settings::copy(), status);
	}
	return result;
}

esp_err_t load_settings() {
	Settings settings = defaults();
	Status status;
	preserve_stored_blob = false;
	esp_err_t result = nvs_flash_init();
	nvs_ready = result == ESP_OK;
	if (nvs_ready) {
		result = load(settings, status.stored_version);
		if (result == ESP_OK) {
			status.loaded = true;
		} else if (result == ESP_ERR_NOT_FOUND) {
			result = save(settings);
		} else {
			ESP_LOGW(tag, "stored settings version %u unusable: %s",
					 status.stored_version, esp_err_to_name(result));
			settings = defaults();
			preserve_stored_blob = true;
		}
	}
	status.last_error = result;
	if (result == ESP_OK && !status.loaded) {
		status.stored_version = settings.version;
	}
	publish(settings, status);
	tz::apply();
	return result;
}

}

esp_err_t init() {
	const std::scoped_lock lock(nvs_mutex);
	return load_settings();
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
	const std::scoped_lock lock(nvs_mutex);
	if (preserve_stored_blob) {
		return ESP_ERR_INVALID_STATE;
	}
	Settings settings = copy();
	if (!assign(settings, name, value)) {
		return ESP_ERR_INVALID_ARG;
	}
	return commit(settings);
}

esp_err_t reset() {
	const std::scoped_lock lock(nvs_mutex);
	return commit(defaults());
}

esp_err_t read(const char* key, void* out, size_t size) {
	if (!std::strcmp(key, SETTINGS_NVS_KEY)) {
		if (size != sizeof(Settings)) {
			return ESP_ERR_NOT_FOUND;
		}
		Settings settings = copy();
		std::memcpy(out, &settings, size);
		return ESP_OK;
	}
	const std::scoped_lock lock(nvs_mutex);
	nvs_handle_t handle = 0;
	if (nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
		return ESP_ERR_NOT_FOUND;
	}
	size_t stored = size;
	const esp_err_t result = nvs_get_blob(handle, key, out, &stored);
	nvs_close(handle);
	return result == ESP_OK && stored == size ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t write(const char* key, const void* blob, size_t size) {
	const std::scoped_lock lock(nvs_mutex);
	if (std::strcmp(key, SETTINGS_NVS_KEY)) {
		return save(key, blob, size);
	}
	Settings settings{};
	if (!migrate(blob, size, settings)) {
		return ESP_ERR_INVALID_ARG;
	}
	return commit(settings);
}

esp_err_t erase_application_settings() {
	const std::scoped_lock lock(nvs_mutex);
	const esp_err_t result = update_namespace(nvs_erase_all);
	if (result == ESP_OK) {
		preserve_stored_blob = false;
		publish(defaults(), Status{});
		tz::apply();
	}
	return result;
}

esp_err_t format_nvs_partition() {
	const std::scoped_lock lock(nvs_mutex);
	esp_err_t result = nvs_flash_deinit();
	if (result == ESP_OK || result == ESP_ERR_NVS_NOT_INITIALIZED) {
		result = nvs_flash_erase();
	}
	if (result != ESP_OK) {
		return result;
	}
	return load_settings();
}
}
