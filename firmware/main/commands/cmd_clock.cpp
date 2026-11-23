#include <sys/time.h>

#include <cstdio>
#include <ctime>

#include "adsbee/adsbee_clock.hpp"
#include "argtable.hpp"
#include "command_status.hpp"
#include "console.h"
#include "esp_console.h"
#include "esp_timer.h"
#include "settings/settings.hpp"
#include "tz/tz.hpp"

namespace {
struct {
	struct arg_lit* sync;
	struct arg_end* end;
	void* table[2];
} args;

int command(int argc, char** argv) {
	if (arg_parse(argc, argv, args.table)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	if (args.sync->count) {
		return command_status::ack(adsbee::timesync::schedule_gnss_time_sync());
	}
	const auto state = adsbee::timesync::State::copy();
	const int64_t now = esp_timer_get_time();
	timeval tv{};
	gettimeofday(&tv, nullptr);
	tm utc{};
	gmtime_r(&tv.tv_sec, &utc);
	char text[32];
	(void)std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &utc);
	std::printf("%-20s %s UTC\n", "Time", text);
	tm local{};
	localtime_r(&tv.tv_sec, &local);
	(void)std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S %Z", &local);
	std::printf("%-20s %s\n", "Local", text);
	const auto zone = tz::State::copy();
	const auto current = settings::copy();
	if (!current.tz_detect) {
		std::printf("%-20s %s (chosen), %s\n", "Zone",
					current.zone[0] ? current.zone : "UTC",
					zone.applied_posix_rule);
	} else if (zone.has_detected_zone) {
		std::printf("%-20s %s (detected %lld s ago), %s\n", "Zone",
					zone.detected_iana_zone,
					(long long)((now - zone.last_zone_match_us) / 1000000),
					zone.applied_posix_rule);
	} else {
		std::printf(
			"%-20s UTC (not detected, table %s)\n", "Zone",
			zone.table_status == tz::TableStatus::available ? "OK" : "OFF");
	}
	if (zone.table_status == tz::TableStatus::available) {
		std::printf("%-20s %s, %u zones, rules for %u, %lu bytes\n", "Table",
					zone.source, zone.zones, zone.rule_year,
					(unsigned long)zone.table_bytes);
	} else {
		std::printf("%-20s OFF\n", "Table");
	}
	// The RTC keeps the time across resets, so without a
	// sync this boot the time is still the last one it was
	// set to.
	if (state.clock_set_from_gnss) {
		const time_t synced_at =
			tv.tv_sec - time_t((now - state.last_sync_us) / 1000000);
		gmtime_r(&synced_at, &utc);
		(void)std::strftime(text, sizeof text, "%Y-%m-%d %H:%M:%S", &utc);
		std::printf(
			"%-20s GNSS, synced %s UTC, correction %+lld ms, RTT %lld "
			"ms, PPS %lu\n",
			"Source", text, (long long)(state.last_correction_us / 1000),
			(long long)(state.rtt_us / 1000), (unsigned long)state.pps);
	} else {
		std::printf("%-20s RTC\n", "Source");
	}
	if (state.last_error != ESP_OK) {
		std::printf("%-20s %s, %lld s ago\n", "Last error",
					esp_err_to_name(state.last_error),
					(long long)((now - state.error_us) / 1000000));
	}
	return 0;
}
}

extern "C" esp_err_t console_register_clock(void) {
	args.sync = arg_lit0(nullptr, "sync", "Start a time ping burst now");
	args.end = arg_end(2);
	args.table[0] = args.sync;
	args.table[1] = args.end;
	if (arg_nullcheck(args.table)) {
		return ESP_ERR_NO_MEM;
	}
	const esp_console_cmd_t entry = {
		.command = "clock",
		.help = "Show the system clock and its GNSS time sync.",
		.hint = "[--sync]",
		.func = command,
		.argtable = static_cast<void*>(args.table),
		.func_w_context = nullptr,
		.context = nullptr,
	};
	return console_register(&entry);
}
