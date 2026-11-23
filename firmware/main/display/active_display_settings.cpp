#include "active_display_settings.hpp"

#include <mutex>

#include "display/settings_store.hpp"
#include "settings/application_settings.hpp"
#include "settings/settings.hpp"

namespace {
std::mutex mutex;

bool read(const char* key, void* out, size_t size) {
	return settings::read(key, out, size) == ESP_OK;
}

bool write(const char* key, const void* blob, size_t size) {
	return settings::write(key, blob, size) == ESP_OK;
}

display::SettingsStore<display::Settings> store{display::kSettingsKey,
												{read, write}};

}

namespace display_settings {
void load() {
	const std::scoped_lock guard(mutex);
	store.load();
}

display::Settings snapshot() {
	const std::scoped_lock guard(mutex);
	return store.settings();
}

bool save(const display::Settings& settings) {
	const std::scoped_lock guard(mutex);
	return store.save(settings);
}

}

namespace application_settings {
esp_err_t erase_application_settings() {
	const std::scoped_lock guard(mutex);
	const esp_err_t result = settings::erase_application_settings();
	if (result == ESP_OK) {
		store.load();
	}
	return result;
}

esp_err_t format_nvs_partition() {
	const std::scoped_lock guard(mutex);
	const esp_err_t result = settings::format_nvs_partition();
	store.load();
	return result;
}
}
