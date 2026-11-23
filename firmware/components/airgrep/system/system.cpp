#include "system.h"

#include <atomic>

#include "adsbee/adsbee_link.hpp"
#include "baro/barometer.hpp"
#include "battery/max17048.hpp"
#include "board.h"
#include "charge_control.hpp"
#include "charger/bq25798.hpp"
#include "cpu_sampler.hpp"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "events/events.hpp"
#include "health.hpp"
#include "i2c_expander/tca9534.hpp"
#include "loop.hpp"
#include "usb/fusb303.hpp"

namespace {

constexpr int kCpuMhz = 80;
constexpr bool kChargerLightSleep = false;
constexpr uint32_t kHealthPeriodMs = 2000;
constexpr unsigned kCpuLimitPercent = 50;
constexpr uint32_t kWatchdogPeriodMs = 10000;

std::atomic mode{POWER_MODE_UNKNOWN};
std::atomic<unsigned> cpu_percent{0};
bool check_pending;

unsigned busiest_core_load_since_last_sample() {
#if CONFIG_AIRGREP_CPU_METRICS
	static cpu::Sampler window;
	return cpu::sample(window, esp_timer_get_time()).busiest;
#else
	return 0;
#endif
}

health::HealthInputs health_snapshot() {
	health::HealthInputs s;
	s.now_us = esp_timer_get_time();
	const expander::State controls = expander::State::copy();
	s.last_update.expander_us = controls.updated_us;
	s.expander_ready = is_ready(controls);
	const charger::State charger_state = charger::State::copy();
	s.last_update.charger_us = charger_state.updated_us;
	s.charger_ready = is_ready(charger_state);
	s.fault0 = charger_state.readings.fault0;
	s.fault1 = charger_state.readings.fault1;
	s.target_decided = charger_state.target.target_available;
	s.stop = charger_state.target.stop;
	s.battery_absent = charger_state.battery() == BatteryDetected::absent;
	const battery::State gauge = battery::State::copy();
	s.last_update.gauge_us = gauge.updated_us;
	s.gauge_ready = is_ready(gauge);
	s.battery_low = gauge.low;
	const fusb::State usb_c = fusb::State::copy();
	s.last_update.usb_c_us = usb_c.updated_us;
	s.usb_c_ready = is_ready(usb_c);
	const barometer::State baro = barometer::State::copy();
	s.last_update.barometer_us = baro.updated_us;
	s.barometer_ready = is_ready(baro);
	const adsbee::State link = adsbee::State::copy();
	s.last_update.adsbee_us = link.updated_us;
	s.adsbee_ready = adsbee::accepts_requests();
	s.last_update.gnss_us = link.last_gnss_report_us;
	s.gnss_bbr = link.gnss.bbr;
	s.gnss_antenna = link.gnss.antenna;
	const uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
				   psram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
	s.internal_free = heap_caps_get_free_size(internal);
	s.internal_total = heap_caps_get_total_size(internal);
	s.psram_free = heap_caps_get_free_size(psram);
	s.psram_total = heap_caps_get_total_size(psram);
	s.cpu_high = cpu_percent > kCpuLimitPercent;
	return s;
}

void check_health() {
	static uint16_t last;
	const uint16_t before = last, mask = system_health();
	if (BOARD_PIN_USR_LED >= 0) {
		gpio_set_level(gpio_num_t(BOARD_PIN_USR_LED), mask ? 0 : 1);
	}
	char text[256];
	health::format(mask, text, sizeof text);
	ESP_LOGD("system", "health %s", text);
	if (events::update_if_changed(last, mask)) {
		events::publish(events::Event::health_changed({before, mask}));
	}
}

// Starts every driver again that stopped on a failure or never showed up. A
// driver that polls ignores the call.
esp_err_t start_watchdog() {
	auto periodic = loop::schedule_periodic(
		kWatchdogPeriodMs, {"system watchdog", [](loop::Handle, loop::Value&) {
								expander::init({});
								charger::init({});
								battery::init({});
								barometer::init({});
								fusb::init({});
								return loop::Result::done;
							}});
	return periodic.valid() ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t start_health_task() {
	if (BOARD_PIN_USR_LED >= 0) {
		const gpio_num_t led = gpio_num_t(BOARD_PIN_USR_LED);
		gpio_set_level(led, 1);
		esp_err_t err = gpio_set_direction(led, GPIO_MODE_OUTPUT);
		if (err == ESP_OK) {
			err = gpio_sleep_sel_dis(led);
		}
		if (err != ESP_OK) {
			return err;
		}
	}
	auto periodic = loop::schedule_periodic(
		kHealthPeriodMs, {"health", [](loop::Handle, loop::Value&) {
							  cpu_percent =
								  busiest_core_load_since_last_sample();
							  check_health();
							  return loop::Result::done;
						  }});
	return periodic.valid() ? ESP_OK : ESP_ERR_NO_MEM;
}

void scale_clock(const bool charger) {
	esp_pm_config_t config = {
		.max_freq_mhz = kCpuMhz,
		.min_freq_mhz = kCpuMhz,
		.light_sleep_enable = charger && kChargerLightSleep,
	};
	const esp_err_t result = esp_pm_configure(&config);
	if (result != ESP_OK) {
		ESP_LOGE("system", "clock scaling: %s", esp_err_to_name(result));
	}
}

void transition(const power_mode_t from, const power_mode_t to) {
	ESP_LOGI("system", "mode %s to %s", power_mode_name(from),
			 power_mode_name(to));
	events::publish(events::Event::mode_changed({from, to}));
}

void apply_system_controls(const expander::State& controls, uint8_t, void*) {
	const bool valid = controls.is_valid();
	const charger::State charger = charger::State::copy();
	const power_mode_t from = mode;
	const power_mode_t to = power_mode_decode(
		{valid, controls.input.power},
		{charger.usable(esp_timer_get_time()), charger.readings.status0.vbus});
	if (to != from) {
		mode = to;
		transition(from, to);
		check_pending = true;
	}
	if (check_pending && health::ready(to, health_snapshot())) {
		check_pending = false;
		check_health();
	}
}
}

esp_err_t system_prepare() {
	return events::subscribe(
			   events::mask_for(events::Type::mode_changed),
			   [](const events::Event& event, void*) {
				   scale_clock(event.data.mode.to == POWER_MODE_CHARGER);
			   })
			   ? ESP_OK
			   : ESP_ERR_NO_MEM;
}

esp_err_t system_init() {
	esp_err_t err = charge_control::init();
	if (err == ESP_OK) {
		err = start_watchdog();
	}
	if (err == ESP_OK) {
		err = start_health_task();
	}
	if (err == ESP_OK && !expander::subscribe(apply_system_controls)) {
		err = ESP_ERR_NO_MEM;
	}
	return err;
}

power_mode_t system_mode() { return mode; }

uint16_t system_health() {
	return health::evaluate_health(mode, health_snapshot());
}

unsigned system_cpu_percent() { return cpu_percent; }
