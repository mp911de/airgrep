#include <cstdio>

#include "argtable.hpp"
#include "command_status.hpp"
#include "console.h"
#include "esp_console.h"
#include "usb/fusb303.hpp"

namespace {
struct {
	struct arg_lit* reset;
	struct arg_end* end;
	void* table[2];
} args;

int command(int argc, char** argv) {
	if (arg_parse(argc, argv, args.table)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	if (args.reset->count) {
		return command_status::ack(fusb::reset());
	}
	const auto state = fusb::State::copy();
	command_status::print_status(state.status, state.updated_us,
								 state.last_error, state.error_us);
	if (!state.updated_us) {
		return 0;
	}
	static const char* const orientations[] = {"UNRESOLVED", "CC1", "CC2",
											   "FAULT"};
	static const char* const roles[] = {
		"NONE",         "SINK",         "SOURCE",
		"ACTIVE CABLE", "AUDIO",        "AUDIO WITH VBUS",
		"DEBUG SINK",   "DEBUG SOURCE", "INVALID"};
	std::printf("%-20s %s (0x%02X)\n", "Attached (STATUS)",
				state.port.attached ? "ATTACHED" : "DETACHED",
				state.raw_status);
	std::printf("%-20s %s (0x%02X)\n", "Orientation (STATUS)",
				orientations[state.port.orientation], state.raw_status);
	std::printf("%-20s %s (0x%02X)\n", "Role (TYPE)",
				roles[unsigned(state.role)], state.raw_type);
	if (state.port.advertised_ma) {
		std::printf("%-20s %u mA (0x%02X)\n", "Advertised (STATUS)",
					state.port.advertised_ma, state.raw_status);
	} else {
		std::printf("%-20s %s (0x%02X)\n", "Advertised (STATUS)",
					state.port.default_current ? "DEFAULT" : "NONE",
					state.raw_status);
	}
	std::printf("%-20s %s (0x%02X)\n", "VBUS (STATUS)",
				state.port.vbus ? "VALID" : "NONE", state.raw_status);
	if (state.cc_limit_ma) {
		std::printf("%-20s %u mA (0x%02X, 0x%02X)\n", "CC limit",
					state.cc_limit_ma, state.raw_status, state.raw_type);
	} else {
		std::printf("%-20s DEFAULT, charger BC1.2 decides (0x%02X, 0x%02X)\n",
					"CC limit", state.raw_status, state.raw_type);
	}
	std::printf("%-20s %s (0x%02X)\n", "Enabled (CONTROL1)",
				state.control.enabled ? "ENABLED" : "DISABLED",
				state.raw_control1);
	std::printf("%-20s version %u, revision %u (0x%02X)\n", "Device ID",
				state.identity.version, state.identity.revision,
				state.raw_identity);
	return 0;
}
}

extern "C" esp_err_t console_register_fusb(void) {
	args.reset = arg_lit0(nullptr, "reset",
						  "Reset and re-enable on the next poll after tEN");
	args.end = arg_end(2);
	args.table[0] = args.reset;
	args.table[1] = args.end;
	if (arg_nullcheck(args.table)) {
		return ESP_ERR_NO_MEM;
	}
	const esp_console_cmd_t entry = {
		.command = "fusb",
		.help =
			"Show USB-C attachment and current advertisement or reset the "
			"controller.",
		.hint = "[--reset]",
		.func = command,
		.argtable = static_cast<void*>(args.table),
		.func_w_context = nullptr,
		.context = nullptr,
	};
	return console_register(&entry);
}
