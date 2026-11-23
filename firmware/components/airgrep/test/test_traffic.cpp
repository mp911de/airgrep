
#include <array>
#include <cmath>
#include <cstring>
#include <initializer_list>
#include <string>

#include "navigation/great_circle.hpp"
#include "navigation/units.h"
#include "traffic/traffic_protocol.hpp"
#include "traffic/traffic_table.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[traffic]")
#else
#define CASE(name, description) static void name()

void setUp() {}

void tearDown() {}
#endif

// Lines captured on the bench, 8 November 2026 near 53.4 N 7.2 E.
static const char* kAircraft =
	"#A:45C86C,1F80107F,FLI7127 "
	",2277,3,53.41461,7.28040,37000,37000,45,467,-"
	"128,0,E938,7F0003,2,-77,13,1,5,3388";
static const char* kNoPosition =
	"#A:4070E9,800003,?,?,0,0.00000,0.00000,36000,"
	"0,0,0,0,0,0,7F0000,0,-83,7,0,0,6908";
static const char* kFlarm =
	"#F:1,F09000,1,0,1,53.8548400,7.2998004,500,0.0,0."
	"0,0.0,0.00,2,0,191.2,223."
	"00,11.2,0,0,0,1794158940,113,3,15,-65,1,AEC9";
static const char* kStats = "#S:2,16,6,2,9,7,0,0,4,0,4234,9102";

static bool decode(const char* line, traffic::Target& out) {
	return traffic::classify(line, std::strlen(line)) == traffic::Line::flarm
			   ? traffic::decode_flarm(line, std::strlen(line), out)
			   : traffic::decode_aircraft(line, std::strlen(line), out);
}

CASE(aircraft, "Mode S line decodes identity, position, motion and signal") {
	traffic::Target t;
	TEST_ASSERT_TRUE(decode(kAircraft, t));
	TEST_ASSERT_EQUAL_HEX32(0x45C86C, t.address);
	TEST_ASSERT_EQUAL(int(traffic::Source::adsb), int(t.source));
	TEST_ASSERT_EQUAL_STRING("FLI7127", t.callsign);
	TEST_ASSERT_EQUAL_INT16(2277, t.squawk);
	TEST_ASSERT_EQUAL_UINT8(3, t.mode_s_category);
	TEST_ASSERT_TRUE(t.airborne);
	TEST_ASSERT_TRUE(t.position_valid);
	TEST_ASSERT_FLOAT_WITHIN(1e-5f, 53.41461f, t.latitude);
	TEST_ASSERT_FLOAT_WITHIN(1e-5f, 7.28040f, t.longitude);
	TEST_ASSERT_TRUE(t.baro_alt_valid);
	TEST_ASSERT_EQUAL_INT32(37000, t.baro_alt_ft);
	TEST_ASSERT_TRUE(t.gnss_alt_valid);
	TEST_ASSERT_TRUE(t.direction_valid);
	TEST_ASSERT_FLOAT_WITHIN(0.1f, 45, t.track_deg);
	TEST_ASSERT_TRUE(t.speed_valid);
	TEST_ASSERT_EQUAL_INT16(467, t.speed_kt);
	TEST_ASSERT_TRUE(t.baro_rate_valid);
	TEST_ASSERT_EQUAL_INT16(-128, t.baro_rate_fpm);
	TEST_ASSERT_FALSE(t.gnss_rate_valid);
	TEST_ASSERT_EQUAL_HEX16(0xE938, t.mode_s_nicnac);
	TEST_ASSERT_EQUAL_HEX32(0x7F0003, t.mode_s_acdims);
	TEST_ASSERT_EQUAL_UINT8(2, t.mode_s_adsb_version);
	TEST_ASSERT_EQUAL_INT8(-77, t.rssi_dbm);
	TEST_ASSERT_EQUAL_UINT8(13, t.signal_quality_db);
	TEST_ASSERT_EQUAL_UINT16(1, t.mode_s_squitter_fps);
	TEST_ASSERT_EQUAL_UINT16(5, t.mode_s_es_fps);
	const traffic::Integrity i = traffic::decode_nicnac(t.mode_s_nicnac);
	TEST_ASSERT_EQUAL_UINT8(8, i.nic);
	TEST_ASSERT_EQUAL_UINT8(1, i.nic_baro);
	TEST_ASSERT_EQUAL_UINT8(9, i.nac_p);
	TEST_ASSERT_EQUAL_UINT8(1, i.nac_v);
	TEST_ASSERT_EQUAL_UINT8(3, i.sil);
	TEST_ASSERT_EQUAL_UINT8(2, i.gva);
	const traffic::Dimensions d = traffic::decode_acdims(t.mode_s_acdims);
	TEST_ASSERT_EQUAL_UINT8(3, d.sda);
	TEST_ASSERT_EQUAL_UINT8(0, d.width_m);
	TEST_ASSERT_EQUAL_UINT8(0, d.length_m);
	TEST_ASSERT_EQUAL_INT8(127, d.antenna_offset_m);
	char category[4];
	traffic::category_name(t.mode_s_category, category);
	TEST_ASSERT_EQUAL_STRING("A3", category);
}

