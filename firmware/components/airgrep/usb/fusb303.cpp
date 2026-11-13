#include "fusb303.hpp"

#include "esp_timer.h"
#include "i2c_bus.h"
#include "sdkconfig.h"
#include "subscribers.hpp"

// The device runs a chain of I2C steps on the loop: identity, then configure,
// then the live sample. The periodic poll starts the chain where State left
// off, so a soft reset simply starts over. A failed identity or configure
// step stops the poll, the system watchdog starts the driver again. After
// SW_RES the chip wants tEN before the next access, ready_us holds that moment.
namespace fusb {
namespace {
// FUSB303B datasheet Table 11 register map, Tables 12 and 18.
constexpr uint8_t kRegDeviceId = 0x01;
constexpr uint8_t kRegControl1 = 0x05;
constexpr uint8_t kRegReset = 0x0a;
constexpr uint8_t kRegStatus = 0x11;
constexpr uint8_t kRegStatus1 = 0x12;
constexpr uint8_t kRegType = 0x13;
constexpr unsigned kVersionFusb303b = 1;  // DEVICE_ID VER_ID
constexpr uint8_t kSwReset = 1 << 0;      // RESET SW_RES

i2c_master_dev_handle_t device;
loop::Handle periodic;
State state;  // working copy of the loop task, published with State::update
bool busy;    // a chain is running
unsigned generation;  // bumped by reset, orphans the steps of the running chain
int64_t ready_us;     // tEN after SW_RES
Completion init_complete;

Subscribers<Handler, FUSB_MAX_SUBSCRIBERS> subscribers;

void publish() {
	State::update([](State& shared) { shared = state; });
}

void fail_poll(esp_err_t result) {
	int64_t now = esp_timer_get_time();
	State::update([&](State& shared) {
		shared.last_error = result;
		shared.error_us = now;
	});
	busy = false;
}

void stop(esp_err_t result) {
	fail_poll(result);
	loop::cancel(periodic);
	periodic = {};
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

using Step = loop::Result (*)(loop::Handle, loop::Value&);

void submit_step(const char* name, Step step) {
	unsigned current = generation;
	if (!loop::try_submit(
			name, [current, step](loop::Handle handle, loop::Value& value) {
				if (current != generation) {
					return loop::Result::done;
				}
				return step(handle, value);
			})) {
		fail_poll(ESP_ERR_NO_MEM);
	}
}

void finish_init(esp_err_t result) {
	if (result != ESP_OK && result != ESP_ERR_NOT_FOUND) {
		fail_poll(result);
	}
	if (init_complete) {
		init_complete(result);
	}
	init_complete = {};
}

loop::Result read_live_status(loop::Handle, loop::Value&) {
	uint8_t data[kRegType - kRegStatus + 1];
	esp_err_t result = read(kRegStatus, data, sizeof data);
	if (result != ESP_OK) {
		fail_poll(result);
		return loop::Result::done;
	}
	state.raw_status = data[0];
	state.raw_status1 = data[kRegStatus1 - kRegStatus];
	state.raw_type = data[kRegType - kRegStatus];
	state.port = decode_status(state.raw_status);
	state.remedy = decode_remedy(state.raw_status1);
	state.role = decode_type(state.raw_type);
	state.cc_limit_ma = cc_limit(state.port, state.role);
	state.updated_us = esp_timer_get_time();
	publish();
	busy = false;
	subscribers.notify(state);
	return loop::Result::done;
}

// Sets CONTROL1.ENABLE with a read, modify, write, read-back sequence.
loop::Result configure(loop::Handle, loop::Value&) {
	esp_err_t result = read(kRegControl1, &state.raw_control1);
	if (result == ESP_OK) {
		result = write(kRegControl1, state.raw_control1 | kControl1Enable);
	}
	if (result == ESP_OK) {
		result = read(kRegControl1, &state.raw_control1);
	}
	if (result == ESP_OK) {
		state.control = decode_control1(state.raw_control1);
		if (!state.control.enabled) {
			result = ESP_ERR_INVALID_RESPONSE;
		}
	}
	if (result != ESP_OK) {
		stop(result);
		return loop::Result::done;
	}
	state.configured = true;
	publish();
	submit_step("fusb sample", read_live_status);
	return loop::Result::done;
}

// Accepts only a FUSB303B. The first success completes the boot row.
loop::Result read_identity_step(loop::Handle, loop::Value&) {
	esp_err_t result = read(kRegDeviceId, &state.raw_identity);
	if (result == ESP_OK) {
		state.identity = decode_identity(state.raw_identity);
		if (state.identity.version != kVersionFusb303b) {
			result = ESP_ERR_INVALID_RESPONSE;
		}
	}
	if (result != ESP_OK) {
		stop(result);
		finish_init(result);
		return loop::Result::done;
	}
	state.identity_known = true;
	state.status = Status::ok;
	publish();
	finish_init(ESP_OK);
	submit_step("fusb configure", configure);
	return loop::Result::done;
}

loop::Result poll_usb_c(loop::Handle, loop::Value&) {
	if (busy || esp_timer_get_time() < ready_us) {
		return loop::Result::done;
	}
	busy = true;
	state = State::copy();
	submit_step("fusb poll", !state.identity_known ? read_identity_step
							 : !state.configured   ? configure
												   : read_live_status);
	return loop::Result::done;
}
}  // namespace

portMUX_TYPE State::mutex_ = portMUX_INITIALIZER_UNLOCKED;

State& State::storage() {
	static State shared;
	return shared;
}

State State::copy() {
	State result;
	update([&](State& shared) { result = shared; });
	return result;
}

bool subscribe(Handler handler, void* context) {
	return subscribers.add(handler, context);
}

// EN_N is tied to GND on the carrier, so tEN ran out while the chip booted.
// The first access can follow at once.
void init(Completion complete) {
	if (periodic.valid()) {
		return;
	}
	init_complete = complete;
	esp_err_t result = i2c_bus_add_if_present(&i2c_config, &device);
	if (result == ESP_ERR_NOT_FOUND) {
		State::update([](State& shared) { shared.status = Status::off; });
	}
	if (result == ESP_OK) {
		periodic = loop::schedule_periodic(FUSB_POLL_PERIOD_MS,
										   {"fusb poll", poll_usb_c});
		if (!periodic.valid()) {
			result = ESP_ERR_NO_MEM;
		}
	}
	if (result == ESP_OK && !loop::try_submit("fusb poll", poll_usb_c)) {
		result = ESP_ERR_NO_MEM;
	}
	if (result != ESP_OK) {
		finish_init(result);
	}
}

esp_err_t reset() {
	bool queued =
		loop::try_submit("fusb reset", [](loop::Handle, loop::Value&) {
			if (!device || esp_timer_get_time() < ready_us) {
				fail_poll(ESP_ERR_INVALID_STATE);
				return loop::Result::done;
			}
			esp_err_t result = write(kRegReset, kSwReset);
			++generation;
			busy = false;
			ready_us = esp_timer_get_time() + FUSB_RESET_DELAY_MS * 1000;
			if (result == ESP_OK) {
				State::update([](State& shared) { shared = State{}; });
			} else {
				fail_poll(result);
			}
			return loop::Result::done;
		});
	return queued ? ESP_OK : ESP_ERR_NO_MEM;
}
}  // namespace fusb
