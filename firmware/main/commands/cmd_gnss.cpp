#include <cstdio>
#include <cstring>

#include "adsbee/adsbee_link.hpp"
#include "console.h"
#include "esp_console.h"
#include "esp_timer.h"
#include "navigation/units.h"

namespace {

const char* pick(const char* const* names, int count, auto field) {
	const int code = static_cast<int>(field);
	return code < 0 ? "UNKNOWN" : code < count ? names[code] : "?";
}

void count(char* out, size_t capacity, int value, const char* label,
		   bool last) {
	if (value < 0) {
		(void)std::snprintf(out, capacity, "- %s%s", label, last ? "" : ", ");
	} else {
		(void)std::snprintf(out, capacity, "%d %s%s", value, label,
							last ? "" : ", ");
	}
}

int command(int argc, char**) {
	if (argc != 1) {
		std::puts("usage: gnss");
		return 1;
	}
	const auto state = adsbee::State::copy();
	if (!state.last_gnss_report_us) {
		std::printf("%-20s UNKNOWN, no GNSS frame received\n", "Status");
		return 0;
	}
	const int64_t now = esp_timer_get_time();
	const adsbee::Gnss& g = state.gnss_at(now);
	std::printf("%-20s %lld ms ago\n", "Updated",
				(long long)((now - state.last_gnss_report_us) / 1000));
	static const char* const qualities[] = {"NONE", "GPS", "DGPS/SBAS"};
	const bool fix = g.fix != adsbee::Fix::none;
	std::printf("%-20s %s, %s", "Fix", fix ? "VALID" : "NONE",
				pick(qualities, 3, g.quality));
	if (g.satellites_used >= 0) {
		std::printf(", %d used", g.satellites_used);
	}
	if (g.hdop >= 0) {
		std::printf(", HDOP %.2f", g.hdop);
	}
	std::puts("");
	if (fix) {
		std::printf("%-20s %.6f %c, %.6f %c\n", "Position",
					g.latitude < 0 ? -g.latitude : g.latitude,
					g.latitude < 0 ? 'S' : 'N',
					g.longitude < 0 ? -g.longitude : g.longitude,
					g.longitude < 0 ? 'W' : 'E');
		std::printf("%-20s %ld ft / %.0f m\n", "Altitude", (long)g.altitude_ft,
					g.altitude_ft * units::metres_per_foot);
		std::printf("%-20s %.0f kts / %.1f km/h, track %.1f deg\n", "Speed",
					g.speed_kt,
					g.speed_kt * (units::metres_per_nautical_mile / 1000.0),
					g.track_deg);
	}
	if (g.utc_known) {
		std::printf("%-20s %02u:%02u:%02u UTC, epoch %llu ms\n", "Time",
					g.utc_hour, g.utc_minute, g.utc_second,
					(unsigned long long)g.epoch_ms);
	} else {
		std::printf("%-20s UNKNOWN\n", "Time");
	}
	if (g.max_cno_dbhz >= 0) {
		std::printf("%-20s best C/N0 %d dBHz\n", "Signal", g.max_cno_dbhz);
	} else {
		std::printf("%-20s UNKNOWN\n", "Signal");
	}
	char sats[160], part[24];
	sats[0] = 0;
	const struct {
		int value;
		const char* label;
	} counts[] = {
		{g.seen, "seen"},        {g.no_signal, "nosig"},
		{g.searching, "search"}, {g.acquired, "acq"},
		{g.unusable, "u/s"},     {g.locked, "lock"},
		{g.ephemeris, "eph"},    {g.used, "used"},
		{g.healthy, "healthy"},  {g.unhealthy, "unhealthy"},
	};
	for (size_t i = 0; i < 10; i++) {
		count(part, sizeof part, counts[i].value, counts[i].label, i == 9);
		std::strncat(sats, part, sizeof sats - std::strlen(sats) - 1);
	}
	std::printf("%-20s %s\n", "Satellites", g.seen < 0 ? "UNKNOWN" : sats);
	if (*g.sbas_mode) {
		std::printf("%-20s %s, %s, %d satellites", "SBAS", g.sbas_mode,
					g.sbas_system, g.sbas_satellites);
		if (g.sbas_corrections) {
			std::printf(", PRC %ld cm, IC %ld cm", (long)g.prc_cm,
						(long)g.ic_cm);
		}
		std::puts("");
	} else {
		std::printf("%-20s UNKNOWN\n", "SBAS");
	}
	static const char* const antennas[] = {"INIT", "DONTKNOW", "OK", "SHORT",
										   "OPEN"};
	static const char* const powers[] = {"OFF", "ON", "DONTKNOW"};
	static const char* const jammings[] = {"UNKNOWN", "NONE", "WARNING",
										   "CRITICAL"};
	std::printf("%-20s %s/%s\n", "Antenna/Power", pick(antennas, 5, g.antenna),
				pick(powers, 3, g.antenna_power));
	std::printf("%-20s %s\n", "Jamming", pick(jammings, 4, g.jamming));
	if (*g.module) {
		std::printf("%-20s %s, %s, hw %s, BBR %s\n", "Module", g.module,
					g.software, g.hardware, g.bbr ? "OK" : "UNKNOWN");
	} else {
		std::printf("%-20s UNKNOWN\n", "Module");
	}
	return 0;
}
}

extern "C" esp_err_t console_register_gnss(void) {
	const esp_console_cmd_t entry = {
		.command = "gnss",
		.help =
			"Show the GNSS fix, satellites, SBAS and antenna state the "
			"ADSBee reports.",
		.hint = "",
		.func = command,
		.argtable = nullptr,
		.func_w_context = nullptr,
		.context = nullptr,
	};
	return console_register(&entry);
}
