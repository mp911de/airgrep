#include "adsbee/adsbee_link.hpp"
#include "baro/barometer.hpp"
#include "battery/max17048.hpp"
#include "buzzer/buzzer.h"
#include "charger/bq25798.hpp"
#include "console.h"
#include "esp_log.h"
#include "i2c_expander/tca9534.hpp"
#include "usb/fusb303.hpp"
#include <cstdio>
#include "i2c_bus.h"
#include "loop.hpp"
#include "sdkconfig.h"
#include "settings/settings.hpp"
#include "tz/tz.hpp"
#include "wifi/wifi_ap.hpp"
#include "device_info.h"
#if CONFIG_AIRGREP_HWTEST
#include "hwtest.hpp"
#endif

static const char* TAG = "airgrep";

using Completion = loop::Callable<void(esp_err_t)>;

struct Component {
	const char* name;
	void (*init)(Completion complete);
};

static bool is_csbee(const adsbee::Message& m) {
	return m.kind == DECODER_CSBEE;
}

static void log_csbee(const adsbee::Message& m, void*) {
	ESP_LOGV(TAG, "csbee %s", m.text);
}

static void start_adsbee(Completion complete) {
	if (!adsbee::subscribe(is_csbee, log_csbee, nullptr)) {
		ESP_LOGW(TAG, "csbee subscriber not registered");
	}
	complete(adsbee::start());
}

// Boot order is the table order. The loop and the I/O ports come first, the expander last
// because buzzer, adsbee and wifi subscribe to its pins during their init.
static const Component components[] = {
	{"Event loop", [](Completion complete) { loop::start(); complete(ESP_OK); }},
	{"Settings", [](Completion complete) { complete(settings::init()); }},
	{"Time zones", [](Completion complete) { complete(tz::init()); }},
	{"I2C", [](Completion complete) { complete(i2c_bus_init()); }},
	{"ADSbee", start_adsbee},
	{
		"Wi-Fi",
		[](Completion complete) { complete(wifi::start(device_info()->device_id)); }
	},
	{"Charger", charger::init},
	{"Battery", battery::init},
	{"Baro", barometer::init},
	{"FUSB", fusb::init},
	{"Buzzer", [](Completion complete) { complete(buzzer_init()); }},
	{"I2CExpander", expander::init},
};

// Same label column as the banner rows above it.
#define BOOT_LABEL_WIDTH 16

static void report(const Component& component, esp_err_t result) {
	if (result == ESP_OK) std::printf("%-*s OK\n", BOOT_LABEL_WIDTH, component.name);
	else if (result == ESP_ERR_NOT_FOUND) std::printf("%-*s OFF\n", BOOT_LABEL_WIDTH, component.name);
	else std::printf("%-*s FAIL %s\n", BOOT_LABEL_WIDTH, component.name, esp_err_to_name(result));
}

extern "C" void app_main(void) {
	console_banner();
	for (const Component& component : components) {
		component.init([&component](esp_err_t outcome) {
			report(component, outcome);
		});
	}
#if CONFIG_AIRGREP_HWTEST
	hwtest::run();
#endif
	ESP_ERROR_CHECK(console_start());
}
