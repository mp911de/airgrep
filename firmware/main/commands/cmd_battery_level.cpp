#include "console.h"
#include "battery/max17048.hpp"
#include <cstdio>
#include "esp_console.h"
#include "command_status.hpp"

namespace {
	int command(int argc, char**) {
		if (argc != 1) { std::puts("usage: battery_level"); return 1; }
		const auto state = battery::State::copy();
		if (!command_status::header(state.status, state.updated_us, state.last_error, state.error_us)) return 0;
		std::printf("%-20s %.2f %% (0x%04X)\n", "Charge (SOC)", state.soc, state.raw_soc);
		std::printf("%-20s %.4f V (0x%04X)\n", "Voltage (VCELL)", state.volts, state.raw_vcell);
		std::printf("%-20s %+.3f %%/h (0x%04X)\n", "Rate (CRATE)", state.rate, state.raw_crate);
		if (state.remaining.valid)
			std::printf("%-20s %.2f h to %s\n", "Remaining", state.remaining.hours,
				state.rate < 0 ? "EMPTY" : "FULL");
		const struct { const char* name; bool active; } alerts[] = {
			{"RI reset indicator", state.alerts.reset},
			{"VH voltage high", state.alerts.voltage_high},
			{"VL voltage low", state.alerts.voltage_low},
			{"VR voltage reset", state.alerts.voltage_reset},
			{"HD low state of charge", state.alerts.low_soc},
			{"SC state of charge changed", state.alerts.soc_change},
		};
		for (const auto& alert : alerts)
			if (alert.active) std::printf("%-20s %s (0x%04X)\n", "Alert (STATUS)", alert.name, state.raw_status);
		std::printf("%-20s 0x%04X\n", "Version", state.raw_version);
		return 0;
	}
}

extern "C" esp_err_t console_register_battery_level(void) {
	const esp_console_cmd_t entry = {
		.command = "battery_level", .help = "Show battery charge, voltage, rate, remaining time and alerts.",
		.hint = "", .func = command, .argtable = nullptr,
		.func_w_context = nullptr, .context = nullptr,
	};
	return console_register(&entry);
}
