#pragma once

#include "device.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "fusb303_i2c.hpp"
#include "i2c_bus.h"
#include "loop.hpp"

namespace fusb {
#define FUSB_I2C_ADDR 0x21
#define FUSB_POLL_PERIOD_MS 250
#define FUSB_MAX_SUBSCRIBERS 2
// tEN: no I2C access within 100 ms after a soft reset. At power-up it ran out
// before boot.
#define FUSB_RESET_DELAY_MS 100

const i2c_device_config_t i2c_config = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = FUSB_I2C_ADDR,
	.scl_speed_hz = I2C_BUS_SPEED,
	.scl_wait_us = 0,
	.flags = {},
};

struct State {
	Status status = Status::unknown;
	uint8_t raw_identity = 0, raw_status = 0, raw_status1 = 0, raw_type = 0,
			raw_control1 = 0;
	Identity identity{};
	PortStatus port{};
	Role role = Role::none;
	Control control{};
	unsigned cc_limit_ma = 0;
	int64_t updated_us = 0, error_us = 0;
	esp_err_t last_error = ESP_OK;
	bool identity_known = false, configured = false, remedy = false;

	bool usable(int64_t now_us) const {
		return state_usable(updated_us, now_us);
	}
	bool negotiating() const { return fusb::negotiating(port, remedy); }
	// 3000, 1500, or 0 for default current or no attached powered sink.
	unsigned advertised_ma() const { return cc_limit_ma; }

	template <typename F>
	static void update(F f) {
		portENTER_CRITICAL(&mutex_);
		f(storage());
		portEXIT_CRITICAL(&mutex_);
	}
	static State copy();

   private:
	static State& storage();
	static portMUX_TYPE mutex_;
};

using Completion = loop::Callable<void(esp_err_t)>;
// Called on the loop after every complete Poll.
using Handler = void (*)(const State&, void*);
bool subscribe(Handler handler, void* context = nullptr);
// Starts the driver unless it polls already. The system watchdog calls it
// again on the loop for a driver that stopped or never showed up.
void init(Completion complete);
// Posts the soft reset. The outcome shows in State on the next poll.
esp_err_t reset();
}  // namespace fusb
