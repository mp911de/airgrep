#include "bq25798.hpp"
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

	constexpr uint8_t kPartBq25798 = 3; // REG48 PN_2:0
	constexpr uint8_t kWatchdog20s = 4; // REG10 WATCHDOG_2:0
	constexpr uint8_t kWatchdogMask = kWatchdogReset | kWatchdogFieldMask;
	// ADC_EN, continuous conversion, ADC_SAMPLE 00 (15 bit, 24 ms per channel).
	constexpr uint8_t kAdcProfile = kAdcEnable;

	portMUX_TYPE State::mutex_ = portMUX_INITIALIZER_UNLOCKED;

	State& State::storage() {
		static State state;
		return state;
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

	void publish() {
		State::update([](State& current) { state = current; });
		busy = false;
	}

	void invalidate() {
		state.configured = state.adc_valid = false;
		State::update([](State& shared) {
			shared.configured = shared.adc_valid = false;
		});
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

	esp_err_t kick_watchdog() {
		uint8_t control1 = state.raw[kRegChargerControl1] & ~kWatchdogMask;
		return write(kRegChargerControl1, control1 | kWatchdogReset | kWatchdog20s);
	}

	int64_t conversion_cycle_us() {
		unsigned channels = decode_adc_disable0(state.raw[kRegAdcFunctionDisable0]) +
			decode_adc_disable1(state.raw[kRegAdcFunctionDisable1]);
		return int64_t(channels) * decode_adc_control(kAdcProfile).sample_ms * 1000;
	}

	void next(const char* name, loop::Action action) {
		if (!loop::hsubmit(name, action)) error(ESP_ERR_NO_MEM);
	}

	void decode_configuration() {
		const auto* raw = state.raw;
		state.charge_current_ma =
			decode_charge_current(raw[kRegChargeCurrentLimit],
								raw[kRegChargeCurrentLimit + 1]);
		state.input_current_ma =
			decode_input_current(raw[kRegInputCurrentLimit],
								raw[kRegInputCurrentLimit + 1]);
		state.control0 = decode_control0(raw[kRegChargerControl0]);
		state.control1 = decode_control1(raw[kRegChargerControl1]);
		state.control5 = decode_control5(raw[kRegChargerControl5]);
		state.thermal_limit_c = decode_temperature_control(raw[kRegTemperatureControl]);
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
		state.adc_valid = state.configured && state.updated_us >= state.adc_ready_us;
	}

	// A failed status read may hide an expiry that the kick would then clear.
	void recover(esp_err_t result) {
		record_error(result);
		invalidate();
		next("charger watchdog", [](loop::Handle, loop::Value&) {
			esp_err_t kick = kick_watchdog();
			if (kick != ESP_OK) record_error(kick);
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
			if (result == ESP_OK && decode_status0(state.raw[kRegChargerStatus0]).
				watchdog) {
				ESP_LOGW(tag, "watchdog expired, reapplying configuration on next poll");
				invalidate();
			}
			break;
		case 1: result = kick_watchdog();
			break;
		case 2: result = read(kBlockFaultFlags);
			break;
		default: result = read(kBlockAdc);
			break;
		}
		if (result != ESP_OK) {
			if (step == 1) recover(result);
			else error(result);
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
		case 0: result = read(kBlockLimits);
			break;
		case 1: result = read(kBlockAdcDisable);
			break;
		case 2: result = kick_watchdog();
			break;
		case 3: result = write(kRegChargerControl5,
								state.raw[kRegChargerControl5] | kSfetPresent);
			break;
		case 4:
			result = write(kRegAdcControl, kAdcProfile);
			if (result == ESP_OK)
				state.adc_ready_us = esp_timer_get_time() + conversion_cycle_us();
			break;
		case 5: result = read(kBlockControls);
			break;
		default: result = read(kBlockAdcControl);
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
		if (!state.control5.ship_fet || state.control1.watchdog_ms != CHARGER_WATCHDOG_MS ||
			state.raw[kRegAdcControl] != kAdcProfile) {
			error(ESP_ERR_INVALID_RESPONSE);
			return loop::Result::done;
		}
		state.configured = true;
		state.adc_valid = false;
		State::update([](State& current) { state = current; });
		step = 0;
		next("charger polling", poll_configured);
		return loop::Result::done;
	}

	loop::Result poll(loop::Handle handle, loop::Value& value) {
		if (busy) return loop::Result::done;
		busy = true;
		state = State::copy();
		step = 0;
		return state.configured ? poll_configured(handle, value) : configure(handle, value);
	}

	void finish(esp_err_t result) {
		if (result != ESP_OK) error(result);
		if (completion) completion(result);
	}

	loop::Result identify(loop::Handle, loop::Value&) {
		esp_err_t result = read(kBlockPartInformation);
		uint8_t raw = state.raw[kRegPartInformation];
		Identity identity = decode_identity(raw);
		if (result == ESP_OK && identity.part != kPartBq25798)
			result = ESP_ERR_INVALID_RESPONSE;
		if (result == ESP_OK) {
			State::update([&](State& shared) {
				shared.status = Status::ok;
				shared.identity_known = true;
				shared.raw[kRegPartInformation] = raw;
				shared.identity = identity;
			});
			periodic = loop::schedule_periodic(CHARGER_POLL_PERIOD_MS, {"charger poll", poll});
			if (!periodic.valid()) result = ESP_ERR_NO_MEM;
		}
		finish(result);
		return loop::Result::done;
	}

	loop::Result probe(loop::Handle, loop::Value&) {
		esp_err_t result = i2c_bus_add_if_present(&i2c_config, &device);
		if (result == ESP_ERR_NOT_FOUND) State::update([](State& shared) { shared.status = Status::off; });
		if (result == ESP_OK && !loop::hsubmit("charger identity", identify)) result = ESP_ERR_NO_MEM;
		if (result != ESP_OK) finish(result);
		return loop::Result::done;
	}

	loop::Handle poll_handle() { return periodic; }

	void init(Completion complete) {
		completion = complete;
		if (!loop::hsubmit("charger probe", probe)) finish(ESP_ERR_NO_MEM);
	}
}
