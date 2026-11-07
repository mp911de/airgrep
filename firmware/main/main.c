#include "airgrep.h"
#include "adsbee_link.h"
#include "board.h"
#include "buzzer.h"
#include "esp_log.h"
#include "i2c_bus.h"

static const char* TAG = "airgrep";

static void on_adsbee_line(const char* line, size_t len, bool csbee) {
	ESP_LOGI(TAG, "%s %.*s", csbee ? "csbee" : "at", (int)len, line);
}

void app_main(void) {
	ESP_LOGI(TAG, "airgrep on %s", BOARD_NAME);

	ESP_ERROR_CHECK(i2c_bus_init());
	i2c_bus_log_scan();

	ESP_ERROR_CHECK(buzzer_init());
	buzzer_beep_ms(50);

	ESP_ERROR_CHECK(adsbee_link_start(on_adsbee_line));
	adsbee_link_send("AT+DEVICE_INFO?");
}
