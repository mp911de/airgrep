#include "traffic/traffic_protocol.hpp"
#include "traffic/traffic_table.hpp"
#include "unity.h"
#include <cstring>

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[traffic]")
#else
#define CASE(name, description) static void name()

void setUp() {
}

void tearDown() {
}
#endif

// Lines captured on the bench, 8 November 2026 near 53.4 N 7.2 E.
static const char* kAircraft =
	"#A:45C86C,1F80107F,FLI7127 ,2277,3,53.41461,7.28040,37000,37000,45,467,-128,0,E938,7F0003,2,-77,13,1,5,3388";
static const char* kNoPosition =
	"#A:4070E9,800003,?,?,0,0.00000,0.00000,36000,0,0,0,0,0,0,7F0000,0,-83,7,0,0,6908";
static const char* kFlarm =
	"#F:1,F09000,1,0,1,53.8548400,7.2998004,500,0.0,0.0,0.0,0.00,2,0,191.2,223.00,11.2,0,0,0,1794158940,113,3,15,-65,1,AEC9";
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
	TEST_ASSERT_EQUAL_UINT8(3, t.category);
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
	TEST_ASSERT_EQUAL_HEX16(0xE938, t.nicnac);
	TEST_ASSERT_EQUAL_HEX32(0x7F0003, t.acdims);
	TEST_ASSERT_EQUAL_UINT8(2, t.adsb_version);
	TEST_ASSERT_EQUAL_INT8(-77, t.rssi_dbm);
	TEST_ASSERT_EQUAL_UINT8(13, t.signal_quality_db);
	TEST_ASSERT_EQUAL_UINT16(1, t.squitter_fps);
	TEST_ASSERT_EQUAL_UINT16(5, t.es_fps);
	traffic::Integrity i = traffic::decode_nicnac(t.nicnac);
	TEST_ASSERT_EQUAL_UINT8(8, i.nic);
	TEST_ASSERT_EQUAL_UINT8(1, i.nic_baro);
	TEST_ASSERT_EQUAL_UINT8(9, i.nac_p);
	TEST_ASSERT_EQUAL_UINT8(1, i.nac_v);
	TEST_ASSERT_EQUAL_UINT8(3, i.sil);
	TEST_ASSERT_EQUAL_UINT8(2, i.gva);
	traffic::Dimensions d = traffic::decode_acdims(t.acdims);
	TEST_ASSERT_EQUAL_UINT8(3, d.sda);
	TEST_ASSERT_EQUAL_UINT8(0, d.width_m);
	TEST_ASSERT_EQUAL_UINT8(0, d.length_m);
	TEST_ASSERT_EQUAL_INT8(127, d.antenna_offset_m);
	char category[4];
	traffic::category_name(t.category, category);
	TEST_ASSERT_EQUAL_STRING("A3", category);
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
	float distance, bearing;
	TEST_ASSERT_FALSE(traffic::relative(t, 53.1f, 7.0f, distance, bearing));
	char words[128];
	traffic::flag_words(t.flags, words, sizeof words);
	TEST_ASSERT_EQUAL_STRING("AIRBORNE", words);
	traffic::flag_words(0x1F80107F, words, sizeof words);
	TEST_ASSERT_EQUAL_STRING("AIRBORNE, TCAS", words);
}