CASE(invalid_position, "Aircraft and FLARM validate the full coordinate pair") {
	traffic::Target t;
	for (const char* line : {kAircraft, kFlarm}) {
		const std::string original(line);
		const size_t at = original.find(",53.") + 1;
		const size_t end = original.find(',', original.find(',', at) + 1);
		for (const char* pair :
			 {"0,", ",0", "0junk,0", "0,181", "nan,0", "91,0"}) {
			std::string frame = original;
			frame.replace(at, end - at, pair);
			TEST_ASSERT_FALSE(decode(frame.c_str(), t));
		}
		std::string frame = original;
		frame.replace(at, end - at, "0,0");
		TEST_ASSERT_TRUE(decode(frame.c_str(), t));
		TEST_ASSERT_TRUE(t.position_valid);
		TEST_ASSERT_EQUAL_FLOAT(0, t.longitude);
	}
}

CASE(no_position, "A squitter-only target keeps its unknowns") {
	traffic::Target t;
	TEST_ASSERT_TRUE(decode(kNoPosition, t));
	TEST_ASSERT_EQUAL_HEX32(0x4070E9, t.address);
	TEST_ASSERT_EQUAL_STRING("", t.callsign);
	TEST_ASSERT_EQUAL_INT16(-1, t.squawk);
	TEST_ASSERT_FALSE(t.position_valid);
	TEST_ASSERT_TRUE(t.baro_alt_valid);
	TEST_ASSERT_EQUAL_INT32(36000, t.baro_alt_ft);
	TEST_ASSERT_FALSE(t.gnss_alt_valid);
	TEST_ASSERT_FALSE(t.speed_valid);
	TEST_ASSERT_FALSE(t.direction_valid);
	float distance = NAN, bearing = NAN;
	TEST_ASSERT_FALSE(
		traffic::relative_position(t, 53.1f, 7.0f, distance, bearing));
	char words[128];
	traffic::flag_words(t.mode_s_flags, words, sizeof words);
	TEST_ASSERT_EQUAL_STRING("AIRBORNE", words);
	traffic::flag_words(0x1F80107F, words, sizeof words);
	TEST_ASSERT_EQUAL_STRING("AIRBORNE, TCAS", words);
}

CASE(qualifiers, "Address qualifiers in the ICAO field select the source") {
	traffic::Target t;
	const char* tisb =
		"#A:7ABCDEF,8,?,?,0,0.00000,0.00000,0,0,0,0,0,0,0,0,0,-"
		"90,0,0,0,0000";
	TEST_ASSERT_TRUE(decode(tisb, t));
	TEST_ASSERT_EQUAL_HEX32(0xABCDEF, t.address);
	TEST_ASSERT_EQUAL(int(traffic::Source::tisb), int(t.source));
	const char* adsr =
		"#A:6ABCDEF,0,?,?,0,0.00000,0.00000,0,0,0,0,0,0,0,0,0,-"
		"90,0,0,0,0000";
	TEST_ASSERT_TRUE(decode(adsr, t));
	TEST_ASSERT_EQUAL(int(traffic::Source::adsr), int(t.source));
	const char* unknown =
		"#A:9ABCDEF,0,?,?,0,0.00000,0.00000,0,0,0,0,0,0,0,0,"
		"0,-90,0,0,0,0000";
	TEST_ASSERT_TRUE(decode(unknown, t));
	TEST_ASSERT_EQUAL(int(traffic::Source::other), int(t.source));
	TEST_ASSERT_FALSE(decode(
		"#A:45C86C,1F80107F,FLI7127 ,2277,3,53.41461,7.28040,37000,3388", t));
}

