#include "max17048.hpp"
#include "esp_timer.h"
#include "i2c_bus.h"
#include "sdkconfig.h"

namespace battery {
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
		// MAX17048 datasheet: slave address 0x6C/0x6D (7-bit 0x36), register summary table 2.
		constexpr uint8_t kRegVcell = 0x02;
		constexpr uint8_t kRegSoc = 0x04; // read together with VCELL
		constexpr uint8_t kRegVersion = 0x08;
		constexpr uint8_t kRegCrate = 0x16;
		constexpr uint8_t kRegStatus = 0x1a;
		constexpr unsigned kSampleReads = 3; // VCELL with SOC, CRATE, STATUS
		i2c_master_dev_handle_t device;
		loop::Handle periodic;
		State sample;
		bool busy;
		unsigned step;

		void error(esp_err_t result) {
			int64_t now = esp_timer_get_time();
			State::update([&](State& state) {
				state.last_error = result;
				state.error_us = now;
			});
			busy = false;
		}

		esp_err_t read(uint8_t reg, uint16_t& word) {
			uint8_t bytes[2];
			esp_err_t result = i2c_master_transmit_receive(
				device, &reg, 1, bytes, sizeof bytes,
				CONFIG_AIRGREP_I2C_TIMEOUT_MS);
			if (result == ESP_OK) word = (uint16_t(bytes[0]) << 8) | bytes[1];
			return result;
		}

		// VCELL and SOC are adjacent, one transaction reads both.
		esp_err_t read_vcell_soc(uint16_t& vcell, uint16_t& soc) {
			uint8_t reg = kRegVcell, bytes[4];
			esp_err_t result = i2c_master_transmit_receive(
				device, &reg, 1, bytes, sizeof bytes,
				CONFIG_AIRGREP_I2C_TIMEOUT_MS);
			if (result == ESP_OK) {
				vcell = (uint16_t(bytes[0]) << 8) | bytes[1];
				soc = (uint16_t(bytes[2]) << 8) | bytes[3];
			}
			return result;
		}

		esp_err_t write(uint8_t reg, uint16_t word) {
			uint8_t bytes[3] = {reg, uint8_t(word >> 8), uint8_t(word)};
			return i2c_master_transmit(device, bytes, sizeof bytes, CONFIG_AIRGREP_I2C_TIMEOUT_MS);
		}

		// RI marks a gauge power-up. Nothing to reload, the defaults are in use, so just clear it.
		loop::Result acknowledge_reset(loop::Handle, loop::Value&) {
			esp_err_t result = write(kRegStatus, battery::acknowledge_reset(sample.raw_status));
			if (result != ESP_OK) error(result);
			return loop::Result::done;
		}

		loop::Result sample_word(loop::Handle, loop::Value&) {
			esp_err_t result;
			switch (step++) {
			case 0: result = read_vcell_soc(sample.raw_vcell, sample.raw_soc);
				break;
			case 1: result = read(kRegCrate, sample.raw_crate);
				break;
			default: result = read(kRegStatus, sample.raw_status);
				break;
			}
			if (result != ESP_OK) {
				error(result);
				return loop::Result::done;
			}
			if (step < kSampleReads) {
				if (!loop::hsubmit("battery sample", sample_word)) error(
					ESP_ERR_NO_MEM);
				return loop::Result::done;
			}
			sample.volts = decode_vcell(sample.raw_vcell);
			sample.soc = decode_soc(sample.raw_soc);
			sample.rate = decode_crate(sample.raw_crate);
			sample.alerts = decode_status(sample.raw_status);
			sample.remaining = remaining_time(sample.soc, sample.rate);
			sample.updated_us = esp_timer_get_time();
			State::update([](State& state) { state = sample; });
			busy = false;
			if (sample.alerts.reset && !loop::hsubmit("battery ack reset", acknowledge_reset))
				error(ESP_ERR_NO_MEM);
			return loop::Result::done;
		}

		loop::Result poll(loop::Handle handle, loop::Value& value) {
			if (busy) return loop::Result::done;
			busy = true;
			sample = State::copy();
			// Telemetry uses the retained gauge configuration. Only RI is cleared, after each sample that shows it.
			sample.configured = true;
			step = 0;
			return sample_word(handle, value);
		}
	}

	loop::Handle poll_handle() { return periodic; }

	Completion init_complete;

	void finish_init(esp_err_t result) {
		if (result != ESP_OK && result != ESP_ERR_NOT_FOUND) error(result);
		if (init_complete) init_complete(result);
	}

	loop::Result identify(loop::Handle, loop::Value&) {
		uint16_t raw;
		esp_err_t result = read(kRegVersion, raw);
		if (result != ESP_OK) {
			finish_init(result);
			return loop::Result::done;
		}
		State::update([&](State& state) {
			state.status = Status::ok;
			state.identity_known = true;
			state.raw_version = raw;
			state.version = raw;
		});
		periodic = loop::schedule_periodic(BATTERY_POLL_PERIOD_MS, {"battery poll", poll});
		finish_init(periodic.valid() ? ESP_OK : ESP_ERR_NO_MEM);
		return loop::Result::done;
	}

	loop::Result probe(loop::Handle, loop::Value&) {
		esp_err_t result = i2c_bus_add_if_present(&i2c_config, &device);
		if (result == ESP_ERR_NOT_FOUND) State::update([](State& shared) { shared.status = Status::off; });
		if (result == ESP_OK && !loop::hsubmit("battery identity", identify)) result = ESP_ERR_NO_MEM;
		if (result != ESP_OK) finish_init(result);
		return loop::Result::done;
	}

	void init(Completion complete) {
		init_complete = complete;
		if (!loop::hsubmit("battery probe", probe)) finish_init(ESP_ERR_NO_MEM);
	}
}
