#include "system.h"

#include <atomic>

#include "buzzer/buzzer.h"
#include "charger/bq25798.hpp"
#include "esp_log.h"
#include "esp_pm.h"
#include "i2c_expander/tca9534.hpp"
#include "sdkconfig.h"

namespace {

constexpr int kCpuMhz = 80;
constexpr bool kChargerLightSleep = false;

std::atomic mode{POWER_MODE_UNKNOWN};

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
	power_mode_t to =
		power_mode_decode({valid, controls.input.power},
						  {charger.is_valid(), charger.status0.vbus});
	if (to != from) {
		mode = to;
		transition(from, to);
	}
}
}  // namespace

esp_err_t system_init() {
	return expander::subscribe(apply_system_controls) ? ESP_OK : ESP_ERR_NO_MEM;
}

power_mode_t system_mode() { return mode; }