CASE(flarm, "FLARM line decodes and normalizes to feet and knots") {
	traffic::Target t;
	TEST_ASSERT_TRUE(decode(kFlarm, t));
	TEST_ASSERT_EQUAL(int(traffic::Source::flarm), int(t.source));
	TEST_ASSERT_EQUAL_HEX32(0xF09000, t.address);
	TEST_ASSERT_EQUAL(int(traffic::FlarmAddressType::synthetic),
					  int(t.flarm_id_type));
	TEST_ASSERT_FALSE(t.flarm_stealth);
	TEST_ASSERT_FALSE(t.flarm_no_track);
	TEST_ASSERT_EQUAL_UINT8(1, t.flarm_aircraft_type);
	TEST_ASSERT_EQUAL_STRING(
		"GLIDER", traffic::flarm_aircraft_name(t.flarm_aircraft_type));
	TEST_ASSERT_TRUE(t.position_valid);
	TEST_ASSERT_FLOAT_WITHIN(1e-6f, 53.85484f, t.latitude);
	TEST_ASSERT_TRUE(t.gnss_alt_valid);
	TEST_ASSERT_EQUAL_INT32(1640, t.gnss_alt_ft);  // 500 m
	TEST_ASSERT_FALSE(t.baro_alt_valid);
	TEST_ASSERT_EQUAL_INT16(0, t.speed_kt);
	TEST_ASSERT_EQUAL_UINT8(2, t.flarm_movement);
	TEST_ASSERT_TRUE(t.airborne);
	TEST_ASSERT_EQUAL_STRING("FLYING",
							 traffic::flarm_movement_name(t.flarm_movement));
	TEST_ASSERT_EQUAL_STRING("NORMAL",
							 traffic::flarm_urgency_name(t.flarm_urgency));
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 191.2f, t.flarm_acc_pos_hor_m);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 223.0f, t.flarm_acc_pos_ver_m);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 11.2f, t.flarm_acc_vel_mps);
	TEST_ASSERT_EQUAL_UINT32(1794158940u, t.flarm_fix_secs);
	TEST_ASSERT_EQUAL_UINT32(113, t.flarm_age_ms);
	TEST_ASSERT_EQUAL_UINT8(3, t.flarm_version);
	TEST_ASSERT_EQUAL_UINT8(15, t.flarm_version_max);
	TEST_ASSERT_EQUAL_INT8(-65, t.rssi_dbm);
	TEST_ASSERT_EQUAL_UINT16(1, t.flarm_frames);
	const char* no_fix =
		"#F:1,F09001,0,3,9,,,500,0.0,0.0,0.0,0.00,0,3,1.0,1."
		"00,1.0,0,0,0,0,0,3,15,-65,1,0000";
	TEST_ASSERT_TRUE(decode(no_fix, t));
	TEST_ASSERT_FALSE(t.position_valid);
	TEST_ASSERT_TRUE(t.flarm_stealth);
	TEST_ASSERT_TRUE(t.flarm_no_track);
	TEST_ASSERT_EQUAL(int(traffic::FlarmAddressType::icao),
					  int(t.flarm_id_type));
	TEST_ASSERT_EQUAL_STRING("MAYDAY",
							 traffic::flarm_urgency_name(t.flarm_urgency));
}

CASE(stats, "Statistics footer decodes") {
	traffic::Stats s;
	TEST_ASSERT_TRUE(traffic::decode_stats(kStats, std::strlen(kStats), s));
	TEST_ASSERT_EQUAL_UINT8(2, s.version);
	TEST_ASSERT_EQUAL_UINT16(16, s.demods_per_second);
	TEST_ASSERT_EQUAL_UINT16(6, s.raw_squitter_fps);
	TEST_ASSERT_EQUAL_UINT16(2, s.squitter_fps);
	TEST_ASSERT_EQUAL_UINT16(9, s.raw_es_fps);
	TEST_ASSERT_EQUAL_UINT16(7, s.es_fps);
	TEST_ASSERT_EQUAL_UINT16(4, s.aircraft);
	TEST_ASSERT_EQUAL_UINT32(4234, s.uptime_s);
	TEST_ASSERT_EQUAL(int(traffic::Line::stats),
					  int(traffic::classify(kStats, std::strlen(kStats))));
	TEST_ASSERT_EQUAL(int(traffic::Line::other),
					  int(traffic::classify("#GNSS:1,2", 9)));
}

CASE(bearing, "Distance and bearing from 53.1 N 7.0 E") {
	traffic::Target t;
	float distance = NAN, bearing = NAN;
	TEST_ASSERT_TRUE(decode(kAircraft, t));
	TEST_ASSERT_TRUE(
		traffic::relative_position(t, 53.1f, 7.0f, distance, bearing));
	TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.41f, distance);
	TEST_ASSERT_FLOAT_WITHIN(0.2f, 27.95f, bearing);
	TEST_ASSERT_TRUE(decode(kFlarm, t));
	TEST_ASSERT_TRUE(
		traffic::relative_position(t, 53.1f, 7.0f, distance, bearing));
	TEST_ASSERT_FLOAT_WITHIN(0.05f, 46.57f, distance);
	TEST_ASSERT_FLOAT_WITHIN(0.2f, 13.18f, bearing);
	auto position = navigation::great_circle(53.1f, 7.0f, 53.16792f, 7.40039f);
	TEST_ASSERT_FLOAT_WITHIN(0.05f, 14.99f, position.distance_nm);
	TEST_ASSERT_FLOAT_WITHIN(0.2f, 74.05f, position.bearing_deg);
}

