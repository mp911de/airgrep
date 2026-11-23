#include <cstdio>
#include <iterator>

#include "adsbee/adsbee_clock.hpp"
#include "adsbee/adsbee_link.hpp"
#include "baro/barometer.hpp"
#include "battery/max17048.hpp"
#include "buzzer/buzzer.h"
#include "charger/bq25798.hpp"
#include "console.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "i2c_bus.h"
#include "i2c_expander/tca9534.hpp"
#include "loop.hpp"
#include "sdkconfig.h"
#include "settings/settings.hpp"
#include "system/device_info.h"
#include "system/system.h"
#include "traffic/traffic.hpp"
#include "tz/tz.hpp"
#include "usb/fusb303.hpp"
#include "usb/usb_device.hpp"
#include "wifi/wifi_ap.hpp"
#if CONFIG_AIRGREP_HWTEST
#include "hwtest.hpp"
#endif

#if AIRGREP_HAS_DISPLAY
#include "daidalus/scheduler.hpp"
#include "display/active_display_settings.hpp"
#endif

static const char* TAG = "airgrep";

using Completion = loop::Callable<void(esp_err_t)>;

// Boot runs two phases over the table. First every prepare: infrastructure
// starts and components subscribe to events, nothing publishes yet. Then every
// init: components start, and no subscriber misses an event.
struct Component {
	const char* name{};
	void (*init)(Completion complete){};
	esp_err_t (*prepare)() = nullptr;
};

static bool is_csbee(const adsbee::Message& m) {
	return m.kind == DECODER_CSBEE;
}

static void feed_traffic(const adsbee::Message& m, void*) {
	ESP_LOGV(TAG, "csbee %s", m.text);
	if (adsbee::State::copy().traffic_ingestion == adsbee::Ingestion::read) {
		traffic::ingest(m.text, m.len, m.received_us);
	}
}

static void start_adsbee(Completion complete) {
	esp_err_t result = traffic::start();
	if (result == ESP_OK &&
		!adsbee::subscribe(is_csbee, feed_traffic, nullptr)) {
		result = ESP_ERR_NO_MEM;
	}
	if (result == ESP_OK) {
		result = adsbee::start();
	}
	complete(result);
}

static void start_settings(Completion complete) {
	const esp_err_t result = settings::init();
#if AIRGREP_HAS_DISPLAY
	display_settings::load();
#endif
	complete(result);
}

static constexpr Component components[] = {
	{"Event loop", [](Completion complete) { complete(ESP_OK); },
	 [] {
		 loop::start();
		 return ESP_OK;
	 }},
	{"Settings", start_settings},
	{"Time zones", [](Completion complete) { complete(tz::init()); }},
	{"I2C", [](Completion complete) { complete(i2c_bus_init()); }},
	{"UART", [](Completion complete) { complete(adsbee::uart_init()); }},
	{"I2CExpander", expander::init},
	{"ADSbee", start_adsbee, adsbee::prepare},
	{"Clock", [](Completion complete) { complete(adsbee::timesync::start()); }},
	{"Wi-Fi", [](Completion complete) { complete(wifi::start()); },
	 [] { return wifi::prepare(device_info()->device_id); }},
	{"Charger", charger::init},
	{"Battery", battery::init},
	{"Baro", barometer::init, barometer::prepare},
	{"FUSB", fusb::init},
	{"Buzzer", [](Completion complete) { complete(buzzer_init()); },
	 buzzer_prepare},
#if AIRGREP_HAS_DISPLAY
	{"Conflicts", [](Completion complete) { complete(conflicts::start()); }},
#endif
	{"System", [](Completion complete) { complete(system_init()); },
	 system_prepare},
};

static bool in_init;

static void report(const Component& component, esp_err_t result,
				   int64_t started_us) {
	if (result == ESP_OK) {
		std::printf("%-*s OK", CONSOLE_LABEL_WIDTH, component.name);
	} else if (result == ESP_ERR_NOT_FOUND) {
		std::printf("%-*s OFF", CONSOLE_LABEL_WIDTH, component.name);
	} else {
		std::printf("%-*s FAIL %s", CONSOLE_LABEL_WIDTH, component.name,
					esp_err_to_name(result));
	}
	const int64_t blocked_us = esp_timer_get_time() - started_us;
	std::printf(" (%lld ms)", (long long)(blocked_us / 1000));
	std::puts("");
}

extern "C" void app_main(void) {
	ESP_ERROR_CHECK(usb::start());
	console_banner();
	esp_err_t prepared[std::size(components)];
	for (size_t i = 0; i < std::size(components); i++) {
		prepared[i] = components[i].prepare ? components[i].prepare() : ESP_OK;
	}
	for (size_t i = 0; i < std::size(components); i++) {
		const Component& component = components[i];
		const int64_t started_us = esp_timer_get_time();
		if (prepared[i] != ESP_OK) {
			report(component, prepared[i], started_us);
			continue;
		}
		in_init = true;
		component.init([&component, started_us](esp_err_t outcome) {
			report(component, outcome, started_us);
		});
		in_init = false;
	}
#if CONFIG_AIRGREP_HWTEST
	hwtest::run_boot_checks();
#endif
	ESP_ERROR_CHECK(console_start());
}
