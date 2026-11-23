#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

#include "argtable.hpp"
#include "battery/max17048.hpp"
#include "charger/bq25798.hpp"
#include "command_status.hpp"
#include "console.h"
#include "duration.hpp"
#include "esp_console.h"
#include "freertos/semphr.h"
#include "settings/settings.hpp"
#include "system/charge_control.hpp"

namespace {
// REG06 and REG03 ranges, BQ25798 datasheet 7.5.1.
constexpr unsigned kInputLimitMinMa = 100, kInputLimitMaxMa = 3300;
constexpr unsigned kChargeLimitMinMa = 50, kChargeLimitMaxMa = 5000;

struct {
	struct arg_lit *battery_test, *verify_battery, *enable_charge,
		*disable_charge;
	struct arg_str* mode;
	struct arg_int *iindpm, *ichg;
	struct arg_end* end;
	void* table[8];
} args;

template <typename Queue>
int charge_command(bool test, Queue queue) {
	struct Reply {
		StaticSemaphore_t storage{};
		SemaphoreHandle_t done{};
		charger::ChargeResult result;
	} reply{};
	reply.done = xSemaphoreCreateBinaryStatic(&reply.storage);
	auto complete = [&reply](charger::ChargeResult result) {
		reply.result = result;
		xSemaphoreGive(reply.done);
	};
	const esp_err_t queued = queue(complete);
	if (queued != ESP_OK) {
		vSemaphoreDelete(reply.done);
		return command_status::ack(queued);
	}
	// The driver completes every exit path. Keep the reply alive through
	// restoration.
	xSemaphoreTake(reply.done, portMAX_DELAY);
	vSemaphoreDelete(reply.done);
	const auto& result = reply.result;
	if (test && result.before_valid) {
		std::printf("%-20s %s\n", "Initial EN_CHG",
					result.initial_enabled ? "ON" : "OFF");
		std::printf("%-20s %u mV (0x%04X)\n", "VBAT before", result.vbat_before,
					result.vbat_before);
	}
	if (test && result.after_valid) {
		std::printf("%-20s %u mV (0x%04X)\n", "VBAT after 1 s OFF",
					result.vbat_after, result.vbat_after);
		std::printf("%-20s %+d mA\n", "IBAT after 1 s OFF",
					result.ibat_after_ma);
		std::printf(
			"%-20s %+ld mW\n", "Battery power",
			long(result.ibat_after_ma) * long(result.vbat_after) / 1000);
	}
	if (result.restore_error != ESP_OK) {
		std::printf("%-20s FAIL %s\n", "Restore EN_CHG",
					esp_err_to_name(result.restore_error));
	} else if (test && result.before_valid) {
		std::printf("%-20s %s\n", "Restored EN_CHG",
					result.initial_enabled ? "ON" : "OFF");
	}
	return command_status::ack(result.error != ESP_OK ? result.error
													  : result.restore_error);
}

void label(const char* name, const char* reg) {
	const int written = std::printf("%s (%s)", name, reg);
	std::printf("%*s", written < 20 ? 21 - written : 1, "");
}

void fault(bool set, const char* name, const char* reg, uint8_t raw) {
	if (!set) {
		return;
	}
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
		 unsigned address, unsigned disable_bit, double value, const char* unit,
		 int decimals) {
	const unsigned raw = (unsigned(state.readings.raw[address]) << 8) |
						 state.readings.raw[address + 1];
	label(name, reg);
	if (state.readings.raw[0x2f] & (1u << disable_bit)) {
		std::printf("DISABLED (0x%04X)\n", raw);
	} else if (!state.adc_valid) {
		std::printf("PENDING (0x%04X)\n", raw);
	} else if (address == 0x3f) {
		const float temperature =
			charger::estimate_ntc_c(static_cast<float>(value));
		std::printf("%.*f %s (", decimals, value, unit);
		if (std::isfinite(temperature)) {
			std::printf("≈%.1f°C, ", temperature);
		} else {
			std::printf("NTC UNAVAILABLE, ");
		}
		std::printf("0x%04X)\n", raw);
	} else {
		std::printf("%.*f %s (0x%04X)\n", decimals, value, unit, raw);
	}
}

const char* pending(bool differs) { return differs ? " (pending)" : ""; }

void control(const charger::State& state, const char* const* vbus_names) {
	const auto& target = state.target;
	std::printf("%-20s %s\n", "Control",
				charge_control::get_charger_mode_name(target.mode));
	if (!target.target_available) {
		return;
	}
	std::printf("%-20s ", "Source");
	switch (target.supply) {
		case charger::Supply::none:
			std::puts("NO INPUT");
			break;
		case charger::Supply::waiting:
			std::printf("WAITING %.1f s\n",
						static_cast<double>(target.vbus_elapsed_us) / 1e6);
			break;
		case charger::Supply::usb_c:
			std::printf("USB-C %u mA (CC)\n", target.input_ma);
			break;
		case charger::Supply::detection:
			std::printf("BC1.2 %s %u mA\n",
						vbus_names[target.detected_vbus_type & 0x0f],
						target.input_ma);
			break;
	}
	const bool input_differs =
		state.readings.control5.external_limit != target.hardware_limit ||
		(target.input_ma && state.readings.input_current_ma != target.input_ma);
	std::printf("%-20s ", "Input target");
	if (target.input_ma) {
		std::printf("%u mA, ", target.input_ma);
	}
	std::printf("hardware limit %s%s\n", target.hardware_limit ? "ON" : "OFF",
				pending(input_differs));
	std::printf("%-20s %u mA%s\n", "Charge target", target.charge_ma,
				pending(state.readings.charge_current_ma != target.charge_ma));
	static const char* const reasons[] = {
		"",
		" (battery cold)",
		" (battery hot)",
		" (battery overvoltage)",
		" (no battery)",
		" (battery overvoltage, locked until restart)"};
	std::printf("%-20s %s%s%s\n", "Charging",
				target.charge_enabled ? "ON" : "OFF",
				reasons[uint8_t(target.stop)],
				pending(state.readings.control0.charge_enabled !=
						target.charge_enabled));
}

int command(int argc, char** argv) {
	if (arg_parse(argc, argv, args.table)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	const int setters = args.battery_test->count + args.verify_battery->count +
						args.enable_charge->count + args.disable_charge->count +
						args.iindpm->count + args.ichg->count;
	if (setters + args.mode->count > 1) {
		std::puts(
			"Choose one of --mode, --iindpm, --ichg, --battery-test, "
			"--verify-battery, --enable-charge, --disable-charge");
		return 1;
	}
	if (args.mode->count) {
		const char* name = args.mode->sval[0];
		if (std::strcmp(name, "AUTO") && std::strcmp(name, "MANUAL")) {
			std::puts("Mode is AUTO or MANUAL");
			return 1;
		}
		const esp_err_t result = charge_control::set_charger_mode(
			std::strcmp(name, "MANUAL") ? charger::ControlMode::automatic
										: charger::ControlMode::manual);
		if (result == ESP_ERR_INVALID_STATE) {
			std::puts("Rejected while a battery test runs");
		}
		return command_status::ack(result);
	}
	if (setters &&
		charge_control::get_charger_mode() == charger::ControlMode::automatic) {
		std::puts("Rejected in AUTO, switch with --mode MANUAL");
		return 1;
	}
	if (args.iindpm->count) {
		const int ma = args.iindpm->ival[0];
		const unsigned ceiling =
			charger::State::copy().readings.control5.external_limit
				? CHARGER_HARDWARE_INPUT_LIMIT_MA
				: kInputLimitMaxMa;
		if (std::cmp_less(ma, kInputLimitMinMa) ||
			std::cmp_greater(ma, ceiling)) {
			std::printf("IINDPM is %u to %u mA%s\n", kInputLimitMinMa, ceiling,
						ceiling == CHARGER_HARDWARE_INPUT_LIMIT_MA
							? " while the hardware input limit is on"
							: "");
			return 1;
		}
		return charge_command(false, [ma](charger::ChargeCompletion done) {
			return charger::set_input_limit(ma, done);
		});
	}
	if (args.ichg->count) {
		const int ma = args.ichg->ival[0];
		if (std::cmp_less(ma, kChargeLimitMinMa) ||
			std::cmp_greater(ma, kChargeLimitMaxMa)) {
			std::printf("ICHG is %u to %u mA\n", kChargeLimitMinMa,
						kChargeLimitMaxMa);
			return 1;
		}
		return charge_command(false, [ma](charger::ChargeCompletion done) {
			return charger::set_charge_limit(ma, done);
		});
	}
	if (args.verify_battery->count) {
		return command_status::ack(
			charger::verify_battery([](charger::ChargeResult) {}));
	}
	if (args.battery_test->count) {
		return charge_command(true, [](charger::ChargeCompletion done) {
			return charger::battery_test(done);
		});
	}
	if (args.enable_charge->count || args.disable_charge->count) {
		const bool enabled = args.enable_charge->count;
		return charge_command(false, [enabled](charger::ChargeCompletion done) {
			return charger::set_charge_enabled(enabled, done);
		});
	}
	const auto state = charger::State::copy();
	std::printf("%-20s %s\n", "Battery",
				battery_detected_name(state.battery()));
	if (state.battery_detection_vbat_valid) {
		std::printf("%-20s %u mV\n", "Detection VBAT",
					state.battery_detection_vbat_mv);
	} else {
		std::printf("%-20s UNKNOWN\n", "Detection VBAT");
	}
	if (state.adc_valid) {
		std::printf(
			"%-20s %+ld mW\n", "Battery power",
			long(state.readings.ibat_ma) * long(state.readings.vbat_mv) / 1000);
		const auto gauge = battery::State::copy();
		const unsigned capacity_mah = settings::copy().battery_mah;
		if (state.readings.ibat_ma < 0 && gauge.status == Status::ok &&
			gauge.soc >= 0 && gauge.soc <= 100) {
			char remaining[16];
			format_hours_minutes(
				gauge.soc / 100 * static_cast<float>(capacity_mah) /
					static_cast<float>(-state.readings.ibat_ma),
				remaining, sizeof remaining);
			std::printf("%-20s %s to EMPTY (%u mAh, SOC %.0f %%)\n",
						"Remaining", remaining, capacity_mah, gauge.soc);
		}
	}
	command_status::print_status(state.readings.status, state.updated_us,
								 state.last_error, state.error_us);
	if (!state.updated_us) {
		return 0;
	}
	const auto* raw = state.readings.raw;
	static const char* const charge[] = {
		"NOT CHARGING",     "TRICKLE",  "PRECHARGE", "CONSTANT CURRENT",
		"CONSTANT VOLTAGE", "RESERVED", "TOP-OFF",   "DONE"};
	static const char* const vbus[] = {"NO INPUT",
									   "USB SDP",
									   "USB CDP",
									   "USB DCP",
									   "HVDCP",
									   "UNKNOWN ADAPTER",
									   "NON-STANDARD ADAPTER",
									   "OTG",
									   "UNQUALIFIED ADAPTER",
									   "RESERVED",
									   "RESERVED",
									   "DIRECT VBUS",
									   "BACKUP",
									   "RESERVED",
									   "RESERVED",
									   "RESERVED"};
	control(state, vbus);
	std::printf("%-20s %s (0x%02X)\n", "VBUS (REG1B)",
				state.readings.status0.vbus ? "PRESENT" : "ABSENT", raw[0x1b]);
	std::printf("%-20s %s (0x%02X)\n", "VBUS type (REG1C)",
				vbus[state.readings.status1.vbus], raw[0x1c]);
	std::printf("%-20s %s (0x%02X)\n", "Charge (REG1C)",
				charge[state.readings.status1.charge], raw[0x1c]);
	fault(state.readings.status3.charge_timer, "Charge timer", "REG1E",
		  raw[0x1e]);
	fault(state.readings.status3.trickle_timer, "Trickle timer", "REG1E",
		  raw[0x1e]);
	fault(state.readings.status3.precharge_timer, "Precharge timer", "REG1E",
		  raw[0x1e]);
	fault(state.readings.status4.otg_low, "OTG battery low", "REG1F",
		  raw[0x1f]);
	fault(state.readings.status4.cold, "TS cold", "REG1F", raw[0x1f]);
	fault(state.readings.status4.cool, "TS cool", "REG1F", raw[0x1f]);
	fault(state.readings.status4.warm, "TS warm", "REG1F", raw[0x1f]);
	fault(state.readings.status4.hot, "TS hot", "REG1F", raw[0x1f]);
	faults(state.readings.fault0, "REG20", raw[0x20]);
	faults(state.readings.fault_flag0, "REG26", raw[0x26]);
	faults(state.readings.fault1, "REG21", raw[0x21]);
	faults(state.readings.fault_flag1, "REG27", raw[0x27]);
	fault(state.readings.flag0.poor_source, "Poor source", "REG22", raw[0x22]);
	adc(state, "VBUS", "REG35", 0x35, 5, state.readings.vbus_mv, "mV", 0);
	adc(state, "IBUS", "REG31", 0x31, 7, state.readings.ibus_ma, "mA", 0);
	adc(state, "VBAT", "REG3B", 0x3b, 4, state.readings.vbat_mv, "mV", 0);
	adc(state, "IBAT", "REG33", 0x33, 6, state.readings.ibat_ma, "mA", 0);
	adc(state, "VSYS", "REG3D", 0x3d, 3, state.readings.vsys_mv, "mV", 0);
	adc(state, "TS", "REG3F", 0x3f, 2, state.ts_percent, "%", 2);
	adc(state, "TDIE", "REG41", 0x41, 1, state.die_c, "C", 1);
	std::printf("%-20s %u mA (0x%02X%02X)\n", "Charge limit (ICHG)",
				state.readings.charge_current_ma, raw[3], raw[4]);
	std::printf("%-20s %u mA (0x%02X%02X)\n", "Input limit (IINDPM)",
				state.readings.input_current_ma, raw[6], raw[7]);
	std::printf("%-20s %u C (0x%02X)\n", "Thermal limit (TREG)",
				state.readings.thermal_limit_c, raw[0x16]);
	std::printf("%-20s %u ms (0x%02X)\n", "Watchdog (REG10)",
				state.readings.control1.watchdog_ms, raw[0x10]);
	std::printf("%-20s %s (0x%02X)\n", "WD_STAT (REG1B)",
				state.readings.status0.watchdog ? "EXPIRED" : "NORMAL",
				raw[0x1b]);
	std::printf("%-20s %s (0x%02X)\n", "EN_HIZ (REG0F)",
				state.readings.control0.high_impedance ? "ON" : "OFF",
				raw[0x0f]);
	std::printf("%-20s %s (0x%02X)\n", "EN_EXTILIM (REG14)",
				state.readings.control5.external_limit ? "ON" : "OFF",
				raw[0x14]);
	std::printf("%-20s %s (0x%02X)\n", "SFET_PRESENT (REG14)",
				state.readings.control5.ship_fet ? "PRESENT" : "ABSENT",
				raw[0x14]);
	return 0;
}
}

extern "C" esp_err_t console_register_charger(void) {
	args.battery_test = arg_lit0(nullptr, "battery-test",
								 "Measure VBAT before and after 1 s with "
								 "charging disabled, then restore EN_CHG");
	args.verify_battery = arg_lit0(nullptr, "verify-battery",
								   "Run the battery test and record NO for a "
								   "drop greater than 1500 mV, YES otherwise");
	args.enable_charge =
		arg_lit0(nullptr, "enable-charge",
				 "Set EN_CHG without changing other configuration");
	args.disable_charge =
		arg_lit0(nullptr, "disable-charge",
				 "Clear EN_CHG without changing other configuration");
	args.mode = arg_str0(nullptr, "mode", "AUTO|MANUAL",
						 "AUTO applies charge control, MANUAL leaves the "
						 "charger to these options until restart");
	args.iindpm =
		arg_int0(nullptr, "iindpm", "MA",
				 "Set the input limit in MANUAL, at most the hardware "
				 "input limit while it is on");
	args.ichg =
		arg_int0(nullptr, "ichg", "MA", "Set the charge current in MANUAL");
	args.end = arg_end(2);
	args.table[0] = args.battery_test;
	args.table[1] = args.verify_battery;
	args.table[2] = args.enable_charge;
	args.table[3] = args.disable_charge;
	args.table[4] = args.mode;
	args.table[5] = args.iindpm;
	args.table[6] = args.ichg;
	args.table[7] = args.end;
	if (arg_nullcheck(args.table)) {
		return ESP_ERR_NO_MEM;
	}
	const esp_console_cmd_t entry = {
		.command = "charger",
		.help =
			"Show charger status, control charging or measure battery "
			"voltage decay.",
		.hint =
			"[--mode AUTO|MANUAL | --iindpm MA | --ichg MA | --battery-test | "
			"--verify-battery | --enable-charge | --disable-charge]",
		.func = command,
		.argtable = static_cast<void*>(args.table),
		.func_w_context = nullptr,
		.context = nullptr,
	};
	return console_register(&entry);
}