CASE(table, "Updating a source and address replaces its report") {
	traffic::Target slots[2], aircraft, flarm, found[2];
	traffic::Table table(slots, 2);
	TEST_ASSERT_TRUE(decode(kAircraft, aircraft));
	table.upsert(aircraft, 1000);
	aircraft.speed_kt = 400;
	table.upsert(aircraft, 3000);
	TEST_ASSERT_EQUAL_UINT(1, table.size());
	TEST_ASSERT_EQUAL_UINT(1, table.find(aircraft.address, found, 2));
	TEST_ASSERT_EQUAL_INT16(400, found[0].speed_kt);
	TEST_ASSERT_EQUAL_INT64(3000, found[0].last_seen_us);

	TEST_ASSERT_TRUE(decode(kFlarm, flarm));
	flarm.address = aircraft.address;
	table.upsert(flarm, 3500);
	TEST_ASSERT_EQUAL_UINT(2, table.find(aircraft.address, found, 2));
	TEST_ASSERT_TRUE(found[0].source != found[1].source);
}

CASE(full_table, "A full table replaces the least recently heard aircraft") {
	traffic::Target slots[2], report, found;
	traffic::Table table(slots, 2);
	int64_t now_us = 0;
	for (const uint32_t address : {1, 2, 1, 3}) {
		report.address = address;
		now_us += 1000;
		table.upsert(report, now_us);
	}
	TEST_ASSERT_EQUAL_UINT(2, table.size());
	TEST_ASSERT_EQUAL_UINT(1, table.find(1, &found, 1));
	TEST_ASSERT_EQUAL_UINT(0, table.find(2, &found, 1));
	TEST_ASSERT_EQUAL_UINT(1, table.find(3, &found, 1));
}

CASE(expiry, "A report expires exactly at the configured age") {
	traffic::Target slots[2], report, found;
	traffic::Table table(slots, 2);
	report.address = 1;
	table.upsert(report, 1000);
	report.address = 2;
	table.upsert(report, 2000);
	constexpr int64_t first_expiry_us = 1000 + 60000000;
	TEST_ASSERT_EQUAL_UINT(0, table.evict(first_expiry_us - 1, 60000));
	TEST_ASSERT_EQUAL_UINT(1, table.evict(first_expiry_us, 60000));
	TEST_ASSERT_EQUAL_UINT(1, table.find(2, &found, 1));
	unsigned visited = 0;
	table.each([&](const traffic::Target& target) {
		TEST_ASSERT_EQUAL_UINT32(2, target.address);
		visited++;
	});
	TEST_ASSERT_EQUAL_UINT(1, visited);
}

CASE(raw_slots,
	 "Raw slot changes remain visible to size, lookup and eviction") {
	traffic::Target slots[3];
	traffic::Table table(slots, 3);
	traffic::Target target;
	TEST_ASSERT_TRUE(decode(kAircraft, target));
	target.last_seen_us = 1000;
	std::memcpy(&slots[1], &target, sizeof target);
	TEST_ASSERT_EQUAL_UINT(1, table.size());
	traffic::Target found;
	TEST_ASSERT_EQUAL_UINT(1, table.find(target.address, &found, 1));
	TEST_ASSERT_EQUAL_INT64(1000, found.last_seen_us);
	int64_t empty = 0;
	std::memcpy(&slots[1].last_seen_us, &empty, sizeof empty);
	TEST_ASSERT_EQUAL_UINT(0, table.size());
	TEST_ASSERT_EQUAL_UINT(0, table.find(target.address, &found, 1));
	std::memcpy(&slots[2], &target, sizeof target);
	TEST_ASSERT_EQUAL_UINT(1, table.evict(1000 + 60000000LL, 60000));
	TEST_ASSERT_EQUAL_UINT(0, table.size());
	table.upsert(target, 2000);
	TEST_ASSERT_EQUAL_UINT(1, table.size());
}

CASE(batch, "Batches keep unrelated targets and replace local freshness time") {
	static traffic::Target slots[40], reports[33];
	for (auto& slot : slots) {
		slot = {};
	}
	for (size_t i = 0; i < 33; ++i) {
		reports[i] = {};
		reports[i].address = i + 1;
		reports[i].last_seen_us = 99999999;
	}
	traffic::Table table(slots, 40);
	traffic::Target unrelated;
	unrelated.address = 100;
	table.upsert(unrelated, 1000);
	TEST_ASSERT_TRUE(table.upsert_batch(reports, 1, 2000));
	TEST_ASSERT_EQUAL_UINT(2, table.size());
	reports[0].speed_kt = 123;
	TEST_ASSERT_TRUE(table.upsert_batch(reports, 32, 3000));
	TEST_ASSERT_EQUAL_UINT(33, table.size());
	traffic::Target found;
	TEST_ASSERT_EQUAL_UINT(1, table.find(100, &found, 1));
	TEST_ASSERT_EQUAL_INT64(1000, found.last_seen_us);
	for (size_t i = 0; i < 32; ++i) {
		TEST_ASSERT_EQUAL_UINT(1, table.find(reports[i].address, &found, 1));
		TEST_ASSERT_EQUAL_INT64(3000, found.last_seen_us);
	}
	TEST_ASSERT_EQUAL_UINT(1, table.find(1, &found, 1));
	TEST_ASSERT_EQUAL_INT16(123, found.speed_kt);
	reports[0].speed_kt = 456;
	TEST_ASSERT_FALSE(table.upsert_batch(reports, 0, 4000));
	TEST_ASSERT_FALSE(table.upsert_batch(reports, 33, 4000));
	TEST_ASSERT_FALSE(table.upsert_batch(nullptr, 1, 4000));
	TEST_ASSERT_EQUAL_UINT(33, table.size());
	TEST_ASSERT_EQUAL_UINT(1, table.find(1, &found, 1));
	TEST_ASSERT_EQUAL_INT16(123, found.speed_kt);
	TEST_ASSERT_EQUAL_INT64(3000, found.last_seen_us);
}