CASE(qualifiers, "Address qualifiers in the ICAO field select the source") {
	traffic::Target t;
	const char* tisb =
		"#A:7ABCDEF,8,?,?,0,0.00000,0.00000,0,0,0,0,0,0,0,0,0,-90,0,0,0,0000";
	TEST_ASSERT_TRUE(decode(tisb, t));
	TEST_ASSERT_EQUAL_HEX32(0xABCDEF, t.address);
	TEST_ASSERT_EQUAL(int(traffic::Source::tisb), int(t.source));
	const char* adsr =
		"#A:6ABCDEF,0,?,?,0,0.00000,0.00000,0,0,0,0,0,0,0,0,0,-90,0,0,0,0000";
	TEST_ASSERT_TRUE(decode(adsr, t));
	TEST_ASSERT_EQUAL(int(traffic::Source::adsr), int(t.source));
	const char* unknown =
		"#A:9ABCDEF,0,?,?,0,0.00000,0.00000,0,0,0,0,0,0,0,0,0,-90,0,0,0,0000";
	TEST_ASSERT_TRUE(decode(unknown, t));
	TEST_ASSERT_EQUAL(int(traffic::Source::other), int(t.source));
	TEST_ASSERT_FALSE(
		decode("#A:45C86C,1F80107F,FLI7127 ,2277,3,53.41461,7.28040,37000,3388", t));
}

CASE(flarm, "FLARM line decodes and normalizes to feet and knots") {
	traffic::Target t;
	TEST_ASSERT_TRUE(decode(kFlarm, t));
	TEST_ASSERT_EQUAL(int(traffic::Source::flarm), int(t.source));
	TEST_ASSERT_EQUAL_HEX32(0xF09000, t.address);
	TEST_ASSERT_EQUAL(int(traffic::FlarmId::icao), int(t.id_type));
	TEST_ASSERT_FALSE(t.stealth);
	TEST_ASSERT_FALSE(t.no_track);
	TEST_ASSERT_EQUAL_UINT8(1, t.aircraft_type);
	TEST_ASSERT_EQUAL_STRING("GLIDER", traffic::flarm_aircraft_name(t.aircraft_type));
	TEST_ASSERT_TRUE(t.position_valid);
	TEST_ASSERT_FLOAT_WITHIN(1e-6f, 53.85484f, t.latitude);
	TEST_ASSERT_TRUE(t.gnss_alt_valid);
	TEST_ASSERT_EQUAL_INT32(1640, t.gnss_alt_ft); // 500 m
	TEST_ASSERT_FALSE(t.baro_alt_valid);
	TEST_ASSERT_EQUAL_INT16(0, t.speed_kt);
	TEST_ASSERT_EQUAL_UINT8(2, t.movement);
	TEST_ASSERT_TRUE(t.airborne);
	TEST_ASSERT_EQUAL_STRING("FLYING", traffic::flarm_movement_name(t.movement));
	TEST_ASSERT_EQUAL_STRING("NORMAL", traffic::flarm_urgency_name(t.urgency));
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 191.2f, t.acc_pos_hor_m);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 223.0f, t.acc_pos_ver_m);
	TEST_ASSERT_FLOAT_WITHIN(0.01f, 11.2f, t.acc_vel_mps);
	TEST_ASSERT_EQUAL_UINT32(1794158940u, t.fix_secs);
	TEST_ASSERT_EQUAL_UINT32(113, t.age_ms);
	TEST_ASSERT_EQUAL_UINT8(3, t.version);
	TEST_ASSERT_EQUAL_UINT8(15, t.version_max);
	TEST_ASSERT_EQUAL_INT8(-65, t.rssi_dbm);
	TEST_ASSERT_EQUAL_UINT16(1, t.frames);
	const char* no_fix =
		"#F:1,F09001,0,3,9,,,500,0.0,0.0,0.0,0.00,0,3,1.0,1.00,1.0,0,0,0,0,0,3,15,-65,1,0000";
	TEST_ASSERT_TRUE(decode(no_fix, t));
	TEST_ASSERT_FALSE(t.position_valid);
	TEST_ASSERT_TRUE(t.stealth);
	TEST_ASSERT_TRUE(t.no_track);
	TEST_ASSERT_EQUAL(int(traffic::FlarmId::random), int(t.id_type));
	TEST_ASSERT_EQUAL_STRING("MAYDAY", traffic::flarm_urgency_name(t.urgency));
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
	TEST_ASSERT_EQUAL(int(traffic::Line::other), int(traffic::classify("#GNSS:1,2", 9)));
}

