#include <cstdio>

#include "argtable.hpp"
#include "command_status.hpp"
#include "console.h"
#include "esp_console.h"
#include "esp_timer.h"
#include "wifi/wifi_ap.hpp"

namespace {
struct {
	struct arg_lit* apply;
	struct arg_lit* poll;
	struct arg_end* end;
	void* table[3];
} args;

int command(int argc, char** argv) {
	if (arg_parse(argc, argv, args.table)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	if (args.apply->count) {
		return command_status::ack(wifi::apply_wifi_state());
	}
	if (args.poll->count) {
		return command_status::ack(wifi::query_access_point());
	}
	const auto state = wifi::State::copy();
	const int64_t now = esp_timer_get_time();
	std::printf("%-20s %s\n", "Status",
				state.status == Status::ok ? "OK" : "OFF");
	if (state.last_error != ESP_OK) {
		std::printf("%-20s %s, %lld s ago\n", "Last error",
					esp_err_to_name(state.last_error),
					(long long)((now - state.error_us) / 1000000));
	}
	if (state.slider_sampled) {
		std::printf("%-20s %s\n", "Switch",
					state.slider_enabled ? "ON" : "OFF");
	} else {
		std::printf("%-20s UNKNOWN\n", "Switch");
	}
	std::printf("%-20s %s\n", "Pending apply",
				state.set_pending ? "YES" : "NO");
	if (state.reported_known) {
		const wifi::Config& c = state.last_reported_config;
		std::printf("%-20s %s, %s, channel %u (%lld s ago)\n", "ADSBee",
					c.enabled ? "ON" : "OFF", c.ssid, unsigned(c.channel),
					(long long)((now - state.reported_us) / 1000000));
	} else {
		std::printf("%-20s UNKNOWN, no reply yet\n", "ADSBee");
	}
	if (state.sent) {
		std::printf("%-20s %lu commands, last %lld s ago, %lu mismatches\n",
					"Sent", (unsigned long)state.sent,
					(long long)((now - state.sent_us) / 1000000),
					(unsigned long)state.mismatches);
	} else {
		std::printf("%-20s NONE\n", "Sent");
	}
	return 0;
}
}

extern "C" esp_err_t console_register_wifi(void) {
	args.apply = arg_lit0(nullptr, "apply",
						  "Send the wanted Wi-Fi state to the ADSBee now");
	args.poll = arg_lit0(nullptr, "poll", "Ask the ADSBee what it runs");
	args.end = arg_end(2);
	args.table[0] = args.apply;
	args.table[1] = args.poll;
	args.table[2] = args.end;
	if (arg_nullcheck(args.table)) {
		return ESP_ERR_NO_MEM;
	}
	const esp_console_cmd_t entry = {
		.command = "wifi",
		.help = "Show the Wi-Fi slider and the ADSBee access point, or resend.",
		.hint = "[--apply] [--poll]",
		.func = command,
		.argtable = static_cast<void*>(args.table),
		.func_w_context = nullptr,
		.context = nullptr,
	};
	return console_register(&entry);
}