namespace {
constexpr int64_t kSecond = 1000000;

traffic::Target north_of(uint32_t address, float distance_nm) {
	traffic::Target target;
	target.address = address;
	target.position_valid = target.baro_alt_valid = true;
	target.latitude = 50 + distance_nm / units::nm_per_degree_latitude;
	target.longitude = 8;
	target.baro_alt_ft = 3000;
	target.speed_kt = 90;
	return target;
}
}

CASE(history,
	 "A target keeps where it was in each of its last 60 seconds, newest "
	 "first") {
	traffic::Target slots[2];
	traffic::Position past[2 * traffic::kHistorySeconds];
	traffic::Table table(slots, 2, past);
	traffic::Tracked listed[2];
	for (int second = 1; second <= 3; second++) {
		table.upsert(north_of(0xAAAAAA, float(second)), second * kSecond);
		table.sample(second * kSecond);
	}
	table.sample(4 * kSecond);
	table.sample(6 * kSecond);
	TEST_ASSERT_EQUAL_UINT(1, table.nearest(nullptr, listed, 2));
	const traffic::Position* was = listed[0].past;
	TEST_ASSERT_EQUAL(int(traffic::Sample::held), int(was[0].sample));
	TEST_ASSERT_EQUAL(int(traffic::Sample::fresh), int(was[1].sample));
	TEST_ASSERT_EQUAL(int(traffic::Sample::fresh), int(was[2].sample));
	TEST_ASSERT_EQUAL(int(traffic::Sample::fresh), int(was[4].sample));
	TEST_ASSERT_EQUAL(int(traffic::Sample::none), int(was[5].sample));
	TEST_ASSERT_FLOAT_WITHIN(1e-4f, 50 + 3 / 60.0f, was[0].latitude);
	TEST_ASSERT_FLOAT_WITHIN(1e-4f, 50 + 2 / 60.0f, was[3].latitude);
	TEST_ASSERT_FLOAT_WITHIN(1e-4f, 50 + 1 / 60.0f, was[4].latitude);
	TEST_ASSERT_EQUAL_INT32(3000, was[0].altitude_ft);
	TEST_ASSERT_EQUAL_INT16(90, was[0].speed_kt);
}

CASE(history_wrap, "Fresh reports fall out after a complete history window") {
	traffic::Target slot;
	traffic::Position past[traffic::kHistorySeconds];
	traffic::Table table(&slot, 1, past);
	traffic::Tracked tracked;
	table.upsert(north_of(1, 1), 6 * kSecond);
	table.sample(6 * kSecond);
	for (int second = 7; second <= 7 + traffic::kHistorySeconds; second++) {
		table.sample(second * kSecond);
	}
	TEST_ASSERT_EQUAL_UINT(1, table.nearest(nullptr, &tracked, 1));
	for (const traffic::Position& position : tracked.past) {
		TEST_ASSERT_EQUAL(int(traffic::Sample::held), int(position.sample));
	}
}

CASE(history_without_position,
	 "A report without coordinates leaves its history empty") {
	traffic::Target slot;
	traffic::Position past[traffic::kHistorySeconds];
	traffic::Table table(&slot, 1, past);
	traffic::Tracked listed[1];
	traffic::Target silent = north_of(0xBBBBBB, 0);
	silent.position_valid = false;
	table.upsert(silent, 40 * kSecond);
	table.sample(40 * kSecond);
	TEST_ASSERT_EQUAL_UINT(1, table.nearest(nullptr, listed, 1));
	TEST_ASSERT_EQUAL_HEX32(0xBBBBBB, listed[0].target.address);
	TEST_ASSERT_EQUAL(int(traffic::Sample::none),
					  int(listed[0].past[0].sample));
}

