#include "console.h"
#include "charger/bq25798.hpp"
#include <cstdio>
#include "esp_console.h"
#include "command_status.hpp"

namespace {
	// Label column: name and register, padded to the shared 20 character width.
	void label(const char* name, const char* reg) {
		int written = std::printf("%s (%s)", name, reg);
		std::printf("%*s", written < 20 ? 21 - written : 1, "");
	}

	void fault(bool set, const char* name, const char* reg, uint8_t raw) {
		if (!set) return;
		label(name, reg);
		std::printf("ON (0x%02X)\n", raw);
	}

	void faults(const charger::Fault0& value, const char* reg, uint8_t raw) {
		fault(value.ac1_overvoltage, "VAC1 OV", reg, raw);
		fault(value.ac2_overvoltage, "VAC2 OV", reg, raw);
		fault(value.converter_overcurrent, "Converter OC", reg, raw);
		fault(value.battery_overcurrent, "Battery OC", reg, raw);
		fault(value.bus_overcurrent, "Bus OC", reg, raw);
		fault(value.battery_overvoltage, "Battery OV", reg, raw);
		fault(value.vbus_overvoltage, "VBUS OV", reg, raw);
		fault(value.battery_regulation, "Batt regulation", reg, raw);
	}

	void faults(const charger::Fault1& value, const char* reg, uint8_t raw) {
		fault(value.thermal_shutdown, "Thermal shdn", reg, raw);
		fault(value.otg_undervoltage, "OTG UV", reg, raw);
		fault(value.otg_overvoltage, "OTG OV", reg, raw);
		fault(value.system_overvoltage, "System OV", reg, raw);
		fault(value.system_short, "System short", reg, raw);
	}

	void adc(const charger::State& state, const char* name, const char* reg,
			unsigned address, unsigned disable_bit, double value, const char* unit, int decimals) {
		unsigned raw = (unsigned(state.raw[address]) << 8) | state.raw[address + 1];
		label(name, reg);
		if (state.raw[0x2f] & (1u << disable_bit))
			std::printf("DISABLED (0x%04X)\n", raw);
		else if (address == 0x33 && !state.status0.vbus && !state.control5.
			discharge_sensing)
			std::printf("UNAVAILABLE, EN_IBAT off in battery-only mode (0x%04X)\n", raw);
		else if (!state.adc_valid)
			std::printf("PENDING (0x%04X)\n", raw);
		else std::printf("%.*f %s (0x%04X)\n", decimals, value, unit, raw);
	}

	int command(int argc, char**) {
		if (argc != 1) {
			std::puts("usage: charger");
			return 1;
		}
		const auto state = charger::State::copy();
		if (!command_status::header(state.status, state.updated_us, state.last_error, state.error_us)) return 0;
		const auto* raw = state.raw;
		static const char* charge[] = {
			"NOT CHARGING", "TRICKLE", "PRECHARGE", "CONSTANT CURRENT",
			"CONSTANT VOLTAGE", "RESERVED", "TOP-OFF", "DONE"
		};
		static const char* vbus[] = {
			"NO INPUT", "USB SDP", "USB CDP", "USB DCP", "HVDCP",
			"UNKNOWN ADAPTER", "NON-STANDARD ADAPTER", "OTG", "UNQUALIFIED ADAPTER",
			"RESERVED",
			"RESERVED", "DIRECT VBUS", "BACKUP", "RESERVED", "RESERVED", "RESERVED"
		};
		std::printf("%-20s %s (0x%02X)\n", "VBUS (REG1B)", state.status0.vbus ? "PRESENT" : "ABSENT", raw[0x1b]);
		std::printf("%-20s %s (0x%02X)\n", "VBUS type (REG1C)", vbus[state.status1.vbus], raw[0x1c]);
		std::printf("%-20s %s (0x%02X)\n", "Charge (REG1C)", charge[state.status1.charge], raw[0x1c]);
		fault(state.status3.charge_timer, "Charge timer", "REG1E", raw[0x1e]);
		fault(state.status3.trickle_timer, "Trickle timer", "REG1E", raw[0x1e]);
		fault(state.status3.precharge_timer, "Precharge timer", "REG1E", raw[0x1e]);
		fault(state.status4.otg_low, "OTG battery low", "REG1F", raw[0x1f]);
		fault(state.status4.cold, "TS cold", "REG1F", raw[0x1f]);
		fault(state.status4.cool, "TS cool", "REG1F", raw[0x1f]);
		fault(state.status4.warm, "TS warm", "REG1F", raw[0x1f]);
		fault(state.status4.hot, "TS hot", "REG1F", raw[0x1f]);
		faults(state.fault0, "REG20", raw[0x20]);
		faults(state.fault_flag0, "REG26", raw[0x26]);
		faults(state.fault1, "REG21", raw[0x21]);
		faults(state.fault_flag1, "REG27", raw[0x27]);
		fault(state.flag0.poor_source, "Poor source", "REG22", raw[0x22]);
		adc(state, "VBUS", "REG35", 0x35, 5, state.vbus_mv, "mV", 0);
		adc(state, "IBUS", "REG31", 0x31, 7, state.ibus_ma, "mA", 0);
		adc(state, "VBAT", "REG3B", 0x3b, 4, state.vbat_mv, "mV", 0);
		adc(state, "IBAT", "REG33", 0x33, 6, state.ibat_ma, "mA", 0);
		adc(state, "VSYS", "REG3D", 0x3d, 3, state.vsys_mv, "mV", 0);
		adc(state, "TS", "REG3F", 0x3f, 2, state.ts_percent, "%", 2);
		adc(state, "TDIE", "REG41", 0x41, 1, state.die_c, "C", 1);
		std::printf("%-20s %u mA (0x%02X%02X)\n", "Charge limit (ICHG)", state.charge_current_ma, raw[3], raw[4]);
		std::printf("%-20s %u mA (0x%02X%02X)\n", "Input limit (IINDPM)", state.input_current_ma, raw[6], raw[7]);
		std::printf("%-20s %u C (0x%02X)\n", "Thermal limit (TREG)", state.thermal_limit_c, raw[0x16]);
		std::printf("%-20s %u ms (0x%02X)\n", "Watchdog (REG10)", state.control1.watchdog_ms, raw[0x10]);
		std::printf("%-20s %s (0x%02X)\n", "WD_STAT (REG1B)", state.status0.watchdog ? "EXPIRED" : "NORMAL", raw[0x1b]);
		std::printf("%-20s %s (0x%02X)\n", "EN_HIZ (REG0F)", state.control0.high_impedance ? "ON" : "OFF", raw[0x0f]);
		std::printf("%-20s %s (0x%02X)\n", "EN_EXTILIM (REG14)", state.control5.external_limit ? "ON" : "OFF", raw[0x14]);
		std::printf("%-20s %s (0x%02X)\n", "SFET_PRESENT (REG14)", state.control5.ship_fet ? "PRESENT" : "ABSENT", raw[0x14]);
		return 0;
	}
}

extern "C" esp_err_t console_register_charger(void) {
	const esp_console_cmd_t entry = {
		.command = "charger",
		.help = "Show charger status, faults, ADC readings and configuration.",
		.hint = "", .func = command, .argtable = nullptr,
		.func_w_context = nullptr, .context = nullptr,
	};
	return console_register(&entry);
}
