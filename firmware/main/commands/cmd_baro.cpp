#include "console.h"
#include "baro/barometer.hpp"
#include <cmath>
#include <cstdio>
#include "argtable.hpp"
#include "esp_console.h"
#include "command_status.hpp"

namespace {
	struct { struct arg_lit* reset; struct arg_end* end; } args;
	unsigned raw24(const uint8_t* bytes) {
		return unsigned(bytes[0]) | (unsigned(bytes[1]) << 8) | (unsigned(bytes[2]) << 16);
	}
	int command(int argc, char** argv) {
		if (arg_parse(argc, argv, reinterpret_cast<void**>(&args))) {
			arg_print_errors(stderr, args.end, argv[0]);
			return 1;
		}
		if (args.reset->count) {
			return command_status::ack(barometer::reset());
		}
		const auto state = barometer::State::copy();
		if (!command_status::header(state.status, state.updated_us, state.last_error, state.error_us)) return 0;
		std::printf("%-20s %.3f hPa (0x%06X)\n", "Pressure",
			state.pressure_hpa, raw24(state.data + barometer::kDataPressureOffset));
		std::printf("%-20s %.3f C (0x%06X)\n", "Temperature",
			state.temperature_c, raw24(state.data + barometer::kDataTemperatureOffset));
		if (std::isfinite(state.altitude_m))
			std::printf("%-20s %.1f m / %.0f ft (1013.25 hPa)\n", "Altitude",
				state.altitude_m, state.altitude_m / 0.3048f);
		else std::printf("%-20s UNAVAILABLE\n", "Altitude");
		static const char* modes[] = {"STANDBY", "NORMAL", "FORCED", "CONTINUOUS"};
		std::printf("%-20s %s (0x%02X)\n", "Power mode (ODR)", modes[state.odr.mode], state.raw_odr);
		std::printf("%-20s %ux pressure, %ux temperature (0x%02X)\n", "Oversampling (OSR)",
			state.osr.pressure, state.osr.temperature, state.raw_osr);
		std::printf("%-20s %u pressure, %u temperature (0x%02X)\n", "IIR (DSP_IIR)",
			state.iir.pressure, state.iir.temperature, state.raw_iir);
		std::printf("%-20s 0x%02X\n", "Chip ID", state.chip_id);
		return 0;
	}
}

extern "C" esp_err_t console_register_baro(void) {
	args.reset = arg_lit0(nullptr, "reset", "Soft reset and reconfigure on the next poll");
	args.end = arg_end(2);
	if (arg_nullcheck(reinterpret_cast<void**>(&args))) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t entry = {
		.command = "baro", .help = "Show barometer readings and configuration or reset the sensor.",
		.hint = "[--reset]", .func = command, .argtable = &args,
		.func_w_context = nullptr, .context = nullptr,
	};
	return console_register(&entry);
}
