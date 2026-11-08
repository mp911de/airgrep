#include "console.h"
#include "adsbee/adsbee_clock.hpp"
#include <cstdio>
#include <ctime>
#include <sys/time.h>
#include "argtable.hpp"
#include "esp_console.h"
#include "esp_timer.h"
#include "command_status.hpp"

namespace {
	struct { struct arg_lit* sync; struct arg_end* end; } args;

	int command(int argc, char** argv) {
		if (arg_parse(argc, argv, reinterpret_cast<void**>(&args))) {
			arg_print_errors(stderr, args.end, argv[0]);
			return 1;
		}
		if (args.sync->count) return command_status::ack(adsbee::timesync::sync());
		const auto state = adsbee::timesync::State::copy();
		timeval tv;
		gettimeofday(&tv, nullptr);
		tm utc;
		gmtime_r(&tv.tv_sec, &utc);
		char text[32];
		std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &utc);
		std::printf("%-20s %s UTC%s\n", "Time", text, state.set ? "" : ", UNSET");
		int64_t now = esp_timer_get_time();
		if (state.set)
			std::printf("%-20s GNSS time ping %lld s ago, correction %+lld ms, RTT %lld ms, PPS %lu\n", "Source",
				(long long)((now - state.synced_us) / 1000000), (long long)(state.correction_us / 1000),
				(long long)(state.rtt_us / 1000), (unsigned long)state.pps);
		else std::printf("%-20s NONE\n", "Source");
		std::printf("%-20s %s, %u replies in the last burst\n", "Sync", state.running ? "RUNNING" : "IDLE", state.answered);
		if (state.last_error != ESP_OK)
			std::printf("%-20s %s, %lld s ago\n", "Last error", esp_err_to_name(state.last_error),
				(long long)((now - state.error_us) / 1000000));
		return 0;
	}
}

extern "C" esp_err_t console_register_clock(void) {
	args.sync = arg_lit0(nullptr, "sync", "Start a time ping burst now");
	args.end = arg_end(2);
	if (arg_nullcheck(reinterpret_cast<void**>(&args))) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t entry = {
		.command = "clock", .help = "Show the system clock and its GNSS sync, or sync now.",
		.hint = "[--sync]", .func = command, .argtable = &args,
		.func_w_context = nullptr, .context = nullptr,
	};
	return console_register(&entry);
}
