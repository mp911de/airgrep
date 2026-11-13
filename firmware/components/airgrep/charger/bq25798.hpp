#pragma once

#include "bq25798_i2c.hpp"
#include "charge_target.hpp"
#include "device.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "i2c_bus.h"
#include "loop.hpp"

namespace charger {
#define CHARGER_I2C_ADDR 0x6b
#define CHARGER_POLL_PERIOD_MS 250
#define CHARGER_WATCHDOG_MS 20000
#define CHARGER_MAX_SUBSCRIBERS 2

const i2c_device_config_t i2c_config = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = CHARGER_I2C_ADDR,
	.scl_speed_hz = I2C_BUS_SPEED,
	.scl_wait_us = 0,
	.flags = {},
};

struct State {
	Status status = Status::unknown;
	uint8_t raw[0x49]{};
	Identity identity{};
	Status0 status0{};  // input limits, watchdog, power good, VBUS present
	Status1 status1{};  // charge phase, VBUS source type, BC1.2 done
	Status2
		status2{};  // ICO, thermal regulation, D+/D- detection, VBAT present
	Status3 status3{};  // input FETs, ADC done, VSYS regulation, safety timers
	Status4 status4{};  // OTG battery low, TS temperature zones
	Fault0 fault0{},
		fault_flag0{};  // overvoltage and overcurrent, live and latched
	Fault1 fault1{},
		fault_flag1{};    // VSYS, OTG and thermal shutdown, live and latched
	Flag0 flag0{};        // latched status changes, poor source
	Control0 control0{};  // charge enable, HIZ, termination
	Control1 control1{};  // watchdog
	Control5 control5{};  // ship FET, IBAT sensing, external ILIM
	AdcControl adc_control{};
	unsigned charge_current_ma = 0, input_current_ma = 0, thermal_limit_c = 0;
	int ibus_ma = 0, ibat_ma = 0;
	unsigned vbus_mv = 0, vbat_mv = 0, vsys_mv = 0;
	BatteryDetected battery_detected = BatteryDetected::unknown;
	unsigned battery_detection_vbat_mv = 0;
	bool battery_detection_vbat_valid = false;
	float ts_percent = 0, die_c = 0;
	int64_t updated_us = 0, error_us = 0, adc_ready_us = 0;
	esp_err_t last_error = ESP_OK;
	bool identity_known = false, configured = false, adc_valid = false;
	TargetState target{};

	// The battery test result when one ran. Otherwise, without VBUS the system
	// runs from the pack, so a valid VBAT reading proves it. With VBUS the BAT
	// pin carries the charger's own voltage and says nothing (data
	// sheet 7.3.8).
	BatteryDetected battery() const {
		if (battery_detected != BatteryDetected::unknown) {
			return battery_detected;
		}
		if (adc_valid && !status0.vbus && vbat_mv >= 3000) {
			return BatteryDetected::vbat;
		}
		return BatteryDetected::unknown;
	}

	bool usable(int64_t now_us) const {
		return state_usable(updated_us, now_us);
	}
	bool detecting() const { return charger::detecting(status0.vbus, status1); }
	unsigned detected_ma() const {
		return charger::detected_ma(status1, status0.vbus);
	}
	float pack_celsius() const {
		return charger::pack_celsius(adc_valid, ts_percent);
	}

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
struct ChargeResult {
	esp_err_t error = ESP_OK, restore_error = ESP_OK;
	bool initial_enabled = false, before_valid = false, after_valid = false;
	uint16_t vbat_before = 0, vbat_after = 0;
	int ibat_after_ma = 0;  // taken with vbat_after, negative while discharging
};

using ChargeCompletion = loop::Callable<void(ChargeResult)>;
esp_err_t set_charge_enabled(bool enabled, ChargeCompletion complete);
esp_err_t battery_test(ChargeCompletion complete);
esp_err_t verify_battery(ChargeCompletion complete);
bool battery_test_running();
// Writes ICHG or IINDPM in mA with a read-back.
esp_err_t set_charge_limit(unsigned ma, ChargeCompletion complete);
esp_err_t set_input_limit(unsigned ma, ChargeCompletion complete);
// Called on the loop after every complete Poll.
using Handler = void (*)(const State&, void*);
bool subscribe(Handler handler, void* context = nullptr);
// On the loop: writes the registers the last Poll read back differently from
// target. The hardware input limit is set before IINDPM is lowered and
// cleared only after IINDPM was raised. A failed write waits for the next
// call.
void set_charger_target(const TargetState& target);
// Starts the driver unless it polls already. The system watchdog calls it
// again on the loop for a driver that stopped or never showed up.
void init(Completion complete);
}  // namespace charger
