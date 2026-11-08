#include "console.h"
#include "adsbee/adsbee_link.hpp"
#include "traffic/traffic.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "argtable.hpp"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

namespace {
	struct {
		struct arg_str* address;
		struct arg_end* end;
	} args;

	// One overview row, copied out under the table lock and sorted afterwards.
	struct Row {
		uint32_t address;
		traffic::Source source;
		char callsign[TRAFFIC_CALLSIGN_SIZE];
		int16_t squawk;
		bool alt_valid, speed_valid, direction_valid, position_valid, relative_valid;
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
		if (c->count == c->capacity) return;
		Row& r = c->rows[c->count++];
		r.address = t.address;
		r.source = t.source;
		std::strcpy(r.callsign, t.callsign);
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

	// Nearest first, then the targets without a distance by address.
	int compare(const void* a, const void* b) {
		const Row& x = *static_cast<const Row*>(a);
		const Row& y = *static_cast<const Row*>(b);
		if (x.relative_valid != y.relative_valid) return x.relative_valid ? -1 : 1;
		if (x.relative_valid && x.distance_nm != y.distance_nm) return
			x.distance_nm < y.distance_nm ? -1 : 1;
		return x.address < y.address ? -1 : x.address > y.address;
	}

	int overview() {
		Collect c{};
		c.capacity = TRAFFIC_MAX_TARGETS;
		c.rows = static_cast<Row*>(heap_caps_malloc(sizeof(Row) * c.capacity,
													MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
		if (!c.rows) {
			std::puts("FAIL no memory");
			return 1;
		}
		traffic::each(collect, &c);
		const auto link = adsbee::State::copy();
		const bool own_valid = link.gnss_us && link.gnss.fix;
		for (size_t i = 0; i < c.count; i++) {
			Row& r = c.rows[i];
			r.relative_valid = own_valid && r.position_valid &&
				traffic::relative(r.latitude, r.longitude, link.gnss.latitude,
								link.gnss.longitude, r.distance_nm, r.bearing_deg);
		}
		std::qsort(c.rows, c.count, sizeof(Row), compare);
		std::printf("%-8s %-9s %-17s %-5s %7s %5s %4s %6s %4s\n", "ADDRESS", "CALLSIGN",
					"SOURCE", "SQWK",
					"ALT FT", "KT", "HDG", "NM", "BRG");
		for (size_t i = 0; i < c.count; i++) {
			const Row& r = c.rows[i];
			char squawk[8] = "", alt[12] = "", speed[8] = "", heading[8] = "", distance[
					12] = "", bearing[8] = "";
			if (r.squawk >= 0) std::snprintf(squawk, sizeof squawk, "%04d", r.squawk);
			if (r.alt_valid) std::snprintf(alt, sizeof alt, "%ld", (long)r.alt_ft);
			if (r.speed_valid) std::snprintf(speed, sizeof speed, "%d", r.speed_kt);
			if (r.direction_valid) std::snprintf(heading, sizeof heading, "%03d",
												int(r.track_deg + 0.5f) % 360);
			if (r.relative_valid) {
				std::snprintf(distance, sizeof distance, "%.1f", r.distance_nm);
				std::snprintf(bearing, sizeof bearing, "%03d",
							int(r.bearing_deg + 0.5f) % 360);
			}
			std::printf("%06lx   %-9s %-17s %-5s %7s %5s %4s %6s %4s\n",
						(unsigned long)r.address, r.callsign,
						traffic::source_name(r.source), squawk, alt, speed, heading,
						distance, bearing);
		}
		heap_caps_free(c.rows);
		const auto counters = traffic::counters();
		int64_t now = esp_timer_get_time();
		if (counters.stats_us)
			std::printf(
				"%u targets, ADSBee %u aircraft, %u demods/s, %u ES frames/s, stats %lld s ago%s\n",
				unsigned(c.count), counters.stats.aircraft,
				counters.stats.demods_per_second,
				counters.stats.es_fps, (long long)((now - counters.stats_us) / 1000000),
				own_valid ? "" : ", no own position");
		else std::printf("%u targets, no statistics yet%s\n", unsigned(c.count),
						own_valid ? "" : ", no own position");
		return 0;
	}

	void row(const char* label, const char* value) {
		std::printf("%-20s %s\n", label, value);
	}

	void detail(const traffic::Target& t, int64_t now) {
		char text[160];
		std::snprintf(text, sizeof text, "%06lx", (unsigned long)t.address);
		row("Address", text);
		row("Source", traffic::source_name(t.source));
		std::printf("%-20s %lld s ago\n", "Seen",
					(long long)((now - t.seen_us) / 1000000));
		const auto link = adsbee::State::copy();
		float distance, bearing;
		if (link.gnss_us && link.gnss.fix && traffic::relative(
			t, link.gnss.latitude, link.gnss.longitude, distance, bearing))
			std::printf("%-20s %.1f NM, bearing %03d\n", "Relative", distance,
						int(bearing + 0.5f) % 360);
		if (t.position_valid) std::printf("%-20s %.5f, %.5f\n", "Position", t.latitude,
										t.longitude);
		else row("Position", "UNKNOWN");
		if (t.source == traffic::Source::flarm) {
			row("ID type", traffic::flarm_id_name(t.id_type));
			row("Aircraft", traffic::flarm_aircraft_name(t.aircraft_type));
			row("Movement", traffic::flarm_movement_name(t.movement));
			row("Urgency", traffic::flarm_urgency_name(t.urgency));
			std::printf("%-20s %ld ft WGS84\n", "Altitude", (long)t.gnss_alt_ft);
			std::printf("%-20s %d kt, track %03d, climb %d fpm, turn %.2f deg/s\n",
						"Motion", t.speed_kt,
						int(t.track_deg + 0.5f) % 360, t.gnss_rate_fpm, t.turn_rate_dps);
			std::printf("%-20s %.1f m horizontal, %.2f m vertical, %.1f m/s velocity\n",
						"Accuracy",
						t.acc_pos_hor_m, t.acc_pos_ver_m, t.acc_vel_mps);
			std::printf("%-20s SIL %u, SDA %u, NIC %u\n", "Integrity", t.sil, t.sda,
						t.nic);
			std::printf("%-20s %s%s%s\n", "Privacy", t.stealth ? "STEALTH" : "",
						t.stealth && t.no_track ? ", " : "",
						t.no_track ? "NO TRACK" : (t.stealth ? "" : "NONE"));
			std::printf("%-20s fix %lu s, frame age %lu ms\n", "Timing",
						(unsigned long)t.fix_secs,
						(unsigned long)t.age_ms);
			std::printf("%-20s %u, max %u\n", "Protocol", t.version, t.version_max);
			std::printf("%-20s %d dBm, %u frames\n", "Signal", t.rssi_dbm, t.frames);
			return;
		}
		row("Callsign", *t.callsign ? t.callsign : "UNKNOWN");
		if (t.squawk >= 0) std::snprintf(text, sizeof text, "%04d", t.squawk);
		else std::strcpy(text, "UNKNOWN");
		row("Squawk", text);
		char category[4];
		traffic::category_name(t.category, category);
		row("Category", category);
		if (t.baro_alt_valid) std::snprintf(text, sizeof text, "%ld ft",
											(long)t.baro_alt_ft);
		else std::strcpy(text, "UNKNOWN");
		row("Alt baro", text);
		if (t.gnss_alt_valid) std::snprintf(text, sizeof text, "%ld ft",
											(long)t.gnss_alt_ft);
		else std::strcpy(text, "UNKNOWN");
		row("Alt geom", text);
		if (t.speed_valid) std::snprintf(text, sizeof text, "%d kt", t.speed_kt);
		else std::strcpy(text, "UNKNOWN");
		row("Speed", text);
		if (t.direction_valid)
			std::snprintf(text, sizeof text, "%.1f deg%s", t.track_deg,
						t.flags & (1u << 14) ? " heading" : "");
		else std::strcpy(text, "UNKNOWN");
		row("Track", text);
		if (t.baro_rate_valid)
			std::snprintf(text, sizeof text, "%d fpm", t.baro_rate_fpm);
		else std::strcpy(text, "UNKNOWN");
		row("Vert rate baro", text);
		if (t.gnss_rate_valid)
			std::snprintf(text, sizeof text, "%d fpm", t.gnss_rate_fpm);
		else std::strcpy(text, "UNKNOWN");
		row("Vert rate geom", text);
		traffic::Integrity i = traffic::decode_nicnac(t.nicnac);
		std::printf("%-20s NIC %u, NIC baro %u, NACp %u, NACv %u, SIL %u, GVA %u\n",
					"Integrity", i.nic, i.nic_baro,
					i.nac_p, i.nac_v, i.sil, i.gva);
		traffic::Dimensions d = traffic::decode_acdims(t.acdims);
		std::printf("%-20s SDA %u, %u m wide, %u m long, antenna %d m\n", "Airframe",
					d.sda, d.width_m, d.length_m,
					d.antenna_offset_m);
		std::printf("%-20s %u\n", "ADS-B version", t.adsb_version);
		traffic::flag_words(t.flags, text, sizeof text);
		row("Flags", *text ? text : "NONE");
		std::printf("%-20s %d dBm, quality %u dB, %u squitter/s, %u ES/s\n", "Signal",
					t.rssi_dbm,
					t.signal_quality_db, t.squitter_fps, t.es_fps);
	}

	int command(int argc, char** argv) {
		if (arg_parse(argc, argv, reinterpret_cast<void**>(&args))) {
			arg_print_errors(stderr, args.end, argv[0]);
			return 1;
		}
		if (!args.address->count) return overview();
		char* end;
		unsigned long address = std::strtoul(args.address->sval[0], &end, 16);
		if (*end || address > 0xFFFFFF) {
			std::puts("usage: traffic [address], six hex digits");
			return 1;
		}
		traffic::Target found[traffic::source_count];
		size_t n = traffic::find(uint32_t(address), found, traffic::source_count);
		if (!n) {
			std::printf("%06lx not in the table\n", address);
			return 1;
		}
		int64_t now = esp_timer_get_time();
		for (size_t i = 0; i < n; i++) {
			if (i) std::puts("");
			detail(found[i], now);
		}
		return 0;
	}
}

extern "C" esp_err_t console_register_traffic(void) {
	args.address = arg_str0(nullptr, nullptr, "<address>",
							"Show every field of one target");
	args.end = arg_end(2);
	if (arg_nullcheck(reinterpret_cast<void**>(&args))) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t entry = {
		.command = "traffic",
		.help = "List the traffic nearest first, or dump one target.",
		.hint = "[address]", .func = command, .argtable = &args,
		.func_w_context = nullptr, .context = nullptr,
	};
	return console_register(&entry);
}