CASE(history_owner,
	 "The past belongs to the target: a slot that changes hands starts "
	 "empty") {
	traffic::Target slots[1];
	traffic::Position past[traffic::kHistorySeconds];
	traffic::Table table(slots, 1, past);
	traffic::Tracked listed[1];
	table.upsert(north_of(0xAAAAAA, 1), kSecond);
	table.sample(kSecond);
	table.sample(2 * kSecond);
	table.upsert(north_of(0xCCCCCC, 5), 3 * kSecond);
	table.sample(3 * kSecond);
	TEST_ASSERT_EQUAL_UINT(1, table.nearest(nullptr, listed, 1));
	TEST_ASSERT_EQUAL_HEX32(0xCCCCCC, listed[0].target.address);
	TEST_ASSERT_EQUAL(int(traffic::Sample::fresh),
					  int(listed[0].past[0].sample));
	TEST_ASSERT_EQUAL(int(traffic::Sample::none),
					  int(listed[0].past[1].sample));
	TEST_ASSERT_EQUAL(int(traffic::Sample::none),
					  int(listed[0].past[2].sample));
	table.upsert(north_of(0xCCCCCC, 6), 4 * kSecond);
	table.sample(4 * kSecond);
	table.nearest(nullptr, listed, 1);
	TEST_ASSERT_EQUAL(int(traffic::Sample::fresh),
					  int(listed[0].past[1].sample));
}

CASE(raw_history_owner,
	 "Raw traffic writes preserve history only for the same aircraft") {
	traffic::Target slots[2]{};
	traffic::Position history[2 * traffic::kHistorySeconds]{};
	traffic::Table table(slots, 2, history);
	for (unsigned i = 0; i < 2; ++i) {
		traffic::Target target{};
		target.address = i + 1;
		target.position_valid = true;
		table.upsert(target, 1);
	}
	table.sample(1);
	auto replacement = slots[1];
	replacement.address = 3;
	std::array<uint8_t, sizeof(slots)> data{};
	std::memcpy(data.data(), slots, sizeof slots);
	std::memcpy(data.data() + sizeof(traffic::Target), &replacement,
				sizeof replacement);
	TEST_ASSERT_FALSE(table.write_bytes(1, data));
	TEST_ASSERT_TRUE(table.write_bytes(1, std::span(data).subspan(1)));
	traffic::Tracked rows[2];
	TEST_ASSERT_EQUAL_UINT(2, table.nearest(nullptr, rows, 2));
	TEST_ASSERT_EQUAL_UINT(1, rows[0].target.address);
	TEST_ASSERT_EQUAL_INT(int(traffic::Sample::fresh),
						  int(rows[0].past[0].sample));
	TEST_ASSERT_EQUAL_UINT(3, rows[1].target.address);
	TEST_ASSERT_EQUAL_INT(int(traffic::Sample::none),
						  int(rows[1].past[0].sample));
}

CASE(no_history_storage,
	 "A table without history still lists current targets") {
	traffic::Target slots[1];
	traffic::Tracked listed[1];
	traffic::Table plain(slots, 1);
	plain.upsert(north_of(1, 1), kSecond);
	plain.sample(5 * kSecond);
	TEST_ASSERT_EQUAL_UINT(1, plain.nearest(nullptr, listed, 1));
	TEST_ASSERT_EQUAL(int(traffic::Sample::none),
					  int(listed[0].past[0].sample));
}

CASE(nearest,
	 "A listing is the nearest first, cut at its limit, the unplaced last") {
	traffic::Target slots[6];
	traffic::Table table(slots, 6);
	const float miles[] = {9, 2, 30, 0.5f, 4};
	for (uint32_t i = 0; i < 5; i++) {
		table.upsert(north_of(100 + i, miles[i]), kSecond + i);
	}
	traffic::Target unplaced = north_of(900, 0);
	unplaced.position_valid = false;
	table.upsert(unplaced, 9 * kSecond);
	const traffic::Reference own{50, 8};
	traffic::Tracked listed[traffic::kListed + 1];

	TEST_ASSERT_EQUAL_UINT(6, table.nearest(&own, listed, 10));
	const uint32_t by_distance[] = {103, 101, 104, 100, 102, 900};
	for (size_t i = 0; i < 6; i++) {
		TEST_ASSERT_EQUAL_UINT32(by_distance[i], listed[i].target.address);
	}
	TEST_ASSERT_EQUAL_UINT(3, table.nearest(&own, listed, 3));
	for (size_t i = 0; i < 3; i++) {
		TEST_ASSERT_EQUAL_UINT32(by_distance[i], listed[i].target.address);
	}
	TEST_ASSERT_EQUAL_UINT(0, table.nearest(&own, listed, 0));

	TEST_ASSERT_EQUAL_UINT(6, table.nearest(nullptr, listed, 10));
	const uint32_t newest_first[] = {900, 104, 103, 102, 101, 100};
	for (size_t i = 0; i < 6; i++) {
		TEST_ASSERT_EQUAL_UINT32(newest_first[i], listed[i].target.address);
	}
}

