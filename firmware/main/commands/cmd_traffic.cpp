
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "adsbee/adsbee_link.hpp"
#include "argtable.hpp"
#include "console.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "navigation/great_circle.hpp"
#include "traffic/traffic.hpp"

namespace {
struct {
	struct arg_str* address;
	struct arg_end* end;
	void* table[2];
} args;

struct Row {
	uint32_t address;
	traffic::Source source;
	char callsign[TRAFFIC_CALLSIGN_SIZE];
	int16_t squawk;
	bool alt_valid, speed_valid, direction_valid, position_valid,
		relative_valid;
	int32_t alt_ft;
	int16_t speed_kt;
	float track_deg, latitude, longitude, distance_nm, bearing_deg;
};

struct Collect {
	Row* rows;
	size_t count, capacity;
};

void collect(const traffic::Target& t, void* context) {
	auto* c = static_cast<Collect*>(context);
	if (c->count == c->capacity) {
		return;
	}
	Row& r = c->rows[c->count++];
	r.address = t.address;
	r.source = t.source;
	std::memcpy(r.callsign, t.callsign, sizeof r.callsign);
	r.squawk = t.squawk;
	r.alt_valid = t.baro_alt_valid || t.gnss_alt_valid;
	r.alt_ft = t.baro_alt_valid ? t.baro_alt_ft : t.gnss_alt_ft;
	r.speed_valid = t.speed_valid;
	r.speed_kt = t.speed_kt;
	r.direction_valid = t.direction_valid;
	r.track_deg = t.track_deg;
	r.position_valid = t.position_valid;
	r.latitude = t.latitude;
	r.longitude = t.longitude;
}

int nearest_first(const void* a, const void* b) {
	const Row& x = *static_cast<const Row*>(a);
	const Row& y = *static_cast<const Row*>(b);
	if (x.relative_valid != y.relative_valid) {
		return x.relative_valid ? -1 : 1;
	}
	if (x.relative_valid && x.distance_nm != y.distance_nm) {
		return x.distance_nm < y.distance_nm ? -1 : 1;
	}
	return x.address < y.address ? -1 : x.address > y.address;
}

int overview() {
	Collect c{};
	c.capacity = TRAFFIC_MAX_TARGETS;
	c.rows = static_cast<Row*>(heap_caps_malloc(
		sizeof(Row) * c.capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	if (!c.rows) {
		std::puts("FAIL no memory");
		return 1;
	}
	traffic::each(collect, &c);
	const auto link = adsbee::State::copy();
	const auto& own = link.gnss_at(esp_timer_get_time());
	const bool own_valid = own.fix != adsbee::Fix::none;
	for (size_t i = 0; i < c.count; i++) {
		Row& r = c.rows[i];
		r.relative_valid = own_valid && r.position_valid;
		if (r.relative_valid) {
			auto position = navigation::great_circle(
				own.latitude, own.longitude, r.latitude, r.longitude);
			r.distance_nm = position.distance_nm;
			r.bearing_deg = position.bearing_deg;
		}
	}
	std::qsort(c.rows, c.count, sizeof(Row), nearest_first);
	std::printf("%-8s %-9s %-17s %-5s %7s %5s %4s %6s %4s\n", "ADDRESS",
				"CALLSIGN", "SOURCE", "SQWK", "ALT FT", "KT", "HDG", "NM",
				"BRG");
	for (size_t i = 0; i < c.count; i++) {
		const Row& r = c.rows[i];
		char squawk[8] = "", alt[12] = "", speed[8] = "", heading[8] = "",
			 distance[12] = "", bearing[8] = "";
		if (r.squawk >= 0) {
			(void)std::snprintf(squawk, sizeof squawk, "%04d", r.squawk);
		}
		if (r.alt_valid) {
			(void)std::snprintf(alt, sizeof alt, "%ld", (long)r.alt_ft);
		}
		if (r.speed_valid) {
			(void)std::snprintf(speed, sizeof speed, "%d", r.speed_kt);
		}
		if (r.direction_valid) {
			(void)std::snprintf(heading, sizeof heading, "%03d",
								int(std::lround(r.track_deg)) % 360);
		}
		if (r.relative_valid) {
			(void)std::snprintf(distance, sizeof distance, "%.1f",
								r.distance_nm);
			(void)std::snprintf(bearing, sizeof bearing, "%03d",
								int(std::lround(r.bearing_deg)) % 360);
		}
		std::printf("%06lx   %-9s %-17s %-5s %7s %5s %4s %6s %4s\n",
					(unsigned long)r.address, r.callsign,
					traffic::source_name(r.source), squawk, alt, speed, heading,
					distance, bearing);
	}
	heap_caps_free(c.rows);
	const auto counters = traffic::counters();
	const int64_t now = esp_timer_get_time();
	if (counters.last_stats_report_us) {
		std::printf(
			"%u targets, ADSBee %u aircraft, %u demods/s, %u ES "
			"frames/s, stats %lld s ago%s\n",
			unsigned(c.count), counters.stats.aircraft,
			counters.stats.demods_per_second, counters.stats.es_fps,
			(long long)((now - counters.last_stats_report_us) / 1000000),
			own_valid ? "" : ", no own position");
	} else {
		std::printf("%u targets, no statistics yet%s\n", unsigned(c.count),
					own_valid ? "" : ", no own position");
	}
	return 0;
}

void row(const char* label, const char* value) {
	std::printf("%-20s %s\n", label, value);
}

void detail(const traffic::Target& t, int64_t now) {
	std::printf("%-20s %06lx\n", "Address", (unsigned long)t.address);
	row("Source", traffic::source_name(t.source));
	std::printf("%-20s %lld s ago\n", "Seen",
				(long long)((now - t.last_seen_us) / 1000000));
	const auto link = adsbee::State::copy();
	const auto& own = link.gnss_at(now);
	float distance = NAN, bearing = NAN;
	if (own.fix != adsbee::Fix::none &&
		traffic::relative_position(t, own.latitude, own.longitude, distance,
								   bearing)) {
		std::printf("%-20s %.1f NM, bearing %03d\n", "Relative", distance,
					int(std::lround(bearing)) % 360);
	}
	if (t.position_valid) {
		std::printf("%-20s %.5f, %.5f\n", "Position", t.latitude, t.longitude);
	} else {
		row("Position", "UNKNOWN");
	}
	if (t.source == traffic::Source::flarm) {
		row("ID type", traffic::flarm_id_name(t.flarm_id_type));
		row("Aircraft", traffic::flarm_aircraft_name(t.flarm_aircraft_type));
		row("Movement", traffic::flarm_movement_name(t.flarm_movement));
		row("Urgency", traffic::flarm_urgency_name(t.flarm_urgency));
		std::printf("%-20s %ld ft WGS84\n", "Altitude", (long)t.gnss_alt_ft);
		std::printf("%-20s %d kt, track %03d, climb %d fpm, turn %.2f deg/s\n",
					"Motion", t.speed_kt, int(std::lround(t.track_deg)) % 360,
					t.gnss_rate_fpm, t.flarm_turn_rate_dps);
		std::printf(
			"%-20s %.1f m horizontal, %.2f m vertical, %.1f m/s "
			"velocity\n",
			"Accuracy", t.flarm_acc_pos_hor_m, t.flarm_acc_pos_ver_m,
			t.flarm_acc_vel_mps);
		std::printf("%-20s SIL %u, SDA %u, NIC %u\n", "Integrity", t.flarm_sil,
					t.flarm_sda, t.flarm_nic);
		std::printf(
			"%-20s %s%s%s\n", "Privacy", t.flarm_stealth ? "STEALTH" : "",
			t.flarm_stealth && t.flarm_no_track ? ", " : "",
			t.flarm_no_track ? "NO TRACK" : (t.flarm_stealth ? "" : "NONE"));
		std::printf("%-20s fix %lu s, frame age %lu ms\n", "Timing",
					(unsigned long)t.flarm_fix_secs,
					(unsigned long)t.flarm_age_ms);
		std::printf("%-20s %u, max %u\n", "Protocol", t.flarm_version,
					t.flarm_version_max);
		std::printf("%-20s %d dBm, %u frames\n", "Signal", t.rssi_dbm,
					t.flarm_frames);
		return;
	}
	row("Callsign", *t.callsign ? t.callsign : "UNKNOWN");
	if (t.squawk >= 0) {
		std::printf("%-20s %04d\n", "Squawk", t.squawk);
	} else {
		row("Squawk", "UNKNOWN");
	}
	char category[4];
	traffic::category_name(t.mode_s_category, category);
	row("Category", category);
	if (t.baro_alt_valid) {
		std::printf("%-20s %ld ft\n", "Alt baro", (long)t.baro_alt_ft);
	} else {
		row("Alt baro", "UNKNOWN");
	}
	if (t.gnss_alt_valid) {
		std::printf("%-20s %ld ft\n", "Alt geom", (long)t.gnss_alt_ft);
	} else {
		row("Alt geom", "UNKNOWN");
	}
	if (t.speed_valid) {
		std::printf("%-20s %d kt\n", "Speed", t.speed_kt);
	} else {
		row("Speed", "UNKNOWN");
	}
	if (t.direction_valid) {
		std::printf("%-20s %.1f deg%s\n", "Track", t.track_deg,
					t.mode_s_flags & (1u << 14) ? " heading" : "");
	} else {
		row("Track", "UNKNOWN");
	}
	if (t.baro_rate_valid) {
		std::printf("%-20s %d fpm\n", "Vert rate baro", t.baro_rate_fpm);
	} else {
		row("Vert rate baro", "UNKNOWN");
	}
	if (t.gnss_rate_valid) {
		std::printf("%-20s %d fpm\n", "Vert rate geom", t.gnss_rate_fpm);
	} else {
		row("Vert rate geom", "UNKNOWN");
	}
	const traffic::Integrity i = traffic::decode_nicnac(t.mode_s_nicnac);
	std::printf("%-20s NIC %u, NIC baro %u, NACp %u, NACv %u, SIL %u, GVA %u\n",
				"Integrity", i.nic, i.nic_baro, i.nac_p, i.nac_v, i.sil, i.gva);
	const traffic::Dimensions d = traffic::decode_acdims(t.mode_s_acdims);
	std::printf("%-20s SDA %u, %u m wide, %u m long, antenna %d m\n",
				"Airframe", d.sda, d.width_m, d.length_m, d.antenna_offset_m);
	std::printf("%-20s %u\n", "ADS-B version", t.mode_s_adsb_version);
	char text[160];
	traffic::flag_words(t.mode_s_flags, text, sizeof text);
	row("Flags", *text ? text : "NONE");
	std::printf("%-20s %d dBm, quality %u dB, %u squitter/s, %u ES/s\n",
				"Signal", t.rssi_dbm, t.signal_quality_db,
				t.mode_s_squitter_fps, t.mode_s_es_fps);
}

int command(int argc, char** argv) {
	if (arg_parse(argc, argv, args.table)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	if (!args.address->count) {
		return overview();
	}
	char* end = nullptr;
	const unsigned long address = std::strtoul(args.address->sval[0], &end, 16);
	if (*end || address > 0xFFFFFF) {
		std::puts("usage: traffic [address], six hex digits");
		return 1;
	}
	traffic::Target found[traffic::source_count];
	const size_t n =
		traffic::find(uint32_t(address), found, traffic::source_count);
	if (!n) {
		std::printf("%06lx not in the table\n", address);
		return 1;
	}
	const int64_t now = esp_timer_get_time();
	for (size_t i = 0; i < n; i++) {
		if (i) {
			std::puts("");
		}
		detail(found[i], now);
	}
	return 0;
}
}

extern "C" esp_err_t console_register_traffic(void) {
	args.address = arg_str0(nullptr, nullptr, "<address>",
							"Show every field of one target");
	args.end = arg_end(2);
	args.table[0] = args.address;
	args.table[1] = args.end;
	if (arg_nullcheck(args.table)) {
		return ESP_ERR_NO_MEM;
	}
	const esp_console_cmd_t entry = {
		.command = "traffic",
		.help = "List the traffic nearest first, or dump one target.",
		.hint = "[address]",
		.func = command,
		.argtable = static_cast<void*>(args.table),
		.func_w_context = nullptr,
		.context = nullptr,
	};
	return console_register(&entry);
}
