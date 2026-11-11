#include "barometer.hpp"

#include <new>

#include "esp_log.h"
#include "esp_timer.h"
#include "i2c_bus.h"
#include "sdkconfig.h"

namespace barometer {
constexpr const char* tag = "baro";
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

namespace {
i2c_master_dev_handle_t device;
loop::Handle periodic;
State state;
bool busy;
bool enabled;
unsigned step, generation;
int64_t ready_us;

void error(esp_err_t result) {
	int64_t now = esp_timer_get_time();
	State::update([&](State& shared) {
		shared.last_error = result;
		shared.error_us = now;
		shared.configured = false;
	});
	busy = false;
}

esp_err_t read(uint8_t reg, uint8_t* bytes, size_t length = 1) {
	return i2c_master_transmit_receive(device, &reg, 1, bytes, length,
									   CONFIG_AIRGREP_I2C_TIMEOUT_MS);
}

esp_err_t write(uint8_t reg, uint8_t value) {
	const uint8_t bytes[] = {reg, value};
	return i2c_master_transmit(device, bytes, sizeof bytes,
							   CONFIG_AIRGREP_I2C_TIMEOUT_MS);
}

void next(const char* name, loop::Result (*action)(loop::Handle, loop::Value&),
		  uint32_t delay = 0) {
	unsigned current = generation;
	loop::Event event{
		name, [current, action](loop::Handle handle, loop::Value& value) {
			if (current != generation) {
				return loop::Result::done;
			}
			return action(handle, value);
		}};
	loop::Handle handle =
		delay ? loop::schedule(delay, event) : loop::submit(event);
	if (!handle.valid()) {
		error(ESP_ERR_NO_MEM);
	}
}

// NVM status only means something in standby, so the live poll reads data
// alone.
loop::Result live(loop::Handle, loop::Value&) {
	esp_err_t result = read(kRegTempDataXlsb, state.data, sizeof state.data);
	if (result != ESP_OK) {
		error(result);
		return loop::Result::done;
	}
	state.temperature_c =
		decode_temperature(state.data[kDataTemperatureOffset],
						   state.data[kDataTemperatureOffset + 1],
						   state.data[kDataTemperatureOffset + 2]);
	state.pressure_hpa = decode_pressure(state.data[kDataPressureOffset],
										 state.data[kDataPressureOffset + 1],
										 state.data[kDataPressureOffset + 2]);
	state.altitude_m = pressure_altitude(state.pressure_hpa);
	state.updated_us = esp_timer_get_time();
	State::update([](State& shared) { shared = state; });
	busy = false;
	return loop::Result::done;
}

loop::Result configure(loop::Handle, loop::Value&) {
	esp_err_t result;
	switch (step++) {
		// Deep standby powers the NVM down, so leave it before checking
		// nvm_rdy.
		case 0:
			result = write(kRegOdrConfig, kOdrConfigStandby | kOdrDeepDisable);
			break;
		case 1:
			result = read(kRegStatus, &state.raw_status);
			if (result == ESP_OK) {
				state.nvm = decode_status(state.raw_status);
				if (!state.nvm.ready || state.nvm.error ||
					state.nvm.command_error) {
					ESP_LOGW(tag, "STATUS 0x%02X: NVM not ready or in error",
							 state.raw_status);
					result = ESP_ERR_INVALID_RESPONSE;
				}
			}
			break;
		case 2:
			result = read(kRegIntConfig, &state.raw_int);
			break;
		case 3:
			result = write(kRegFifoSel, 0);
			break;
		case 4:
			result = write(kRegDspIir, 0);
			break;
		case 5:
			result = write(kRegIntConfig,
						   (state.raw_int | kIntEnable) & ~kIntOpenDrain);
			break;
		case 6: {
			const uint8_t bytes[] = {
				kRegOsrConfig,
				uint8_t(kOsrConfig | (enabled ? kOsrPressureEnable : 0)),
				enabled ? kOdrConfigNormal : kOdrConfigStandby};
			result = i2c_master_transmit(device, bytes, sizeof bytes,
										 CONFIG_AIRGREP_I2C_TIMEOUT_MS);
			break;
		}
		case 7: {
			uint8_t config[kRegFifoSel - kRegIntConfig + 1];
			result = read(kRegIntConfig, config, sizeof config);
			if (result == ESP_OK) {
				state.raw_int = config[0];
				state.raw_fifo = config[kRegFifoSel - kRegIntConfig];
			}
			break;
		}
		case 8: {
			uint8_t config[kRegOsrEff - kRegOsrConfig + 1];
			result = read(kRegOsrConfig, config, sizeof config);
			if (result == ESP_OK) {
				state.raw_osr = config[0];
				state.raw_odr = config[kRegOdrConfig - kRegOsrConfig];
				state.raw_osr_eff = config[kRegOsrEff - kRegOsrConfig];
			}
			break;
		}
		default:
			result = read(kRegDspIir, &state.raw_iir);
			break;
	}
	if (result != ESP_OK) {
		error(result);
		return loop::Result::done;
	}
	if (step < 10) {
		// Mode changes need tstandby before configuration writes.
		next("baro configure", configure,
			 step == 1 || step == 7 ? BMP_STARTUP_MS : 0);
		return loop::Result::done;
	}
	state.osr = decode_osr(state.raw_osr);
	state.odr = decode_odr(state.raw_odr);
	state.iir = decode_iir(state.raw_iir);
	const uint8_t wanted_odr = enabled ? kOdrConfigNormal : kOdrConfigStandby;
	const uint8_t wanted_osr = kOsrConfig | (enabled ? kOsrPressureEnable : 0);
	if (state.raw_osr != wanted_osr || state.raw_odr != wanted_odr ||
		state.raw_iir != 0 || state.raw_fifo != 0 ||
		(enabled && state.raw_osr_eff != (kOsrEffectiveValid | kOsrConfig)) ||
		(state.raw_int & (kIntEnable | kIntOpenDrain)) != kIntEnable) {
		ESP_LOGW(tag,
				 "readback INT 0x%02X OSR 0x%02X ODR 0x%02X, wanted ODR 0x%02X",
				 state.raw_int, state.raw_osr, state.raw_odr, wanted_odr);
		error(ESP_ERR_INVALID_RESPONSE);
		return loop::Result::done;
	}
	state.configured = true;
	state.status = enabled ? Status::ok : Status::off;
	state.last_error = ESP_OK;
	State::update([](State& shared) { shared = state; });
	// Do not publish old data while the first conversion is pending.
	ready_us = esp_timer_get_time() + BMP_POLL_PERIOD_MS * 1000;
	busy = false;
	return loop::Result::done;
}

loop::Result standby(loop::Handle, loop::Value&) {
	esp_err_t result = write(kRegOdrConfig, kOdrConfigStandby);
	if (result != ESP_OK) {
		error(result);
	} else {
		// Stop conversions before changing standby-only fields such as
		// deep_dis.
		next("baro standby", configure, BMP_STANDBY_MS);
	}
	return loop::Result::done;
}

// Reads CHIP_ID into the working copy and accepts only a BMP580.
esp_err_t read_identity() {
	esp_err_t result = read(kRegChipId, &state.chip_id);
	if (result != ESP_OK) {
		return result;
	}
	if (state.chip_id != kChipIdBmp580) {
		return ESP_ERR_INVALID_RESPONSE;
	}
	state.identity_known = true;
	state.status = Status::ok;
	return ESP_OK;
}

loop::Result identity(loop::Handle, loop::Value&) {
	esp_err_t result = read_identity();
	if (result != ESP_OK) {
		error(result);
		return loop::Result::done;
	}
	State::update([](State& shared) { shared = state; });
	next("baro configure", standby);
	return loop::Result::done;
}

loop::Result poll(loop::Handle, loop::Value&) {
	if (busy || esp_timer_get_time() < ready_us) {
		return loop::Result::done;
	}
	state = State::copy();
	if (state.configured && !enabled) {
		return loop::Result::done;
	}
	busy = true;
	step = 0;
	next("baro poll", !state.identity_known ? identity
					  : state.configured    ? live
											: standby);
	return loop::Result::done;
}
}  // namespace

loop::Handle poll_handle() { return periodic; }

void set_enabled(bool value) {
	if (enabled == value) {
		return;
	}
	enabled = value;
	++generation;
	busy = false;
	State::update([](State& shared) {
		shared.configured = false;
		shared.updated_us = 0;
	});
	if (device && periodic.valid()) {
		step = 0;
		state = State::copy();
		busy = true;
		next("baro mode", standby,
			 esp_timer_get_time() < ready_us ? BMP_STARTUP_MS : 0);
	}
}

Completion init_complete;

void finish_init(esp_err_t result) {
	if (result != ESP_OK && result != ESP_ERR_NOT_FOUND) {
		error(result);
	}
	if (init_complete) {
		init_complete(result);
	}
}

loop::Result identify(loop::Handle, loop::Value&) {
	state = State::copy();
	esp_err_t result = read_identity();
	if (result != ESP_OK) {
		finish_init(result);
		return loop::Result::done;
	}
	State::update([](State& shared) { shared = state; });
	periodic = loop::schedule_periodic(BMP_POLL_PERIOD_MS, {"baro poll", poll});
	if (periodic.valid()) {
		busy = true;
		step = 0;
		next("baro configure", standby);
	}
	finish_init(periodic.valid() ? ESP_OK : ESP_ERR_NO_MEM);
	return loop::Result::done;
}

loop::Result probe(loop::Handle, loop::Value&) {
	esp_err_t result = i2c_bus_add_if_present(&i2c_config, &device);
	if (result == ESP_ERR_NOT_FOUND) {
		State::update([](State& shared) { shared.status = Status::off; });
	}
	if (result == ESP_OK && !loop::hsubmit("baro identity", identify)) {
		result = ESP_ERR_NO_MEM;
	}
	if (result != ESP_OK) {
		finish_init(result);
	}
	return loop::Result::done;
}

void init(Completion complete) {
	init_complete = complete;
	// The sensor needs its start-up time after power before the first access.
	if (!loop::schedule(BMP_STARTUP_MS, {"baro probe", probe}).valid()) {
		finish_init(ESP_ERR_NO_MEM);
	}
}

esp_err_t reset() {
	bool queued = loop::hsubmit("baro reset", [](loop::Handle, loop::Value&) {
		esp_err_t result =
			device ? write(kRegCmd, kCmdSoftReset) : ESP_ERR_INVALID_STATE;
		if (result == ESP_ERR_INVALID_RESPONSE) {
			result = ESP_OK;
		}
		++generation;
		busy = false;
		ready_us = esp_timer_get_time() + BMP_STARTUP_MS * 1000;
		if (result == ESP_OK) {
			State::update([](State& state) { state = State{}; });
			if (!periodic.valid()) {
				periodic = loop::schedule_periodic(BMP_POLL_PERIOD_MS,
												   {"baro poll", poll});
				if (!periodic.valid()) {
					result = ESP_ERR_NO_MEM;
					error(result);
				}
			}
		} else {
			State::update([](State& state) {
				state.identity_known = false;
				state.configured = false;
			});
			error(result);
		}
		return loop::Result::done;
	});
	return queued ? ESP_OK : ESP_ERR_NO_MEM;
}
}  // namespace barometer