CASE(listing_limit, "A listing caps the number of returned contacts") {
	const traffic::Reference own{50, 8};
	static traffic::Tracked listed[traffic::kListed + 1];
	static traffic::Target many[traffic::kListed + 8];
	traffic::Table crowded(many, traffic::kListed + 8);
	for (uint32_t i = 0; i < traffic::kListed + 8; i++) {
		crowded.upsert(north_of(1000 + i, float(i)), kSecond);
	}
	TEST_ASSERT_EQUAL_UINT(traffic::kListed,
						   crowded.nearest(&own, listed, 1000));
	TEST_ASSERT_EQUAL_UINT32(1000, listed[0].target.address);
	TEST_ASSERT_EQUAL_UINT32(1000 + traffic::kListed - 1,
							 listed[traffic::kListed - 1].target.address);
}

static traffic::Tracked turning(float turn_dps) {
	constexpr float groundspeed_kt = 90;
	constexpr double distance_nm = groundspeed_kt / 3600.0;
	double latitude = 50, longitude = 8, course_deg = 0;
	traffic::Tracked tracked;
	for (int age = traffic::kTurnWindowS - 1; age >= 0; age--) {
		auto& position = tracked.past[age];
		position.latitude = static_cast<float>(latitude);
		position.longitude = static_cast<float>(longitude);
		position.sample = traffic::Sample::fresh;
		position.speed_kt = groundspeed_kt;
		course_deg += turn_dps;
		const double course_rad =
			course_deg * units::radians_per_degree<double>;
		latitude +=
			distance_nm * std::cos(course_rad) / units::nm_per_degree_latitude;
		longitude += distance_nm * std::sin(course_rad) /
					 units::nm_per_degree_latitude /
					 std::cos(50 * units::radians_per_degree<double>);
	}
	return tracked;
}

CASE(turn, "History distinguishes straight, left and right turns") {
	TEST_ASSERT_EQUAL_FLOAT(0, traffic::turn_rate_dps(turning(0)));
	TEST_ASSERT_FLOAT_WITHIN(0.3f, 3, traffic::turn_rate_dps(turning(3)));
	TEST_ASSERT_FLOAT_WITHIN(0.3f, -3, traffic::turn_rate_dps(turning(-3)));
}

CASE(turn_deadband, "Rates below the turn deadband read as straight") {
	TEST_ASSERT_EQUAL_FLOAT(0, traffic::turn_rate_dps(turning(0.5f)));
}

CASE(turn_minimum_reports, "Two positions cannot establish a turn") {
	auto tracked = turning(3);
	for (int age = 2; age < traffic::kHistorySeconds; age++) {
		tracked.past[age].sample = traffic::Sample::held;
	}
	TEST_ASSERT_EQUAL_FLOAT(0, traffic::turn_rate_dps(tracked));
}

CASE(turn_gaps, "A gap between reports counts towards elapsed time") {
	auto tracked = turning(3);
	for (const int age : {1, 2, 4, 5, 7, 8}) {
		tracked.past[age].sample = traffic::Sample::held;
	}
	TEST_ASSERT_FLOAT_WITHIN(0.5f, 3, traffic::turn_rate_dps(tracked));
}

CASE(repeated_positions, "Repeating a position does not add a turn") {
	auto straight = turning(0);
	straight.past[0] = straight.past[1];
	TEST_ASSERT_EQUAL_FLOAT(0, traffic::turn_rate_dps(straight));
	straight.past[traffic::kTurnWindowS - 1] =
		straight.past[traffic::kTurnWindowS - 2];
	TEST_ASSERT_EQUAL_FLOAT(0, traffic::turn_rate_dps(straight));
	auto tracked = turning(3);
	for (int age = 0; age + 1 < traffic::kTurnWindowS; age += 2) {
		tracked.past[age] = tracked.past[age + 1];
	}
	TEST_ASSERT_FLOAT_WITHIN(0.5f, 3, traffic::turn_rate_dps(tracked));
}

CASE(flarm_turn, "FLARM reported turn takes precedence over history") {
	auto tracked = turning(3);
	tracked.target.source = traffic::Source::flarm;
	tracked.target.flarm_turn_rate_dps = -12;
	TEST_ASSERT_EQUAL_FLOAT(-12, traffic::turn_rate_dps(tracked));
	tracked.target.flarm_turn_rate_dps = 0.4f;
	TEST_ASSERT_EQUAL_FLOAT(0, traffic::turn_rate_dps(tracked));
}

static traffic::Tracked climbing() {
	constexpr float groundspeed_kt = 90;
	constexpr int climb_fpm = 600;
	traffic::Tracked tracked;
	tracked.target.baro_alt_valid = true;
	for (int age = 0; age < traffic::kMotionWindowS; age++) {
		auto& position = tracked.past[age];
		position.latitude = 50 - static_cast<float>(age) * groundspeed_kt /
									 3600 / units::nm_per_degree_latitude;
		position.longitude = 8;
		position.sample = traffic::Sample::fresh;
		position.altitude_ft = 3000 - age * climb_fpm / 60;
		position.altitude_datum = traffic::AltitudeDatum::barometric;
	}
	return tracked;
}

