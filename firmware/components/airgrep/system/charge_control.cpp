#include "charge_control.hpp"

#include <cmath>
#include <mutex>

#include "charge_rules.hpp"
#include "charger/bq25798.hpp"
#include "esp_timer.h"
#include "events/events.hpp"
#include "loop.hpp"
#include "settings/settings.hpp"
#include "usb/fusb303.hpp"

namespace charge_control {
namespace {
constexpr uint32_t kPeriodicRunMs = 2000;
std::mutex control_mutex;
charger::ControlMode control = charger::ControlMode::automatic;

ChargeInputs read_charge_inputs(const charger::State& charger_state,
								const fusb::State& usb_c) {
	const int64_t now = esp_timer_get_time();
	ChargeInputs in;
	in.now_us = now;
	in.mode = control;
	in.charger_usable = charger_state.usable(now);
	in.vbus = charger_state.readings.status0.vbus;
	in.detecting = charger_state.detecting();
	in.detected_ma = charger_state.detected_ma();
	in.detected_vbus_type = charger_state.readings.status1.vbus;
	in.pack_c = in.charger_usable ? charger_state.pack_celsius() : NAN;
	in.vbat_mv = charger_state.adc_valid ? charger_state.readings.vbat_mv : 0;
	in.battery_overvoltage =
		charger_state.readings.fault0.battery_overvoltage ||
		charger_state.readings.fault_flag0.battery_overvoltage;
	in.usb_c_usable = usb_c.usable(now);
	in.usb_c_negotiating = usb_c.negotiating();
	in.advertised_ma = usb_c.advertised_ma();
	in.capacity_mah = settings::copy().battery_mah;
	return in;
}

void publish_events(const charger::TargetState& target) {
	static events::Supply supply;
	static events::ChargeLimit limit;
	const events::Supply supply_now{target.supply, uint16_t(target.input_ma)};
	if (events::update_if_changed(supply, supply_now)) {
		events::publish(events::Event::supply_changed(supply));
	}
	const events::ChargeLimit limit_now{target.stop,
										uint16_t(target.charge_ma)};
	if (events::update_if_changed(limit, limit_now)) {
		events::publish(events::Event::charge_limit_changed(limit));
	}
}

void run_charge_control(const charger::State& charger_state,
						const fusb::State& usb_c) {
	charger::TargetState target;
	{
		const std::scoped_lock lock(control_mutex);
		if (control == charger::ControlMode::manual) {
			return;
		}
		static Rules rules;
		target = rules.apply_rules(read_charge_inputs(charger_state, usb_c));
		charger::set_charger_target(target);
	}
	publish_events(target);
}

void charger_polled(const charger::State& state, void*) {
	run_charge_control(state, fusb::State::copy());
}

void usb_c_polled(const fusb::State& state, void*) {
	run_charge_control(charger::State::copy(), state);
}
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

charger::ControlMode get_charger_mode() {
	const std::scoped_lock lock(control_mutex);
	return control;
}

esp_err_t set_charger_mode(charger::ControlMode mode) {
	const std::scoped_lock lock(control_mutex);
	if (charger::battery_test_running()) {
		return ESP_ERR_INVALID_STATE;
	}
	if (control != mode) {
		control = mode;
		charger::State::update(
			[mode](charger::State& state) { state.target = {.mode = mode}; });
	}
	return ESP_OK;
}

const char* get_charger_mode_name(charger::ControlMode mode) {
	return mode == charger::ControlMode::manual ? "MANUAL" : "AUTO";
}

}
