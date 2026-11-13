#include "system.h"

#include <atomic>

#include "adsbee/adsbee_link.hpp"
#include "baro/barometer.hpp"
#include "battery/max17048.hpp"
#include "board.h"
#include "buzzer/buzzer.h"
#include "charge_control.hpp"
#include "charger/bq25798.hpp"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
std::atomic<bool> cpu_high{false};
// A mode change wants one health check as soon as the mode is ready.
bool check_pending;

// Average load of all cores since the previous call.
unsigned cpu_load_percent() {
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
	static uint64_t idle_at[CONFIG_FREERTOS_NUMBER_OF_CORES], time_at;
	const uint64_t now = esp_timer_get_time(), elapsed = now - time_at;
	uint64_t busy = 0;
	for (int core = 0; core < CONFIG_FREERTOS_NUMBER_OF_CORES; core++) {
		const uint64_t idle = ulTaskGetIdleRunTimeCounterForCore(core);
		const uint64_t idle_delta = idle - idle_at[core];
		busy += idle_delta < elapsed ? elapsed - idle_delta : 0;
		idle_at[core] = idle;
	}
	time_at = now;
	return elapsed ? busy * 100 / (elapsed * CONFIG_FREERTOS_NUMBER_OF_CORES)
				   : 0;
#else
	return 0;
#endif
}

health::HealthInputs health_snapshot() {
	health::HealthInputs s;
	s.now_us = esp_timer_get_time();
	const expander::State controls = expander::State::copy();
	s.expander_us = controls.updated_us;
	s.expander_ready = is_ready(controls);
	const charger::State charger_state = charger::State::copy();
	s.charger_us = charger_state.updated_us;
	s.charger_ready = is_ready(charger_state);
	s.fault0 = charger_state.fault0;
	s.fault1 = charger_state.fault1;
	s.target_decided = charger_state.target.decided;
	s.stop = charger_state.target.stop;
	const battery::State gauge = battery::State::copy();
	s.gauge_us = gauge.updated_us;
	s.gauge_ready = is_ready(gauge);
	s.soc = gauge.soc;
	const fusb::State usb_c = fusb::State::copy();
	s.usb_c_us = usb_c.updated_us;
	s.usb_c_ready = is_ready(usb_c);
	const barometer::State baro = barometer::State::copy();
	s.barometer_us = baro.updated_us;
	s.barometer_ready = is_ready(baro);
	const adsbee::State link = adsbee::State::copy();
	s.adsbee_us = link.updated_us;
	s.adsbee_ready = adsbee::ready();
	s.gnss_us = link.gnss_us;
	s.gnss_fix = link.gnss.fix;
	s.gnss_utc = link.gnss.utc_known;
	s.gnss_bbr = link.gnss.bbr;
	const uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
				   psram = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
	s.internal_free = heap_caps_get_free_size(internal);
	s.internal_total = heap_caps_get_total_size(internal);
	s.psram_free = heap_caps_get_free_size(psram);
	s.psram_total = heap_caps_get_total_size(psram);
	s.cpu_high = cpu_high;
	return s;
}

void check_health() {
	const uint16_t mask = system_health();
	gpio_set_level(gpio_num_t(BOARD_PIN_USR_LED), mask ? 0 : 1);
	char text[96];
	health::format(mask, text, sizeof text);
	ESP_LOGD("system", "health %s", text);
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
	if (BOARD_PIN_USR_LED < 0) {
		return ESP_OK;
	}
	const gpio_num_t led = gpio_num_t(BOARD_PIN_USR_LED);
	gpio_set_level(led, 1);
	esp_err_t err = gpio_set_direction(led, GPIO_MODE_OUTPUT);
	if (err == ESP_OK) {
		err = gpio_sleep_sel_dis(led);
	}
	if (err != ESP_OK) {
		return err;
	}
	auto periodic = loop::schedule_periodic(
		kHealthPeriodMs, {"health", [](loop::Handle, loop::Value&) {
							  cpu_high = cpu_load_percent() > kCpuLimitPercent;
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
	esp_err_t result = esp_pm_configure(&config);
	if (result != ESP_OK) {
		ESP_LOGE("system", "clock scaling: %s", esp_err_to_name(result));
	}
}

void transition(const power_mode_t from, const power_mode_t to) {
	ESP_LOGI("system", "mode %s to %s", power_mode_name(from),
			 power_mode_name(to));
	scale_clock(to == POWER_MODE_CHARGER);
	barometer::set_enabled(to == POWER_MODE_ON);
	if (const char* chime = power_mode_chime(from, to)) {
		buzzer_play(chime, 0);
	}
}

void apply_system_controls(const expander::State& controls, uint8_t, void*) {
	bool valid = controls.is_valid();
	if (valid) {
		buzzer_set_mute(controls.input.mute);
	}
	charger::State charger = charger::State::copy();
	power_mode_t from = mode;
	power_mode_t to = power_mode_decode(
		{valid, controls.input.power},
		{charger.usable(esp_timer_get_time()), charger.status0.vbus});
	if (to != from) {
		mode = to;
		transition(from, to);
		check_pending = BOARD_PIN_USR_LED >= 0;
	}
	if (check_pending && health::ready(to, health_snapshot())) {
		check_pending = false;
		check_health();
	}
}
}  // namespace

esp_err_t system_init() {
	esp_err_t err = charge_control::init();
	if (err == ESP_OK) {
		err = start_watchdog();
	}
	if (err == ESP_OK) {
		err = start_health_task();
	}
	// The expander polls already, so its handler runs from here on.
	if (err == ESP_OK && !expander::subscribe(apply_system_controls)) {
		err = ESP_ERR_NO_MEM;
	}
	return err;
}

power_mode_t system_mode() { return mode; }

uint16_t system_health() {
	return health::evaluate_health(mode, health_snapshot());
}