CASE(bearing, "Distance and bearing from 53.1 N 7.0 E") {
	traffic::Target t;
	float distance, bearing;
	TEST_ASSERT_TRUE(decode(kAircraft, t));
	TEST_ASSERT_TRUE(traffic::relative(t, 53.1f, 7.0f, distance, bearing));
	TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.41f, distance);
	TEST_ASSERT_FLOAT_WITHIN(0.2f, 27.95f, bearing);
	TEST_ASSERT_TRUE(decode(kFlarm, t));
	TEST_ASSERT_TRUE(traffic::relative(t, 53.1f, 7.0f, distance, bearing));
	TEST_ASSERT_FLOAT_WITHIN(0.05f, 46.57f, distance);
	TEST_ASSERT_FLOAT_WITHIN(0.2f, 13.18f, bearing);
	t.latitude = 53.16792f;
	t.longitude = 7.40039f;
	TEST_ASSERT_TRUE(traffic::relative(t, 53.1f, 7.0f, distance, bearing));
	TEST_ASSERT_FLOAT_WITHIN(0.05f, 14.99f, distance);
	TEST_ASSERT_FLOAT_WITHIN(0.2f, 74.05f, bearing);
}

CASE(table,
	"Table keeps one entry per address and source, evicts by age, drops the oldest when full") {
	traffic::Target slots[3];
	traffic::Table table(slots, 3);
	traffic::Target a, f;
	TEST_ASSERT_TRUE(decode(kAircraft, a));
	TEST_ASSERT_TRUE(decode(kFlarm, f));
	table.upsert(a, 1000);
	table.upsert(a, 2000);
	TEST_ASSERT_EQUAL_UINT(1, table.size());
	a.speed_kt = 400;
	table.upsert(a, 3000);
	traffic::Target found[2];
	TEST_ASSERT_EQUAL_UINT(1, table.find(0x45C86C, found, 2));
	TEST_ASSERT_EQUAL_INT16(400, found[0].speed_kt);
	TEST_ASSERT_EQUAL_INT64(3000, found[0].seen_us);
	// The same address as a FLARM target is a second entry.
	f.address = a.address;
	table.upsert(f, 3500);
	TEST_ASSERT_EQUAL_UINT(2, table.size());
	TEST_ASSERT_EQUAL_UINT(2, table.find(0x45C86C, found, 2));
	traffic::Target n;
	TEST_ASSERT_TRUE(decode(kNoPosition, n));
	table.upsert(n, 4000);
	TEST_ASSERT_EQUAL_UINT(3, table.size());
	// Full: a fourth target replaces the least recently seen one, the Mode S entry from 3000.
	traffic::Target x = n;
	x.address = 0x111111;
	table.upsert(x, 5000);
	TEST_ASSERT_EQUAL_UINT(3, table.size());
	TEST_ASSERT_EQUAL_UINT(1, table.find(0x45C86C, found, 2));
	TEST_ASSERT_EQUAL(int(traffic::Source::flarm), int(found[0].source));
	TEST_ASSERT_EQUAL_UINT(1, table.find(0x111111, found, 2));
	// Eviction removes what is older than 60 s at the given time.
	TEST_ASSERT_EQUAL_UINT(0, table.evict(3500 + 60000000LL - 1, 60000));
	TEST_ASSERT_EQUAL_UINT(2, table.evict(4000 + 60000000LL, 60000));
	TEST_ASSERT_EQUAL_UINT(1, table.size());
	TEST_ASSERT_EQUAL_UINT(1, table.find(0x111111, found, 2));
	unsigned visited = 0;
	table.each([&](const traffic::Target&) { visited++; });
	TEST_ASSERT_EQUAL_UINT(1, visited);
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(aircraft);
	RUN_TEST(no_position);
	RUN_TEST(qualifiers);
	RUN_TEST(flarm);
	RUN_TEST(stats);
	RUN_TEST(bearing);
	RUN_TEST(table);
	return UNITY_END();
}
#endif
