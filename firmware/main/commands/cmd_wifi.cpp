#include "console.h"
#include "wifi/wifi_ap.hpp"
#include <cstdio>
#include "argtable.hpp"
#include "command_status.hpp"
#include "esp_console.h"
#include "esp_timer.h"

namespace {
	struct { struct arg_lit* apply; struct arg_lit* poll; struct arg_end* end; } args;

	int command(int argc, char** argv) {
		if (arg_parse(argc, argv, reinterpret_cast<void**>(&args))) {
			arg_print_errors(stderr, args.end, argv[0]);
			return 1;
		}
		if (args.apply->count) return command_status::ack(wifi::apply());
		if (args.poll->count) return command_status::ack(wifi::poll());
		const auto state = wifi::State::copy();
		int64_t now = esp_timer_get_time();
		std::printf("%-20s %s\n", "Status", state.status == Status::ok ? "OK" : "OFF");
		if (state.last_error != ESP_OK)
			std::printf("%-20s %s, %lld s ago\n", "Last error", esp_err_to_name(state.last_error),
						(long long)((now - state.error_us) / 1000000));
		if (state.switch_known) std::printf("%-20s %s\n", "Switch", state.wanted ? "ON" : "OFF");
		else std::printf("%-20s UNKNOWN\n", "Switch");
		if (state.reported_known) {
			const wifi::Config& c = state.reported;
			std::printf("%-20s %s, %s, channel %u (%lld s ago)\n", "ADSBee", c.enabled ? "ON" : "OFF",
						c.ssid, unsigned(c.channel), (long long)((now - state.reported_us) / 1000000));
		}
		else std::printf("%-20s UNKNOWN, no reply yet\n", "ADSBee");
		if (state.sent)
			std::printf("%-20s %lu commands, last %lld s ago, %lu mismatches\n", "Sent",
						(unsigned long)state.sent, (long long)((now - state.sent_us) / 1000000),
						(unsigned long)state.mismatches);
		else std::printf("%-20s NONE\n", "Sent");
		return 0;
	}
}

extern "C" esp_err_t console_register_wifi(void) {
	args.apply = arg_lit0(nullptr, "apply", "Send the slider position to the ADSBee now");
	args.poll = arg_lit0(nullptr, "poll", "Ask the ADSBee what it runs");
	args.end = arg_end(2);
	if (arg_nullcheck(reinterpret_cast<void**>(&args))) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t entry = {
		.command = "wifi", .help = "Show the Wi-Fi slider and the ADSBee access point, or resend.",
		.hint = "[--apply] [--poll]", .func = command, .argtable = &args,
		.func_w_context = nullptr, .context = nullptr,
	};
	return console_register(&entry);
}
