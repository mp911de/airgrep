#pragma once

#include <cstdint>

#include "charge_target.hpp"
#include "device.h"

namespace charger {
// Register fields shared by driver and decoder, BQ25798 datasheet
// section 7.5.1.
static constexpr uint8_t kWatchdogReset = 1 << 3;     // REG10 WD_RST
static constexpr uint8_t kWatchdogFieldMask = 0x07;   // REG10 WATCHDOG_2:0
static constexpr uint8_t kSfetPresent = 1 << 7;       // REG14 SFET_PRESENT
static constexpr uint8_t kDischargeSensing = 1 << 5;  // REG14 EN_IBAT
static constexpr uint8_t kExternalLimit = 1 << 1;     // REG14 EN_EXTILIM
static constexpr uint8_t kAdcEnable = 1 << 7;         // REG2E ADC_EN
static constexpr uint8_t kAdcOneShot = 1
									   << 6;  // REG2E ADC_RATE, 0 is continuous

struct Status0 {
	bool input_limit, voltage_limit, watchdog, power_good, ac2, ac1, vbus;
};

struct Identity {
	uint8_t part, revision;
};

struct AdcControl {
	bool enabled, continuous;
	unsigned sample_ms;
};

struct Control5 {
	bool ship_fet, discharge_sensing, external_limit;
};

struct Control1 {
	unsigned watchdog_ms;
	bool reset;
};

struct Control0 {
	bool high_impedance, charge_enabled;
};

struct Flag0 {
	bool input_limit, voltage_limit, watchdog, poor_source, power_good, ac2,
		ac1, vbus;
};

struct Fault1 {
	bool system_short, system_overvoltage, otg_overvoltage, otg_undervoltage,
		thermal_shutdown;
};

struct Fault0 {
	bool battery_regulation, vbus_overvoltage, battery_overvoltage,
		bus_overcurrent, battery_overcurrent, converter_overcurrent,
		ac2_overvoltage, ac1_overvoltage;
};

struct Status4 {
	bool otg_low, cold, cool, warm, hot;
};

struct Status3 {
	bool ac2, ac1, adc_done, minimum_system, charge_timer, trickle_timer,
		precharge_timer;
};

struct Status2 {
	uint8_t optimizer;
	bool thermal, detection, battery;
};

// CHG_STAT (data sheet 7.3.9): every code between these two charges.
inline constexpr uint8_t kChargeNone = 0, kChargeDone = 7;

struct Status1 {
	uint8_t charge, vbus;
	bool detection_done;
};

// Without a pack the BAT pin falls to about 2.5 V once charging stops (data
// sheet 7.3.8). A drop above 1500 mV between a charging and a disabled VBAT
// reading means no battery.
BatteryDetected detect_battery(unsigned vbat_charging_mv,
							   unsigned vbat_disabled_mv);

// Input current of the D+/D- detection result, BQ25798 table 7-4 with DCP at
// the BC1.2 1.5 A and a non-standard adapter at its lowest divider, 1 A,
// because VBUS_STAT does not name the divider. Unknown sources count as
// 500 mA, no VBUS as 0.
unsigned detected_ma(Status1 status1, bool vbus);
bool detecting(bool vbus, Status1 status1);
// The NTC estimate, NaN while the ADC is pending or out of range.
float pack_celsius(bool adc_valid, float ts_percent);
// ICHG and IINDPM share the 10 mA, 9 bit field of a big-endian register pair.
uint16_t encode_current(uint16_t raw, unsigned ma);
uint8_t encode_temperature_control(uint8_t raw, unsigned celsius);

Status0 decode_status0(uint8_t raw);
float decode_tdie(uint8_t high, uint8_t low);
float decode_ts(uint8_t high, uint8_t low);
// Nominal divider and beta-model estimate, NaN for invalid or out-of-range
// input.
float estimate_ntc_c(float ts_percent);
// VBUS, VBAT and VSYS ADC registers share the 1 mV per LSB format.
unsigned decode_adc_mv(uint8_t high, uint8_t low);
int decode_ibat(uint8_t high, uint8_t low);
int decode_ibus(uint8_t high, uint8_t low);
unsigned decode_input_current(uint8_t high, uint8_t low);
unsigned decode_charge_current(uint8_t high, uint8_t low);
Identity decode_identity(uint8_t raw);
unsigned decode_adc_disable1(uint8_t raw);
unsigned decode_adc_disable0(uint8_t raw);
AdcControl decode_adc_control(uint8_t raw);
unsigned decode_temperature_control(uint8_t raw);
Control5 decode_control5(uint8_t raw);
Control1 decode_control1(uint8_t raw);
Control0 decode_control0(uint8_t raw);
Fault1 decode_fault_flag1(uint8_t raw);
Fault0 decode_fault_flag0(uint8_t raw);
Flag0 decode_flag0(uint8_t raw);
Fault1 decode_fault1(uint8_t raw);
Fault0 decode_fault0(uint8_t raw);
Status4 decode_status4(uint8_t raw);
Status3 decode_status3(uint8_t raw);
Status2 decode_status2(uint8_t raw);
Status1 decode_status1(uint8_t raw);

struct PollReadings {
	Status status = Status::unknown;
	uint8_t raw[0x49]{};
	Identity identity{};
	Status0 status0{};
	Status1 status1{};
	Status2 status2{};
	Status3 status3{};
	Status4 status4{};
	Fault0 fault0{}, fault_flag0{};
	Fault1 fault1{}, fault_flag1{};
	Flag0 flag0{};
	Control0 control0{};
	Control1 control1{};
	Control5 control5{};
	AdcControl adc_control{};
	unsigned charge_current_ma = 0, input_current_ma = 0, thermal_limit_c = 0;
	int ibus_ma = 0, ibat_ma = 0;
	unsigned vbus_mv = 0, vbat_mv = 0, vsys_mv = 0;
};

struct StateData {
	PollReadings readings{};
	BatteryDetected battery_detected = BatteryDetected::unknown;
	unsigned battery_detection_vbat_mv = 0;
	bool battery_detection_vbat_valid = false;
	float ts_percent = 0, die_c = 0;
	int64_t updated_us = 0, error_us = 0, adc_ready_us = 0;
	EspErrorCode last_error = 0;
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
		if (adc_valid && !readings.status0.vbus && readings.vbat_mv >= 3000) {
			return BatteryDetected::voltage_present;
		}
		return BatteryDetected::unknown;
	}

	bool usable(int64_t now_us) const {
		return state_usable(updated_us, now_us);
	}
	bool detecting() const {
		return charger::detecting(readings.status0.vbus, readings.status1);
	}
	unsigned detected_ma() const {
		return charger::detected_ma(readings.status1, readings.status0.vbus);
	}
	float pack_celsius() const {
		return charger::pack_celsius(adc_valid, ts_percent);
	}
};
}
