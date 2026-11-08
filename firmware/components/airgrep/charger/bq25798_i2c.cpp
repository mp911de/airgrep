#include "bq25798_i2c.hpp"

namespace charger {
	namespace {
		// BQ25798 datasheet section 7.5.1: ADC registers are signed 16 bit, limits 9 bit.
		constexpr unsigned kAdcSignBit = 1 << 15;
		constexpr int kAdcRange = 1 << 16;
		constexpr float kTdieCPerLsb = 0.5f;               // REG41
		constexpr float kTsPercentPerLsb = 100.0f / 1024;  // REG3F, 0.0976563 %
		constexpr unsigned kLimitFieldMask = 0x1ff;        // REG03 ICHG_8:0, REG06 IINDPM_8:0
		constexpr unsigned kIchgMaPerLsb = 10;             // REG03
		constexpr unsigned kIindpmMaPerLsb = 10;           // REG06
		// REG48 PN_2:0 and DEV_REV_2:0
		constexpr unsigned kPartNumberShift = 3;
		constexpr uint8_t kPartFieldMask = 0x07;
		// REG2F IBUS..TDIE_ADC_DIS in bits 7..1, REG30 DP..VAC1_ADC_DIS in bits 7..4
		constexpr unsigned kAdcDisable0FirstBit = 1;
		constexpr unsigned kAdcDisable1FirstBit = 4;
		// REG2E ADC_SAMPLE_1:0, 24 ms at 15 bit halving per step
		constexpr unsigned kAdcSampleShift = 4;
		constexpr uint8_t kAdcSampleMask = 0x03;
		constexpr unsigned kAdcSample15BitMs = 24;
		// REG16 TREG_1:0, 60 C plus 20 C per step
		constexpr unsigned kTregShift = 6;
		constexpr unsigned kTregBaseC = 60;
		constexpr unsigned kTregStepC = 20;
		// REG14
		constexpr uint8_t kDischargeSensing = 1 << 5;  // EN_IBAT
		constexpr uint8_t kExternalLimit = 1 << 1;     // EN_EXTILIM
		// REG0F
		constexpr uint8_t kHighImpedance = 1 << 2;     // EN_HIZ
		// REG22 and REG1B share the bit layout except POORSRC_FLAG at bit 4
		constexpr uint8_t kInputLimit = 1 << 7;
		constexpr uint8_t kVoltageLimit = 1 << 6;
		constexpr uint8_t kWatchdog = 1 << 5;
		constexpr uint8_t kPoorSource = 1 << 4;
		constexpr uint8_t kPowerGood = 1 << 3;
		constexpr uint8_t kAc2Present = 1 << 2;
		constexpr uint8_t kAc1Present = 1 << 1;
		constexpr uint8_t kVbusPresent = 1 << 0;
		// REG21 and REG27
		constexpr uint8_t kSystemShort = 1 << 7;
		constexpr uint8_t kSystemOvervoltage = 1 << 6;
		constexpr uint8_t kOtgOvervoltage = 1 << 5;
		constexpr uint8_t kOtgUndervoltage = 1 << 4;
		constexpr uint8_t kThermalShutdown = 1 << 2;
		// REG20 and REG26
		constexpr uint8_t kBatteryRegulation = 1 << 7;
		constexpr uint8_t kVbusOvervoltage = 1 << 6;
		constexpr uint8_t kBatteryOvervoltage = 1 << 5;
		constexpr uint8_t kBusOvercurrent = 1 << 4;
		constexpr uint8_t kBatteryOvercurrent = 1 << 3;
		constexpr uint8_t kConverterOvercurrent = 1 << 2;
		constexpr uint8_t kAc2Overvoltage = 1 << 1;
		constexpr uint8_t kAc1Overvoltage = 1 << 0;
		// REG1F
		constexpr uint8_t kOtgLow = 1 << 4;
		constexpr uint8_t kTsCold = 1 << 3;
		constexpr uint8_t kTsCool = 1 << 2;
		constexpr uint8_t kTsWarm = 1 << 1;
		constexpr uint8_t kTsHot = 1 << 0;
		// REG1E
		constexpr uint8_t kAcrb2 = 1 << 7;
		constexpr uint8_t kAcrb1 = 1 << 6;
		constexpr uint8_t kAdcDone = 1 << 5;
		constexpr uint8_t kMinimumSystem = 1 << 4;
		constexpr uint8_t kChargeTimer = 1 << 3;
		constexpr uint8_t kTrickleTimer = 1 << 2;
		constexpr uint8_t kPrechargeTimer = 1 << 1;
		// REG1D
		constexpr unsigned kIcoShift = 6;
		constexpr uint8_t kThermalRegulation = 1 << 2;
		constexpr uint8_t kDpdmDone = 1 << 1;
		constexpr uint8_t kVbatPresent = 1 << 0;
		// REG1C
		constexpr unsigned kChargeStatusShift = 5;
		constexpr unsigned kVbusStatusShift = 1;
		constexpr uint8_t kVbusStatusMask = 0x0f;
		constexpr uint8_t kDetectionDone = 1 << 0;
	}

	float decode_tdie(uint8_t high, uint8_t low) {
		unsigned raw = (unsigned(high) << 8) | low;
		return (raw & kAdcSignBit ? int(raw) - kAdcRange : int(raw)) * kTdieCPerLsb;
	}

