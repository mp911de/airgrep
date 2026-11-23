#include <string.h>

#include "board.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "unity.h"
#include "unity_test_runner.h"

void app_main(void) {
	ESP_LOGI("test", "airgrep tests on %s, tags \"%s\"", BOARD_NAME,
			 CONFIG_AIRGREP_TEST_TAGS);

	char tags[] = CONFIG_AIRGREP_TEST_TAGS;
	char* rest = NULL;
	for (char* tag = strtok_r(tags, " ", &rest); tag;
		 tag = strtok_r(NULL, " ", &rest)) {
		UNITY_BEGIN();
		unity_run_tests_by_tag(tag, false);
		UNITY_END();
	}
	unity_run_menu();
}
