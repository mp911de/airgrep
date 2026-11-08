#include "log_session.h"
#include "esp_private/log_level.h"
#include "unity.h"

esp_log_level_t __wrap_esp_log_level_get_timeout(const char* tag);

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[logs]")
#else
#define CASE(name, description) static void name(void)
void setUp(void) {}
void tearDown(void) {}
#endif

CASE(inherited, "Temporary tag levels preserve default inheritance") {
	esp_log_level_t saved = esp_log_get_default_level();
	esp_log_set_default_level(ESP_LOG_INFO);
	log_session_begin(ESP_LOG_DEBUG, "session_inherited");
	TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, __wrap_esp_log_level_get_timeout("session_inherited"));
	TEST_ASSERT_EQUAL(ESP_LOG_INFO, esp_log_level_get("session_inherited"));
	log_session_end();
	esp_log_set_default_level(ESP_LOG_WARN);
	TEST_ASSERT_EQUAL(ESP_LOG_WARN, __wrap_esp_log_level_get_timeout("session_inherited"));
	esp_log_set_default_level(saved);
}

CASE(explicit_level, "Temporary levels preserve explicit tags and match whole tag names") {
	esp_log_level_set("session_explicit", ESP_LOG_ERROR);
	esp_log_level_set("session_explicit_suffix", ESP_LOG_WARN);
	log_session_begin(ESP_LOG_DEBUG, "other,session_explicit");
	TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, __wrap_esp_log_level_get_timeout("session_explicit"));
	TEST_ASSERT_EQUAL(ESP_LOG_WARN, __wrap_esp_log_level_get_timeout("session_explicit_suffix"));
	log_session_end();
	TEST_ASSERT_EQUAL(ESP_LOG_ERROR, __wrap_esp_log_level_get_timeout("session_explicit"));
}

CASE(default_level, "Default sessions restore the default without erasing explicit tags") {
	esp_log_level_t saved = esp_log_get_default_level();
	esp_log_set_default_level(ESP_LOG_INFO);
	esp_log_level_set("session_fixed", ESP_LOG_ERROR);
	log_session_begin(ESP_LOG_DEBUG, NULL);
	TEST_ASSERT_EQUAL(ESP_LOG_DEBUG, esp_log_get_default_level());
	TEST_ASSERT_EQUAL(ESP_LOG_ERROR, __wrap_esp_log_level_get_timeout("session_fixed"));
	log_session_end();
	TEST_ASSERT_EQUAL(ESP_LOG_INFO, esp_log_get_default_level());
	esp_log_set_default_level(saved);
}

#ifndef ESP_PLATFORM
int main(void) {
	UNITY_BEGIN();
	RUN_TEST(inherited);
	RUN_TEST(explicit_level);
	RUN_TEST(default_level);
	return UNITY_END();
}
#endif
