#include "charge_control.hpp"

#include <atomic>
#include <cmath>

#include "charge_rules.hpp"
#include "charger/bq25798.hpp"
#include "esp_timer.h"
#include "loop.hpp"
#include "settings/settings.hpp"
#include "usb/fusb303.hpp"

namespace charge_control {
namespace {
constexpr uint32_t kPeriodicRunMs = 2000;
}  // namespace

std::atomic control{charger::ControlMode::automatic};
Rules rules;  // loop task only

ChargeInputs read_charge_inputs(const charger::State& charger_state,
								const fusb::State& usb_c) {
	const int64_t now = esp_timer_get_time();
	ChargeInputs in;
	in.now_us = now;
	in.mode = control;
	in.charger_usable = charger_state.usable(now);
	in.vbus = charger_state.status0.vbus;
	in.detecting = charger_state.detecting();
	in.detected_ma = charger_state.detected_ma();
	in.vbus_type = charger_state.status1.vbus;
	in.pack_c = in.charger_usable ? charger_state.pack_celsius() : NAN;
	in.vbat_mv = charger_state.adc_valid ? charger_state.vbat_mv : 0;
	in.battery_overvoltage = charger_state.fault0.battery_overvoltage ||
							 charger_state.fault_flag0.battery_overvoltage;
	in.usb_c_usable = usb_c.usable(now);
	in.usb_c_negotiating = usb_c.negotiating();
	in.advertised_ma = usb_c.advertised_ma();
	in.capacity_mah = settings::copy().battery_mah;
	return in;
}

void run_charge_control(const charger::State& charger_state,
						const fusb::State& usb_c) {
	ChargeInputs in = read_charge_inputs(charger_state, usb_c);
	if (in.mode == charger::ControlMode::automatic) {
		const charger::TargetState target = rules.apply_rules(in);
		charger::set_charger_target(target);
	}
}

void charger_polled(const charger::State& state, void*) {
	run_charge_control(state, fusb::State::copy());
}

void usb_c_polled(const fusb::State& state, void*) {
	run_charge_control(charger::State::copy(), state);
}

esp_err_t init() {
	if (!charger::subscribe(charger_polled) || !fusb::subscribe(usb_c_polled)) {
		return ESP_ERR_NO_MEM;
	}
	auto periodic = loop::schedule_periodic(
		kPeriodicRunMs, {"charge control", [](loop::Handle, loop::Value&) {
							 run_charge_control(charger::State::copy(),
												fusb::State::copy());
							 return loop::Result::done;
						 }});
	return periodic.valid() ? ESP_OK : ESP_ERR_NO_MEM;
}

charger::ControlMode get_charger_mode() { return control; }

esp_err_t set_charger_mode(charger::ControlMode mode) {
	if (charger::battery_test_running()) {
		return ESP_ERR_INVALID_STATE;
	}
	control = mode;
	return ESP_OK;
}

const char* get_charger_mode_name(charger::ControlMode mode) {
	return mode == charger::ControlMode::manual ? "MANUAL" : "AUTO";
}

}  // namespace charge_control
