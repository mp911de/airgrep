#pragma once

#include "device.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "i2c_bus.h"
#include "loop.hpp"
#include "tca9534_i2c.hpp"

namespace expander {
#define EXPANDER_MAX_SUBSCRIBERS 8
#define EXPANDER_I2C_ADDR 0x20
#define EXPANDER_POLL_PERIOD_MS 250
#define EXPANDER_PULSE_MS 10

const i2c_device_config_t device_config = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = EXPANDER_I2C_ADDR,
	.scl_speed_hz = I2C_BUS_SPEED,
	.scl_wait_us = 0,
	.flags = {},
};

struct State {
	Status status = Status::unknown;
	uint8_t raw_input = 0, raw_output = 0, raw_polarity = 0,
			raw_configuration = 0;
	Input input{};
	Output output{};
	Polarity polarity{};
	Configuration configuration{};
	int64_t updated_us = 0, error_us = 0;
	esp_err_t last_error = ESP_OK;
	bool identity_known = false, configured = false;

	template <typename F>
	static void update(F f) {
		portENTER_CRITICAL(&mutex_);
		f(storage());
		portEXIT_CRITICAL(&mutex_);
	}

	static bool is_valid();

	static State copy();

   private:
	static State& storage();
	static portMUX_TYPE mutex_;
};

using Completion = loop::Callable<void(esp_err_t)>;
// Called on the loop after every Poll. A flipped MUTE or Wi-Fi slider, and the
// first read of each, is also published as mute_switch_changed and
// wifi_switch_changed.
using Handler = void (*)(const State&, uint8_t changed, void*);
bool subscribe(Handler handler, void* context = nullptr);
// Starts the driver unless it polls already. The system watchdog calls it
// again on the loop for a driver that stopped or never showed up.
void init(Completion complete);
// Posts a 10 ms pulse on P2 to P5. Failures land in State and the log.
esp_err_t trigger(unsigned pin);
}  // namespace expander
