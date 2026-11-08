#include "console.h"
#include "adsbee/adsbee_clock.hpp"
#include "settings/settings.hpp"
#include "tz/tz.hpp"
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
		if (args.sync->count) return command_status::ack(
			adsbee::timesync::schedule_nss_time_sync());
		const auto state = adsbee::timesync::State::copy();
		int64_t now = esp_timer_get_time();
		timeval tv;
		gettimeofday(&tv, nullptr);
		tm utc;
		gmtime_r(&tv.tv_sec, &utc);
		char text[32];
		std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &utc);
		std::printf("%-20s %s UTC\n", "Time", text);
		tm local;
		localtime_r(&tv.tv_sec, &local);
		std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S %Z", &local);
		std::printf("%-20s %s\n", "Local", text);
		const auto zone = tz::State::copy();
		const auto current = settings::copy();
		if (!current.tz_detect) std::printf("%-20s %s (tz_rule)\n", "Zone", zone.rule);
		else if (zone.detected)
			std::printf("%-20s %s (detected %lld s ago), %s\n", "Zone", zone.zone,
						(long long)((now - zone.detected_us) / 1000000), zone.rule);
		else
			std::printf("%-20s UTC (not detected, table %s)\n", "Zone",
						zone.status == Status::ok ? "OK" : "OFF");
		if (zone.status == Status::ok)
			std::printf("%-20s %s, %u zones, rules for %u, %lu bytes\n", "Table",
						zone.source, zone.zones,
						zone.rule_year, (unsigned long)zone.table_bytes);
		else std::printf("%-20s OFF\n", "Table");
		// The RTC keeps the time across resets, so without a schedule_nss_time_sync this boot the time is still
		// the last one it was set to.
		if (state.set) {
			time_t synced_at = tv.tv_sec - time_t((now - state.synced_us) / 1000000);
			gmtime_r(&synced_at, &utc);
			std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &utc);
			std::printf(
				"%-20s GNSS, synced %s UTC, correction %+lld ms, RTT %lld ms, PPS %lu\n",
				"Source",
				text, (long long)(state.correction_us / 1000),
				(long long)(state.rtt_us / 1000),
				(unsigned long)state.pps);
		}
		else std::printf("%-20s RTC\n", "Source");
		if (state.last_error != ESP_OK)
			std::printf("%-20s %s, %lld s ago\n", "Last error", esp_err_to_name(state.last_error),
				(long long)((now - state.error_us) / 1000000));
		return 0;
	}
}

extern "C" esp_err_t console_register_clock(void) {
	args.sync = arg_lit0(nullptr, "schedule_nss_time_sync",
						"Start a time ping burst now");
	args.end = arg_end(2);
	if (arg_nullcheck(reinterpret_cast<void**>(&args))) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t entry = {
		.command = "clock", .help = "Show the system clock and its GNSS time sync.",
		.hint = "[--sync]", .func = command, .argtable = &args,
		.func_w_context = nullptr, .context = nullptr,
	};
	return console_register(&entry);
}