CASE(motion, "History supplies missing vertical and horizontal velocity") {
	auto moves = traffic::motion(climbing());
	TEST_ASSERT_TRUE(moves.speed_valid && moves.direction_valid);
	TEST_ASSERT_FLOAT_WITHIN(2, 90, moves.speed_kt);
	TEST_ASSERT_FLOAT_WITHIN(1, 0, std::remainder(moves.track_deg, 360.0f));
	TEST_ASSERT_FLOAT_WITHIN(1, 600, moves.vertical_fpm);
}

CASE(altitude_history,
	 "Only altitudes measured against the same datum give a vertical rate") {
	traffic::Target slot;
	traffic::Position past[traffic::kHistorySeconds];
	traffic::Table table(&slot, 1, past);
	auto report = north_of(1, 0);
	traffic::Tracked tracked;
	auto sample = [&](int second) {
		table.upsert(report, second * kSecond);
		table.sample(second * kSecond);
		TEST_ASSERT_EQUAL_UINT(1, table.nearest(nullptr, &tracked, 1));
	};
	for (int second = 1; second < 10; second++) {
		report.baro_alt_valid = false;
		sample(second);
	}
	report.baro_alt_valid = true;
	report.baro_alt_ft = 90;
	sample(10);
	TEST_ASSERT_EQUAL_FLOAT(0, traffic::motion(tracked).vertical_fpm);
	for (int second = 11; second <= 15; second++) {
		report.baro_alt_valid = false;
		report.gnss_alt_valid = true;
		report.gnss_alt_ft = (second - 11) * 10;
		sample(second);
		if (second == 11) {
			TEST_ASSERT_EQUAL_FLOAT(0, traffic::motion(tracked).vertical_fpm);
		}
	}
	TEST_ASSERT_EQUAL_FLOAT(600, traffic::motion(tracked).vertical_fpm);
	report.baro_alt_valid = true;
	report.baro_alt_ft = 990;
	sample(16);
	TEST_ASSERT_EQUAL_FLOAT(9000, traffic::motion(tracked).vertical_fpm);
}

CASE(short_climb_history,
	 "Closely spaced altitudes cannot establish vertical speed") {
	auto tracked = climbing();
	for (int age = traffic::kRateSpanS; age < traffic::kHistorySeconds; age++) {
		tracked.past[age].sample = traffic::Sample::held;
	}
	auto moves = traffic::motion(tracked);
	TEST_ASSERT_EQUAL_FLOAT(0, moves.vertical_fpm);
	TEST_ASSERT_FLOAT_WITHIN(2, 90, moves.speed_kt);
}

CASE(reported_motion,
	 "Each reported velocity overrides only its own estimate") {
	auto tracked = climbing();
	tracked.target.speed_valid = true;
	tracked.target.speed_kt = 120;
	tracked.target.gnss_rate_valid = true;
	tracked.target.gnss_rate_fpm = -300;
	auto moves = traffic::motion(tracked);
	TEST_ASSERT_EQUAL_FLOAT(120, moves.speed_kt);
	TEST_ASSERT_FLOAT_WITHIN(1, 0, std::remainder(moves.track_deg, 360.0f));
	TEST_ASSERT_EQUAL_FLOAT(-300, moves.vertical_fpm);
}

CASE(single_position, "A single position cannot establish velocity") {
	traffic::Tracked once;
	once.past[0] = climbing().past[0];
	auto moves = traffic::motion(once);
	TEST_ASSERT_FALSE(moves.speed_valid || moves.direction_valid);
	TEST_ASSERT_EQUAL_FLOAT(0, moves.vertical_fpm);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(aircraft);
	RUN_TEST(invalid_position);
	RUN_TEST(no_position);
	RUN_TEST(qualifiers);
	RUN_TEST(flarm);
	RUN_TEST(stats);
	RUN_TEST(bearing);
	RUN_TEST(table);
	RUN_TEST(full_table);
	RUN_TEST(expiry);
	RUN_TEST(raw_slots);
	RUN_TEST(batch);
	RUN_TEST(history);
	RUN_TEST(history_wrap);
	RUN_TEST(history_without_position);
	RUN_TEST(history_owner);
	RUN_TEST(raw_history_owner);
	RUN_TEST(no_history_storage);
	RUN_TEST(nearest);
	RUN_TEST(listing_limit);
	RUN_TEST(turn);
	RUN_TEST(turn_deadband);
	RUN_TEST(turn_minimum_reports);
	RUN_TEST(turn_gaps);
	RUN_TEST(repeated_positions);
	RUN_TEST(flarm_turn);
	RUN_TEST(motion);
	RUN_TEST(altitude_history);
	RUN_TEST(short_climb_history);
	RUN_TEST(reported_motion);
	RUN_TEST(single_position);
	return UNITY_END();
}
#endif
