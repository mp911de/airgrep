#include <cstdio>
#include <cstring>

#include "adsbee/adsbee_link.hpp"
#include "argtable.hpp"
#include "command_status.hpp"
#include "console.h"
#include "esp_console.h"
#include "esp_timer.h"

namespace {
struct {
	struct arg_str* at;
	struct arg_end* end;
	void* table[2];
} args;
int command(int argc, char** argv) {
	if (arg_parse(argc, argv, args.table)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	if (args.at->count) {
		const loop::Handle handle =
			adsbee::request(args.at->sval[0], {}, {}, ADSBEE_REPLY_WINDOW_MS);
		return command_status::ack(
			handle.valid() ? ESP_OK : adsbee::State::copy().last_error);
	}
	const auto state = adsbee::State::copy();
	const int64_t now = esp_timer_get_time();
	if (state.updated_us) {
		std::printf("%-20s %lld ms ago\n", "Updated",
					(long long)((now - state.updated_us) / 1000));
	}
	std::printf("%-20s %s\n%-20s %s\n%-20s %s\n", "Status",
				state.known ? "OK" : "UNKNOWN", "TX",
				state.tx_attached ? "ATTACHED" : "DETACHED", "Init",
				state.initialized ? "DONE" : "PENDING");
	if (state.reply_us) {
		std::printf("%-20s %lld ms\n", "AT age",
					(long long)((now - state.reply_us) / 1000));
	} else {
		std::printf("%-20s UNKNOWN\n", "AT age");
	}
	if (state.last_error != ESP_OK) {
		std::printf("%-20s %s, %lld ms ago\n", "Last error",
					esp_err_to_name(state.last_error),
					(long long)((now - state.error_us) / 1000));
	}
	std::printf("%-20s %lu\n%-20s %lu\n%-20s %lu\n%-20s %lu\n%-20s %lu\n",
				"CSBEE lines", (unsigned long)state.telemetry_lines,
				"CRC failures", (unsigned long)state.crc_failures,
				"Parity errors", (unsigned long)state.parity_errors,
				"Framing errors", (unsigned long)state.framing_errors,
				"Overruns", (unsigned long)state.overruns);
	if (state.last_gnss_report_us) {
		const auto& gnss = state.gnss_at(now);
		std::printf("%-20s %s, %d used, %lld s ago\n", "GNSS",
					gnss.fix != adsbee::Fix::none ? "VALID" : "NONE",
					gnss.satellites_used,
					(long long)((now - state.last_gnss_report_us) / 1000000));
	} else {
		std::printf("%-20s UNKNOWN\n", "GNSS");
	}
	static char device_info[2048];
	if (adsbee::device_info(device_info, sizeof device_info)) {
		std::printf("%-20s\n%s", "Device info", device_info);
	} else {
		std::printf("%-20s UNKNOWN\n", "Device info");
	}
	return 0;
}
}

extern "C" esp_err_t console_register_adsbee(void) {
	args.at = arg_str0(nullptr, "at", "cmd",
					   "Send one AT command, the reply goes to the debug log");
	args.end = arg_end(2);
	args.table[0] = args.at;
	args.table[1] = args.end;
	if (arg_nullcheck(args.table)) {
		return ESP_ERR_NO_MEM;
	}
	const esp_console_cmd_t entry = {
		.command = "adsbee",
		.help = "Show ADSBee link state or send an AT command.",
		.hint = "[--at <cmd>]",
		.func = command,
		.argtable = static_cast<void*>(args.table),
		.func_w_context = nullptr,
		.context = nullptr,
	};
	return console_register(&entry);
}
