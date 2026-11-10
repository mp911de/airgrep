#include "bq25798.hpp"

#include <new>

#include "esp_log.h"
#include "esp_timer.h"
#include "i2c_bus.h"
#include "sdkconfig.h"

namespace charger {
constexpr const char* tag = "charger";
// BQ25798 datasheet: I2C address 7.3.14, register map 7.5.1.
constexpr uint8_t kRegChargeCurrentLimit = 0x03;
constexpr uint8_t kRegInputCurrentLimit = 0x06;
constexpr uint8_t kRegChargerControl0 = 0x0f;
constexpr uint8_t kRegChargerControl1 = 0x10;
constexpr uint8_t kRegChargerControl5 = 0x14;
constexpr uint8_t kRegTemperatureControl = 0x16;
constexpr uint8_t kRegChargerStatus0 = 0x1b;
constexpr uint8_t kRegChargerStatus1 = 0x1c;
constexpr uint8_t kRegChargerStatus2 = 0x1d;
constexpr uint8_t kRegChargerStatus3 = 0x1e;
constexpr uint8_t kRegChargerStatus4 = 0x1f;
constexpr uint8_t kRegFaultStatus0 = 0x20;
constexpr uint8_t kRegFaultStatus1 = 0x21;
constexpr uint8_t kRegChargerFlag0 = 0x22;
constexpr uint8_t kRegFaultFlag0 = 0x26;
constexpr uint8_t kRegFaultFlag1 = 0x27;
constexpr uint8_t kRegAdcControl = 0x2e;
constexpr uint8_t kRegAdcFunctionDisable0 = 0x2f;
constexpr uint8_t kRegAdcFunctionDisable1 = 0x30;
constexpr uint8_t kRegIbusAdc = 0x31;
constexpr uint8_t kRegIbatAdc = 0x33;
constexpr uint8_t kRegVbusAdc = 0x35;
constexpr uint8_t kRegVbatAdc = 0x3b;
constexpr uint8_t kRegVsysAdc = 0x3d;
constexpr uint8_t kRegTsAdc = 0x3f;
constexpr uint8_t kRegTdieAdc = 0x41;
constexpr uint8_t kRegPartInformation = 0x48;

struct Block {
	uint8_t first, count;
};

constexpr Block kBlockLimits{kRegChargeCurrentLimit, 20};
constexpr Block kBlockAdcDisable{kRegAdcFunctionDisable0, 2};
constexpr Block kBlockControls{kRegChargerControl0, 8};
constexpr Block kBlockAdcControl{kRegAdcControl, 1};
constexpr Block kBlockStatus{kRegChargerStatus0, 8};
constexpr Block kBlockFaultFlags{kRegFaultFlag0, 2};
constexpr Block kBlockAdc{kRegIbusAdc, 18};
constexpr Block kBlockPartInformation{kRegPartInformation, 1};

constexpr uint8_t kPartBq25798 = 3;  // REG48 PN_2:0
constexpr uint8_t kWatchdog20s = 4;  // REG10 WATCHDOG_2:0
constexpr uint8_t kWatchdogMask = kWatchdogReset | kWatchdogFieldMask;
// ADC_EN, continuous conversion, ADC_SAMPLE 00 (15 bit, 24 ms per channel).
constexpr uint8_t kAdcProfile = kAdcEnable;

portMUX_TYPE State::mutex_ = portMUX_INITIALIZER_UNLOCKED;

State& State::storage() {
	static State state;
	return state;
}

bool State::is_valid() {
	const State state = storage();
	return state.updated_us && state.updated_us >= state.error_us;
}

State State::copy() {
	State result;
	update([&](State& state) { result = state; });
	return result;
}

i2c_master_dev_handle_t device;
loop::Handle periodic;
Completion completion;
State state;
bool busy;
unsigned step;

void record_error(esp_err_t result) {
	int64_t now = esp_timer_get_time();
	State::update([&](State& shared) {
		shared.last_error = result;
		shared.error_us = now;
	});
}

void error(esp_err_t result) {
	record_error(result);
	busy = false;
}

void publish_state() {
	State::update([](State& current) {
		// Detection belongs to explicit verification, not to the
		// periodic sample.
		state.battery_detected = current.battery_detected;
		state.battery_detection_vbat_mv = current.battery_detection_vbat_mv;
		state.battery_detection_vbat_valid =
			current.battery_detection_vbat_valid;
		state.last_error = current.last_error;
		state.error_us = current.error_us;
		current = state;
	});
}

void publish() {
	publish_state();
	busy = false;
}

void invalidate() {
	state.configured = state.adc_valid = false;
	State::update(
		[](State& shared) { shared.configured = shared.adc_valid = false; });
}

esp_err_t read(Block block) {
	return i2c_master_transmit_receive(device, &block.first, 1,
									   state.raw + block.first, block.count,
									   CONFIG_AIRGREP_I2C_TIMEOUT_MS);
}

esp_err_t write(uint8_t reg, uint8_t value) {
	const uint8_t bytes[] = {reg, value};
	return i2c_master_transmit(device, bytes, sizeof bytes,
							   CONFIG_AIRGREP_I2C_TIMEOUT_MS);
}

// Charge operations. One runs at a time on the loop, guarded by busy like the
// poll.
namespace {
constexpr uint8_t kChargeEnable = 1 << 5;  // REG0F EN_CHG
constexpr uint32_t kSettleMs = 1000;

struct Operation {
	ChargeCompletion complete;
	void (*observe)(const ChargeResult&) = nullptr;
	ChargeResult result;
	bool changed = false;  // EN_CHG was written, restore it before completing
} operation;

void publish_control() {
	State::update([](State& shared) {
		shared.raw[kRegChargerControl0] = state.raw[kRegChargerControl0];
		shared.control0 = decode_control0(state.raw[kRegChargerControl0]);
	});
}

esp_err_t read_enabled(bool& enabled) {
	esp_err_t result = read({kRegChargerControl0, 1});
	enabled = state.raw[kRegChargerControl0] & kChargeEnable;
	return result;
}

// The only writer of EN_CHG: read, modify, write, read back.
esp_err_t set_charge_enabled(bool enabled) {
	bool current;
	esp_err_t result = read_enabled(current);
	if (result != ESP_OK) {
		return result;
	}
	uint8_t control = state.raw[kRegChargerControl0] & ~kChargeEnable;
	if (enabled) {
		control |= kChargeEnable;
	}
	result = write(kRegChargerControl0, control);
	if (result != ESP_OK) {
		return result;
	}
	result = read_enabled(current);
	if (result == ESP_OK && current != enabled) {
		result = ESP_ERR_INVALID_RESPONSE;
	}
	if (result == ESP_OK) {
		publish_control();
	}
	return result;
}

void complete_operation() {
	const auto finished = operation;
	busy = false;
	if (finished.result.error != ESP_OK) {
		record_error(finished.result.error);
	}
	if (finished.result.restore_error != ESP_OK) {
		record_error(finished.result.restore_error);
	}
	if (finished.observe) {
		finished.observe(finished.result);
	}
	finished.complete(finished.result);
}

// Every exit of the battery test passes here, so restoration exists once.
void restore_and_complete() {
	auto& result = operation.result;
	if (operation.changed) {
		result.restore_error = set_charge_enabled(result.initial_enabled);
	}
	complete_operation();
}

void fail(esp_err_t error) {
	operation.result.error = error;
	restore_and_complete();
}

void apply_charge_enabled(bool enabled) {
	operation.result.error = set_charge_enabled(enabled);
	complete_operation();
}

// Battery test: VBAT while charging, 1 s with charging off, VBAT again,
// restore.
esp_err_t read_vbat(uint16_t& mv, bool& valid) {
	esp_err_t result = read({kRegVbatAdc, 2});
	mv = decode_adc_mv(state.raw[kRegVbatAdc], state.raw[kRegVbatAdc + 1]);
	valid = result == ESP_OK;
	return result;
}

esp_err_t adc_continuous() {
	esp_err_t result = read({kRegAdcControl, 3});
	if (result != ESP_OK) {
		return result;
	}
	bool enabled = state.raw[kRegAdcControl] & kAdcEnable;
	bool one_shot = state.raw[kRegAdcControl] & kAdcOneShot;
	bool vbat_disabled = state.raw[kRegAdcFunctionDisable0] & (1 << 4);
	return enabled && !one_shot && !vbat_disabled ? ESP_OK
												  : ESP_ERR_INVALID_STATE;
}

// A failed write may still have reached the device, so changed is set
// regardless.
esp_err_t switch_and_settle(bool enabled, void (*then)()) {
	esp_err_t result = set_charge_enabled(enabled);
	operation.changed = true;
	if (result != ESP_OK) {
		return result;
	}
	loop::Event settle{"charger settle", [then](loop::Handle, loop::Value&) {
						   then();
						   return loop::Result::done;
					   }};
	auto settled = loop::schedule(kSettleMs, settle, {},
								  [](loop::Outcome) { fail(ESP_ERR_NO_MEM); });
	return settled.valid() ? ESP_OK : ESP_ERR_NO_MEM;
}

void measure_disabled() {
	auto& result = operation.result;
	esp_err_t error = read_vbat(result.vbat_after, result.after_valid);
	if (error == ESP_OK) {
		error = read({kRegIbatAdc, 2});
	}
	if (error == ESP_OK) {
		result.ibat_after_ma =
			decode_ibat(state.raw[kRegIbatAdc], state.raw[kRegIbatAdc + 1]);
	}
	if (error != ESP_OK) {
		result.error = error;
	}
	restore_and_complete();
}

void measure_charging() {
	auto& result = operation.result;
	esp_err_t error = read_vbat(result.vbat_before, result.before_valid);
	if (error == ESP_OK) {
		error = switch_and_settle(false, measure_disabled);
	}
	if (error != ESP_OK) {
		fail(error);
	}
}

void start_battery_test() {
	auto& result = operation.result;
	esp_err_t error = read_enabled(result.initial_enabled);
	if (error == ESP_OK) {
		error = adc_continuous();
	}
	// Charging off needs one settle period with it on to get the charging
	// reading.
	if (error == ESP_OK && !result.initial_enabled) {
		error = switch_and_settle(true, measure_charging);
	} else if (error == ESP_OK) {
		measure_charging();
	}
	if (error != ESP_OK) {
		fail(error);
	}
}

void record_detection(const ChargeResult& result) {
	State::update([&](State& shared) {
		shared.battery_detection_vbat_mv =
			result.after_valid ? result.vbat_after : 0;
		shared.battery_detection_vbat_valid = result.after_valid;
		if (result.error == ESP_OK && result.restore_error == ESP_OK &&
			result.before_valid && result.after_valid) {
			shared.battery_detected =
				detect_battery(result.vbat_before, result.vbat_after);
		}
	});
}

void start_verify_battery() {
	operation.observe = record_detection;
	start_battery_test();
}

// Admission: hop onto the loop, run start when the driver is idle and the
// device known. A battery test also needs the configured continuous ADC.
esp_err_t submit(const char* name, bool needs_adc, ChargeCompletion complete,
				 void (*start)()) {
	if (!complete) {
		return ESP_ERR_INVALID_ARG;
	}
	struct Pending {
		ChargeCompletion complete;
		bool needs_adc;
		void (*start)();
	};
	auto* pending = new (std::nothrow) Pending{complete, needs_adc, start};
	if (!pending) {
		return ESP_ERR_NO_MEM;
	}
	bool queued = loop::hsubmit(name, [pending](loop::Handle, loop::Value&) {
		const auto admitted = *pending;
		delete pending;
		const auto current = State::copy();
		if (!device || !current.identity_known || busy ||
			(admitted.needs_adc &&
			 (!current.configured || !current.adc_valid))) {
			ChargeResult result;
			result.error = ESP_ERR_INVALID_STATE;
			record_error(result.error);
			admitted.complete(result);
			return loop::Result::done;
		}
		busy = true;
		state = current;
		operation = {};
		operation.complete = admitted.complete;
		admitted.start();
		return loop::Result::done;
	});
	if (!queued) {
		delete pending;
	}
	return queued ? ESP_OK : ESP_ERR_NO_MEM;
}
}  // namespace

esp_err_t set_charge_enabled(bool enabled, ChargeCompletion complete) {
	return submit(
		"charger enable", false, complete,
		enabled ? +[] { apply_charge_enabled(true); }
				: +[] { apply_charge_enabled(false); });
}

esp_err_t battery_test(ChargeCompletion complete) {
	return submit("charger battery test", true, complete, start_battery_test);
}

esp_err_t verify_battery(ChargeCompletion complete) {
	State::update([](State& shared) {
		shared.battery_detected = BatteryDetected::unknown;
		shared.battery_detection_vbat_mv = 0;
		shared.battery_detection_vbat_valid = false;
	});
	return submit("charger verify battery", true, complete,
				  start_verify_battery);
}

esp_err_t kick_watchdog() {
	uint8_t control1 = state.raw[kRegChargerControl1] & ~kWatchdogMask;
	return write(kRegChargerControl1, control1 | kWatchdogReset | kWatchdog20s);
}

int64_t conversion_cycle_us() {
	unsigned channels =
		decode_adc_disable0(state.raw[kRegAdcFunctionDisable0]) +
		decode_adc_disable1(state.raw[kRegAdcFunctionDisable1]);
	return int64_t(channels) * decode_adc_control(kAdcProfile).sample_ms * 1000;
}

void next(const char* name, loop::Action action) {
	if (!loop::hsubmit(name, action)) {
		error(ESP_ERR_NO_MEM);
	}
}

void decode_configuration() {
	const auto* raw = state.raw;
	state.charge_current_ma = decode_charge_current(
		raw[kRegChargeCurrentLimit], raw[kRegChargeCurrentLimit + 1]);
	state.input_current_ma = decode_input_current(
		raw[kRegInputCurrentLimit], raw[kRegInputCurrentLimit + 1]);
	state.control0 = decode_control0(raw[kRegChargerControl0]);
	state.control1 = decode_control1(raw[kRegChargerControl1]);
	state.control5 = decode_control5(raw[kRegChargerControl5]);
	state.thermal_limit_c =
		decode_temperature_control(raw[kRegTemperatureControl]);
	state.adc_control = decode_adc_control(raw[kRegAdcControl]);
}

void decode_status() {
	const auto* raw = state.raw;
	state.status0 = decode_status0(raw[kRegChargerStatus0]);
	state.status1 = decode_status1(raw[kRegChargerStatus1]);
	state.status2 = decode_status2(raw[kRegChargerStatus2]);
	state.status3 = decode_status3(raw[kRegChargerStatus3]);
	state.status4 = decode_status4(raw[kRegChargerStatus4]);
	state.fault0 = decode_fault0(raw[kRegFaultStatus0]);
	state.fault1 = decode_fault1(raw[kRegFaultStatus1]);
	state.flag0 = decode_flag0(raw[kRegChargerFlag0]);
	state.fault_flag0 = decode_fault_flag0(raw[kRegFaultFlag0]);
	state.fault_flag1 = decode_fault_flag1(raw[kRegFaultFlag1]);
	state.ibus_ma = decode_ibus(raw[kRegIbusAdc], raw[kRegIbusAdc + 1]);
	state.ibat_ma = decode_ibat(raw[kRegIbatAdc], raw[kRegIbatAdc + 1]);
	state.vbus_mv = decode_adc_mv(raw[kRegVbusAdc], raw[kRegVbusAdc + 1]);
	state.vbat_mv = decode_adc_mv(raw[kRegVbatAdc], raw[kRegVbatAdc + 1]);
	state.vsys_mv = decode_adc_mv(raw[kRegVsysAdc], raw[kRegVsysAdc + 1]);
	state.ts_percent = decode_ts(raw[kRegTsAdc], raw[kRegTsAdc + 1]);
	state.die_c = decode_tdie(raw[kRegTdieAdc], raw[kRegTdieAdc + 1]);
	state.updated_us = esp_timer_get_time();
	state.adc_valid =
		state.configured && state.updated_us >= state.adc_ready_us;
}

// A failed status read may hide an expiry that the kick would then clear.
void recover(esp_err_t result) {
	record_error(result);
	invalidate();
	next("charger watchdog", [](loop::Handle, loop::Value&) {
		esp_err_t kick = kick_watchdog();
		if (kick != ESP_OK) {
			record_error(kick);
		}
		busy = false;
		return loop::Result::done;
	});
}

loop::Result poll_configured(loop::Handle, loop::Value&) {
	esp_err_t result;
	switch (step++) {
		case 0:
			// A watchdog write clears WD_STAT. Observe it before the kick.
			result = read(kBlockStatus);
			if (result == ESP_OK &&
				decode_status0(state.raw[kRegChargerStatus0]).watchdog) {
				ESP_LOGW(tag,
						 "watchdog expired, reapplying "
						 "configuration on next poll");
				invalidate();
			}
			break;
		case 1:
			result = kick_watchdog();
			break;
		case 2:
			result = read(kBlockFaultFlags);
			break;
		default:
			result = read(kBlockAdc);
			break;
	}
	if (result != ESP_OK) {
		if (step == 1) {
			recover(result);
		} else {
			error(result);
		}
		return loop::Result::done;
	}
	if (step < 4) {
		next("charger sample", poll_configured);
		return loop::Result::done;
	}
	decode_status();
	publish();
	return loop::Result::done;
}

loop::Result configure(loop::Handle, loop::Value&) {
	esp_err_t result;
	switch (step++) {
		case 0:
			result = read(kBlockLimits);
			break;
		case 1:
			result = read(kBlockAdcDisable);
			break;
		case 2:
			result = kick_watchdog();
			break;
		// EN_IBAT keeps IBAT readable in battery-only mode. The ADC it feeds
		// already runs continuously, the data sheet lists no separate cost for
		// the sense path.
		case 3:
			result = write(kRegChargerControl5, state.raw[kRegChargerControl5] |
													kSfetPresent |
													kDischargeSensing);
			break;
		case 4:
			result = write(kRegAdcControl, kAdcProfile);
			if (result == ESP_OK) {
				state.adc_ready_us =
					esp_timer_get_time() + conversion_cycle_us();
			}
			break;
		case 5:
			result = read(kBlockControls);
			break;
		default:
			result = read(kBlockAdcControl);
			break;
	}
	if (result != ESP_OK) {
		error(result);
		return loop::Result::done;
	}
	if (step < 7) {
		next("charger configure", configure);
		return loop::Result::done;
	}
	decode_configuration();
	if (!state.control5.ship_fet || !state.control5.discharge_sensing ||
		state.control1.watchdog_ms != CHARGER_WATCHDOG_MS ||
		state.raw[kRegAdcControl] != kAdcProfile) {
		error(ESP_ERR_INVALID_RESPONSE);
		return loop::Result::done;
	}
	state.configured = true;
	state.adc_valid = false;
	publish_state();
	step = 0;
	next("charger polling", poll_configured);
	return loop::Result::done;
}

loop::Result poll(loop::Handle handle, loop::Value& value) {
	if (busy) {
		return loop::Result::done;
	}
	busy = true;
	state = State::copy();
	step = 0;
	return state.configured ? poll_configured(handle, value)
							: configure(handle, value);
}

void finish(esp_err_t result) {
	if (result != ESP_OK) {
		error(result);
	}
	if (completion) {
		completion(result);
	}
}

loop::Result identify(loop::Handle, loop::Value&) {
	esp_err_t result = read(kBlockPartInformation);
	uint8_t raw = state.raw[kRegPartInformation];
	Identity identity = decode_identity(raw);
	if (result == ESP_OK && identity.part != kPartBq25798) {
		result = ESP_ERR_INVALID_RESPONSE;
	}
	if (result == ESP_OK) {
		State::update([&](State& shared) {
			shared.status = Status::ok;
			shared.identity_known = true;
			shared.raw[kRegPartInformation] = raw;
			shared.identity = identity;
		});
		periodic = loop::schedule_periodic(CHARGER_POLL_PERIOD_MS,
										   {"charger poll", poll});
		if (!periodic.valid()) {
			result = ESP_ERR_NO_MEM;
		}
	}
	finish(result);
	return loop::Result::done;
}

loop::Result probe(loop::Handle, loop::Value&) {
	esp_err_t result = i2c_bus_add_if_present(&i2c_config, &device);
	if (result == ESP_ERR_NOT_FOUND) {
		State::update([](State& shared) { shared.status = Status::off; });
	}
	if (result == ESP_OK && !loop::hsubmit("charger identity", identify)) {
		result = ESP_ERR_NO_MEM;
	}
	if (result != ESP_OK) {
		finish(result);
	}
	return loop::Result::done;
}

loop::Handle poll_handle() { return periodic; }

void init(Completion complete) {
	completion = complete;
	if (!loop::hsubmit("charger probe", probe)) {
		finish(ESP_ERR_NO_MEM);
	}
}
}  // namespace charger
