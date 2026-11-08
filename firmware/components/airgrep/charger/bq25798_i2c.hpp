#pragma once

#include <cstdint>

namespace charger {
	// Register fields shared by driver and decoder, BQ25798 datasheet section 7.5.1.
	static constexpr uint8_t kWatchdogReset = 1 << 3; // REG10 WD_RST
	static constexpr uint8_t kWatchdogFieldMask = 0x07; // REG10 WATCHDOG_2:0
	static constexpr uint8_t kSfetPresent = 1 << 7; // REG14 SFET_PRESENT
	static constexpr uint8_t kAdcEnable = 1 << 7; // REG2E ADC_EN
	static constexpr uint8_t kAdcOneShot = 1 << 6; // REG2E ADC_RATE, 0 is continuous

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
		bool high_impedance;
	};

	struct Flag0 {
		bool input_limit, voltage_limit, watchdog, poor_source, power_good, ac2, ac1,
			vbus;
	};

	struct Fault1 {
		bool system_short, system_overvoltage, otg_overvoltage, otg_undervoltage,
			thermal_shutdown;
	};

	struct Fault0 {
		bool battery_regulation, vbus_overvoltage, battery_overvoltage, bus_overcurrent,
			battery_overcurrent, converter_overcurrent, ac2_overvoltage, ac1_overvoltage;
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

	struct Status1 {
		uint8_t charge, vbus;
		bool detection_done;
	};

	Status0 decode_status0(uint8_t raw);
	float decode_tdie(uint8_t high, uint8_t low);
	float decode_ts(uint8_t high, uint8_t low);
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
}