	float decode_ts(uint8_t high, uint8_t low) {
		return ((unsigned(high) << 8) | low) * kTsPercentPerLsb;
	}

	unsigned decode_adc_mv(uint8_t high, uint8_t low) {
		return (unsigned(high) << 8) | low;
	}

	int decode_ibat(uint8_t high, uint8_t low) {
		unsigned raw = (unsigned(high) << 8) | low;
		return raw & kAdcSignBit ? int(raw) - kAdcRange : int(raw);
	}

	int decode_ibus(uint8_t high, uint8_t low) {
		unsigned raw = (unsigned(high) << 8) | low;
		return raw & kAdcSignBit ? int(raw) - kAdcRange : int(raw);
	}

	unsigned decode_input_current(uint8_t high, uint8_t low) {
		return (((unsigned(high) << 8) | low) & kLimitFieldMask) * kIindpmMaPerLsb;
	}

	unsigned decode_charge_current(uint8_t high, uint8_t low) {
		return (((unsigned(high) << 8) | low) & kLimitFieldMask) * kIchgMaPerLsb;
	}

	Identity decode_identity(uint8_t raw) {
		return {uint8_t((raw >> kPartNumberShift) & kPartFieldMask), uint8_t(raw & kPartFieldMask)};
	}

	unsigned decode_adc_disable1(uint8_t raw) {
		unsigned enabled = 0;
		for (unsigned bit = kAdcDisable1FirstBit; bit < 8; bit++) if (!(raw & (1u << bit))) enabled++;
		return enabled;
	}

	unsigned decode_adc_disable0(uint8_t raw) {
		unsigned enabled = 0;
		for (unsigned bit = kAdcDisable0FirstBit; bit < 8; bit++) if (!(raw & (1u << bit))) enabled++;
		return enabled;
	}

	AdcControl decode_adc_control(uint8_t raw) {
		return {bool(raw & kAdcEnable), !(raw & kAdcOneShot),
			kAdcSample15BitMs >> ((raw >> kAdcSampleShift) & kAdcSampleMask)};
	}

	unsigned decode_temperature_control(uint8_t raw) {
		return kTregBaseC + kTregStepC * (raw >> kTregShift);
	}

	Control5 decode_control5(uint8_t raw) {
		return {bool(raw & kSfetPresent), bool(raw & kDischargeSensing), bool(raw & kExternalLimit)};
	}

	Control1 decode_control1(uint8_t raw) {
		// REG10 WATCHDOG_2:0
		static constexpr unsigned kWatchdogPeriodsMs[] = {0, 500, 1000, 2000, 20000, 40000, 80000, 160000};
		return {kWatchdogPeriodsMs[raw & kWatchdogFieldMask], bool(raw & kWatchdogReset)};
	}

	Control0 decode_control0(uint8_t raw) {
		return {bool(raw & kHighImpedance)};
	}

	Fault1 decode_fault_flag1(uint8_t raw) {
		return decode_fault1(raw);
	}

	Fault0 decode_fault_flag0(uint8_t raw) {
		return decode_fault0(raw);
	}

	Flag0 decode_flag0(uint8_t raw) {
		return {bool(raw & kInputLimit), bool(raw & kVoltageLimit), bool(raw & kWatchdog), bool(raw & kPoorSource),
			bool(raw & kPowerGood), bool(raw & kAc2Present), bool(raw & kAc1Present), bool(raw & kVbusPresent)};
	}

	Fault1 decode_fault1(uint8_t raw) {
		return {bool(raw & kSystemShort), bool(raw & kSystemOvervoltage), bool(raw & kOtgOvervoltage),
			bool(raw & kOtgUndervoltage), bool(raw & kThermalShutdown)};
	}

	Fault0 decode_fault0(uint8_t raw) {
		return {bool(raw & kBatteryRegulation), bool(raw & kVbusOvervoltage), bool(raw & kBatteryOvervoltage),
			bool(raw & kBusOvercurrent), bool(raw & kBatteryOvercurrent), bool(raw & kConverterOvercurrent),
			bool(raw & kAc2Overvoltage), bool(raw & kAc1Overvoltage)};
	}

	Status4 decode_status4(uint8_t raw) {
		return {bool(raw & kOtgLow), bool(raw & kTsCold), bool(raw & kTsCool), bool(raw & kTsWarm), bool(raw & kTsHot)};
	}

	Status3 decode_status3(uint8_t raw) {
		return {bool(raw & kAcrb2), bool(raw & kAcrb1), bool(raw & kAdcDone), bool(raw & kMinimumSystem),
			bool(raw & kChargeTimer), bool(raw & kTrickleTimer), bool(raw & kPrechargeTimer)};
	}

	Status2 decode_status2(uint8_t raw) {
		return {uint8_t(raw >> kIcoShift), bool(raw & kThermalRegulation), bool(raw & kDpdmDone),
			bool(raw & kVbatPresent)};
	}

	Status1 decode_status1(uint8_t raw) {
		return {uint8_t(raw >> kChargeStatusShift), uint8_t((raw >> kVbusStatusShift) & kVbusStatusMask),
			bool(raw & kDetectionDone)};
	}

	Status0 decode_status0(uint8_t raw) {
		return {bool(raw & kInputLimit), bool(raw & kVoltageLimit), bool(raw & kWatchdog), bool(raw & kPowerGood),
			bool(raw & kAc2Present), bool(raw & kAc1Present), bool(raw & kVbusPresent)};
	}
}
