#include "console.h"
#include <cstdio>
#include "argtable.hpp"
#include "esp_console.h"
#include "i2c_expander/tca9534.hpp"
#include "command_status.hpp"

namespace {
	struct { struct arg_int* trigger; struct arg_end* end; } args;

	int command(int argc, char** argv) {
		if (arg_parse(argc, argv, reinterpret_cast<void**>(&args))) {
			arg_print_errors(stderr, args.end, argv[0]);
			return 1;
		}
		if (args.trigger->count) {
			return command_status::ack(expander::trigger(args.trigger->ival[0]));
		}
		const auto state = expander::State::copy();
		if (!command_status::header(state.status, state.updated_us, state.last_error, state.error_us)) return 0;
		static const char* names[] = {"Buzzer Mute", "WLAN", "M10S Reset", "M10S Safeboot",
			"Display Reset 1", "Display Reset 2", "Backlight", "Power"};
		static const char* signals[] = {"BUZZ_ENABLE", "WIFI_ENABLE", "RESET_N", "SAFEBOOT_N",
			"RST_1", "RST_2", "LCD_LED", "PWR_ON"};
		for (unsigned pin = 0; pin < 8; pin++) {
			bool high = state.input.high[pin];
			const char* direction = state.configuration.input[pin] ? "input" : "output";
			const char* meaning;
			if (pin == 0) meaning = state.input.mute ? "OFF (muted)" : "ON";
			else if (pin == 1) meaning = state.input.wifi ? "ON" : "OFF";
			else if (pin == 7) meaning = state.input.power ? "ON" : "OFF";
			else if (pin == 6) meaning = high ? "ON" : "OFF";
			else meaning = high ? "RELEASED" : "HELD";
			int written = std::printf("P%u %s", pin, signals[pin]);
			std::printf("%*s%s %s, %s (%s)\n", written < 20 ? 21 - written : 1, "", direction,
				high ? "high" : "low", meaning, names[pin]);
		}
		std::printf("%-20s 0x%02X\n%-20s 0x%02X\n%-20s 0x%02X\n", "Input (INPUT_PORT)", state.raw_input,
			"Output (OUTPUT_PORT)", state.raw_output, "Config (CONFIG)", state.raw_configuration);
		if (state.raw_polarity) std::printf("%-20s 0x%02X\n", "Polarity (POLARITY)", state.raw_polarity);
		return 0;
	}
}

extern "C" esp_err_t console_register_exp(void) {
	args.trigger = arg_int0(nullptr, "trigger", "N", "Pulse control pin P2 to P5 for 10 ms");
	args.end = arg_end(2);
	if (arg_nullcheck(reinterpret_cast<void**>(&args))) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t entry = {
		.command = "exp", .help = "Show expander pins or pulse a reset control.",
		.hint = "[--trigger N]", .func = command, .argtable = &args,
		.func_w_context = nullptr, .context = nullptr,
	};
	return console_register(&entry);
}
