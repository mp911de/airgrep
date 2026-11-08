#include "log_session.h"
#include <string.h>
#include "esp_private/log_level.h"
#include "freertos/FreeRTOS.h"

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static const char* selected_tags;
static esp_log_level_t selected_level, saved_default;
static bool default_changed;

void log_session_begin(esp_log_level_t level, const char* tags) {
	portENTER_CRITICAL(&lock);
	selected_level = level;
	selected_tags = tags;
	default_changed = tags == NULL;
	if (default_changed) {
		saved_default = esp_log_get_default_level();
		esp_log_set_default_level(level > saved_default ? level : saved_default);
	}
	portEXIT_CRITICAL(&lock);
}

void log_session_end(void) {
	portENTER_CRITICAL(&lock);
	selected_tags = NULL;
	if (default_changed) esp_log_set_default_level(saved_default);
	default_changed = false;
	portEXIT_CRITICAL(&lock);
}

esp_log_level_t __real_esp_log_level_get_timeout(const char* tag);

// Temporary overrides leave SDK tag entries and default-level inheritance intact.
esp_log_level_t __wrap_esp_log_level_get_timeout(const char* tag) {
	esp_log_level_t level = __real_esp_log_level_get_timeout(tag);
	portENTER_CRITICAL(&lock);
	const char* next = selected_tags;
	while (tag && next && *next) {
		size_t length = strcspn(next, ",");
		if (strlen(tag) == length && !strncmp(tag, next, length)) {
			if (selected_level > level) level = selected_level;
			break;
		}
		next += length;
		if (*next) ++next;
	}
	portEXIT_CRITICAL(&lock);
	return level;
}
