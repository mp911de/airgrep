#pragma once

#include <cstddef>
#include <cstdint>

// The CSBEE traffic lines of the ADSBee airgrep profile, decoded into Target
// and Stats. #A: one Mode S aircraft, #F: one FLARM target, #S: the statistics
// footer of a 1 s report round. Hardware free, host tested. Units are
// normalized so a Target reads the same whatever the source: feet, knots, feet
// per minute, degrees true.
namespace traffic {
#define TRAFFIC_AIRCRAFT_PREFIX "#A:"
#define TRAFFIC_FLARM_PREFIX "#F:"
#define TRAFFIC_STATS_PREFIX "#S:"
#define TRAFFIC_CALLSIGN_SIZE 9

enum class Source : uint8_t {
	adsb,        // 1090 ADS-B with an ICAO address, DF17 or DF18 CF=0
	adsb_other,  // DF18 CF=1, anonymous, ground vehicle or obstruction address
	adsr,        // DF18 CF=6, rebroadcast of UAT ADS-B
	tisb,        // DF18 CF=2 or 5, radar or multilateration track
	other,
	flarm,
};

constexpr size_t source_count = 6;

enum class FlarmAddressType : uint8_t { icao, synthetic };

enum class FlarmMovement : uint8_t { unknown, ground, flying, circling };
enum class FlarmUrgency : uint8_t { normal, high, pan_pan, mayday };

struct Target {
	uint32_t address = 0;  // 24 bit, qualifier stripped
	Source source = Source::adsb;
	int64_t last_seen_us = 0;

	char callsign[TRAFFIC_CALLSIGN_SIZE]{};  // trimmed, empty when unknown
	int16_t squawk = -1;                     // -1 unknown
	bool airborne = false;
	bool position_valid = false, baro_alt_valid = false, gnss_alt_valid = false;
	bool direction_valid = false, speed_valid = false;
	bool baro_rate_valid = false, gnss_rate_valid = false;
	float latitude = 0, longitude = 0;  // degrees, north and east positive
	int32_t baro_alt_ft = 0, gnss_alt_ft = 0;
	float track_deg = 0;
	int16_t speed_kt = 0;
	int16_t baro_rate_fpm = 0, gnss_rate_fpm = 0;
	int8_t rssi_dbm = 0;
	uint8_t signal_quality_db = 0;

	uint32_t mode_s_flags = 0;    // ADSBee BitFlag word as transmitted
	uint8_t mode_s_category = 0;  // ADSBee emitter category code
	uint16_t mode_s_nicnac = 0;   // SIL:2 GVA:2 NACp:4 NACv:3 NICbaro:1 NIC:4
	uint32_t mode_s_acdims = 0;   // SDA:2 width:7 length:7 antenna offset:8
	uint8_t mode_s_adsb_version = 0;
	uint16_t mode_s_squitter_fps = 0, mode_s_es_fps = 0;

	FlarmAddressType flarm_id_type = FlarmAddressType::icao;
	bool flarm_stealth = false, flarm_no_track = false;
	uint8_t flarm_aircraft_type = 0;  // FLARM aircraft type code
	FlarmMovement flarm_movement = FlarmMovement::unknown;
	FlarmUrgency flarm_urgency = FlarmUrgency::normal;
	float flarm_turn_rate_dps = 0;
	float flarm_acc_pos_hor_m = 0, flarm_acc_pos_ver_m = 0,
		  flarm_acc_vel_mps = 0;
	uint8_t flarm_sil = 0, flarm_sda = 0, flarm_nic = 0;
	uint32_t flarm_fix_secs = 0;  // unix seconds of the target's GNSS fix
	uint32_t flarm_age_ms =
		0;  // the ADSBee's age of the last frame when it reported
	uint8_t flarm_version = 0, flarm_version_max = 0;
	uint16_t flarm_frames = 0;
};

inline constexpr int kHistorySeconds = 60;

enum class Sample : uint8_t { none, held, fresh };
enum class AltitudeDatum : uint8_t {
	unknown = 0,
	barometric = 1,
	gnss = 2,
	unavailable = 3
};

struct Position {
	float latitude = 0, longitude = 0;
	int32_t altitude_ft = 0;
	int16_t speed_kt = 0;
	Sample sample = Sample::none;
	AltitudeDatum altitude_datum = AltitudeDatum::unknown;
};

// A Target with its past, the newest second first.
struct Tracked {
	Target target;
	Position past[kHistorySeconds];
};

// The turn is read from this many seconds of the past, and a turn slower
// than the dead band counts as straight, so noise bends no prediction.
inline constexpr int kTurnWindowS = 10;
inline constexpr float kTurnDeadBandDps = 1.0f;

// Degrees per second, positive to the right, 0 for straight or unknown. A
// FLARM Target reports its turn, any other shows it by how its last fresh
// positions bend.
float turn_rate_dps(const Tracked& tracked);

// How a Target moves. What it reports comes first. Where it reports nothing
// its past stands in: the leg from its oldest to its newest fresh place of
// this many seconds gives speed and course, its altitudes there the rate.
inline constexpr int kMotionWindowS = 10;
static_assert(kMotionWindowS <= kHistorySeconds);
// The altitudes that give a rate lie at least this far apart, so that the
// step of a reported altitude does not read as a climb.
inline constexpr int kRateSpanS = 4;

struct Motion {
	bool speed_valid = false, direction_valid = false;
	float speed_kt = 0, track_deg = 0;
	// Level where neither a report nor the past says otherwise.
	float vertical_fpm = 0;
};
Motion motion(const Tracked& tracked);

struct Stats {
	uint8_t version = 0;
	uint16_t demods_per_second = 0;
	uint16_t raw_squitter_fps = 0, squitter_fps = 0;
	uint16_t raw_es_fps = 0, es_fps = 0;
	uint16_t raw_uat_fps = 0, uat_fps = 0;
	uint16_t aircraft = 0;
	uint32_t tscal = 0, uptime_s = 0;
};

enum class Line { aircraft, flarm, stats, other };

Line classify(const char* text, size_t length);

// Each decoder takes one validated CSBEE line without the line end and fills
// the output from scratch. Use it only on success. last_seen_us is left to the
// caller.
bool decode_aircraft(const char* text, size_t length, Target& out);
bool decode_flarm(const char* text, size_t length, Target& out);
bool decode_stats(const char* text, size_t length, Stats& out);

// Great circle distance in nautical miles and initial bearing in degrees true
// from the reference to the target. False when the target has no position.
bool relative_position(const Target& target, float ref_latitude,
					   float ref_longitude, float& distance_nm,
					   float& bearing_deg);

const char* source_name(Source source);
// ADS-B emitter category as the web shows it, A3 style, or the raw code.
void category_name(uint8_t category, char out[4]);
const char* flarm_aircraft_name(uint8_t aircraft_type);
const char* flarm_id_name(FlarmAddressType id);
const char* flarm_movement_name(FlarmMovement movement);
const char* flarm_urgency_name(FlarmUrgency urgency);
// Comma separated flag words from the Mode S FLAGS word, empty when none is
// set.
void flag_words(uint32_t flags, char* out, size_t capacity);

struct Integrity {
	uint8_t nic, nic_baro, nac_p, nac_v, sil, gva;
};

Integrity decode_nicnac(uint16_t nicnac);

struct Dimensions {
	uint8_t sda, width_m, length_m;
	int8_t antenna_offset_m;
};

Dimensions decode_acdims(uint32_t acdims);
}
