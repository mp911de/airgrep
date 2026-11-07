#include "adsbee_link.hpp"
#include "board.h"
#include "buzzer.h"
#include "console.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "loop.hpp"
#include "sdkconfig.h"
#if CONFIG_AIRGREP_HWTEST
#include "hwtest.hpp"
#endif

static const char* TAG = "airgrep";

static bool is_csbee(const adsbee::Message& m) {
	return m.kind == DECODER_CSBEE;
}

static void log_csbee(const adsbee::Message& m, void*) {
	ESP_LOGI(TAG, "csbee %s", m.text);
}

extern "C" void app_main(void) {
	ESP_LOGI(TAG, "airgrep on %s", BOARD_NAME);
	loop::start();
	ESP_ERROR_CHECK(console_start());

	ESP_ERROR_CHECK(i2c_bus_init());
	ESP_ERROR_CHECK(buzzer_init());
	buzzer_beep_ms(50);

	if (!adsbee::subscribe(is_csbee, log_csbee, nullptr)) {
		ESP_LOGW(TAG, "csbee subscriber not registered");
	}
	ESP_ERROR_CHECK(adsbee::start());
	loop::Handle info = adsbee::request(
		"AT+DEVICE_INFO?",
		[](loop::Value v) {
			ESP_LOGI(TAG, "at %s", static_cast<const adsbee::Message*>(v.p)->text);
		},
		[](loop::Outcome o) {
			ESP_LOGW(TAG, "adsbee not responding, outcome %d", (int)o);
		});
	if (!info.valid()) {
		ESP_LOGW(TAG, "device info request dropped");
	}

#if CONFIG_AIRGREP_HWTEST
	hwtest::run();
#endif
}
