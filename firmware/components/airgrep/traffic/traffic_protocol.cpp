#include "traffic_protocol.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <type_traits>

#include "adsbee/csbee_fields.hpp"
#include "navigation/great_circle.hpp"
#include "navigation/units.h"

namespace traffic {
namespace {
constexpr size_t kAircraftFields = 20;
constexpr size_t kFlarmFields = 26;
constexpr size_t kStatsFields = 11;
constexpr size_t kMaxFields = 26;

// ADSBee BitFlag positions.
constexpr uint32_t kFlagAirborne = 1u << 0;
constexpr uint32_t kFlagBaroAlt = 1u << 1;
constexpr uint32_t kFlagGnssAlt = 1u << 2;
constexpr uint32_t kFlagPosition = 1u << 3;
constexpr uint32_t kFlagDirection = 1u << 4;
constexpr uint32_t kFlagSpeed = 1u << 5;
constexpr uint32_t kFlagBaroRate = 1u << 6;
constexpr uint32_t kFlagGnssRate = 1u << 7;

// Address qualifiers the ADSBee folds into bits 24 to 27 of the ICAO field.
// ICAO addressed targets carry none, whatever message type delivered them.
constexpr unsigned kQualifierAdsbOther = 5, kQualifierAdsr = 6;
constexpr unsigned kQualifierTisbTrackFile = 7, kQualifierTisbOther = 8;

using csbee::copy;
using csbee::decimal;
using csbee::Field;
using csbee::integer;
using csbee::split;

// An empty measurement is allowed only when its validity flag is clear.
template <typename T>
bool measurement(Field field, bool valid, T& value) {
	if (field.empty()) {
		return !valid;
	}
	if constexpr (std::is_floating_point_v<T>) {
		return decimal(field, value);
	} else {
		return integer(field, value);
	}
}

template <typename T>
bool scaled(Field field, double factor, T& value) {
	double parsed = NAN;
	if (!decimal(field, parsed)) {
		return false;
	}
	const double rounded = std::round(parsed * factor);
	if (rounded < std::numeric_limits<T>::lowest() ||
		rounded > std::numeric_limits<T>::max()) {
		return false;
	}
	value = static_cast<T>(rounded);
	return true;
}

bool starts(const char* text, size_t length, const char* prefix) {
	const size_t n = std::strlen(prefix);
	return length >= n && std::memcmp(text, prefix, n) == 0;
}

void append(char* out, size_t capacity, const char* word) {
	const size_t used = std::strlen(out);
	(void)std::snprintf(out + used, capacity - used, "%s%s", used ? ", " : "",
						word);
}
}

Line classify(const char* text, size_t length) {
	if (starts(text, length, TRAFFIC_AIRCRAFT_PREFIX)) {
		return Line::aircraft;
	}
	if (starts(text, length, TRAFFIC_FLARM_PREFIX)) {
		return Line::flarm;
	}
	if (starts(text, length, TRAFFIC_STATS_PREFIX)) {
		return Line::stats;
	}
	return Line::other;
}

bool decode_aircraft(const char* text, size_t length, Target& out) {
	Field f[kMaxFields];
	if (!starts(text, length, TRAFFIC_AIRCRAFT_PREFIX) ||
		!split(text, length, sizeof TRAFFIC_AIRCRAFT_PREFIX - 1,
			   kAircraftFields, f)) {
		return false;
	}
	out = Target{};
	uint32_t icao = 0;
	if (!integer(f[0], icao, 16) || !integer(f[1], out.mode_s_flags, 16)) {
		return false;
	}
	out.address = icao & 0xFFFFFF;
	switch (icao >> 24) {
		case 0:
			out.source = Source::adsb;
			break;
		case kQualifierAdsbOther:
			out.source = Source::adsb_other;
			break;
		case kQualifierAdsr:
			out.source = Source::adsr;
			break;
		case kQualifierTisbTrackFile:
		case kQualifierTisbOther:
			out.source = Source::tisb;
			break;
		default:
			out.source = Source::other;
			break;
	}
	if (!(f[2].length == 1 && f[2].text[0] == '?')) {
		copy(f[2], out.callsign, sizeof out.callsign);
		for (size_t n = std::strlen(out.callsign);
			 n && out.callsign[n - 1] == ' '; n--) {
			out.callsign[n - 1] = 0;
		}
	}
	if (!(f[3].length == 1 && f[3].text[0] == '?') &&
		!csbee::optional_integer(f[3], out.squawk)) {
		return false;
	}
	out.airborne = out.mode_s_flags & kFlagAirborne;
	out.position_valid = out.mode_s_flags & kFlagPosition;
	out.baro_alt_valid = out.mode_s_flags & kFlagBaroAlt;
	out.gnss_alt_valid = out.mode_s_flags & kFlagGnssAlt;
	out.direction_valid = out.mode_s_flags & kFlagDirection;
	out.speed_valid = out.mode_s_flags & kFlagSpeed;
	out.baro_rate_valid = out.mode_s_flags & kFlagBaroRate;
	out.gnss_rate_valid = out.mode_s_flags & kFlagGnssRate;
	if ((out.position_valid || !f[5].empty() || !f[6].empty()) &&
		!csbee::position(f[5], f[6], out.latitude, out.longitude)) {
		return false;
	}
	return integer(f[4], out.mode_s_category) &&
		   measurement(f[7], out.baro_alt_valid, out.baro_alt_ft) &&
		   measurement(f[8], out.gnss_alt_valid, out.gnss_alt_ft) &&
		   measurement(f[9], out.direction_valid, out.track_deg) &&
		   measurement(f[10], out.speed_valid, out.speed_kt) &&
		   measurement(f[11], out.baro_rate_valid, out.baro_rate_fpm) &&
		   measurement(f[12], out.gnss_rate_valid, out.gnss_rate_fpm) &&
		   integer(f[13], out.mode_s_nicnac, 16) &&
		   integer(f[14], out.mode_s_acdims, 16) &&
		   integer(f[15], out.mode_s_adsb_version) &&
		   integer(f[16], out.rssi_dbm) &&
		   integer(f[17], out.signal_quality_db) &&
		   integer(f[18], out.mode_s_squitter_fps) &&
		   integer(f[19], out.mode_s_es_fps);
}

bool decode_flarm(const char* text, size_t length, Target& out) {
	Field f[kMaxFields];
	if (!starts(text, length, TRAFFIC_FLARM_PREFIX) ||
		!split(text, length, sizeof TRAFFIC_FLARM_PREFIX - 1, kFlarmFields,
			   f)) {
		return false;
	}
	out = Target{};
	out.source = Source::flarm;
	uint8_t id_type = 0, flags = 0;
	if (!integer(f[1], out.address, 16) || out.address > 0xffffff ||
		!integer(f[2], id_type) || !integer(f[3], flags, 16)) {
		return false;
	}
	out.flarm_id_type = FlarmAddressType(id_type & 1);
	out.flarm_stealth = flags & 1;
	out.flarm_no_track = flags & 2;
	out.position_valid = !f[5].empty() || !f[6].empty();
	if (out.position_valid &&
		!csbee::position(f[5], f[6], out.latitude, out.longitude)) {
		return false;
	}
	// FLARM altitude is geometric, in metres above the WGS84 ellipsoid.
	out.gnss_alt_valid = out.direction_valid = out.speed_valid =
		out.gnss_rate_valid = true;
	if (!integer(f[12], out.flarm_movement)) {
		return false;
	}
	out.airborne = out.flarm_movement >= FlarmMovement::flying;
	return integer(f[4], out.flarm_aircraft_type) &&
		   scaled(f[7], units::feet_per_metre, out.gnss_alt_ft) &&
		   decimal(f[8], out.track_deg) &&
		   scaled(f[9], units::knots_per_mps, out.speed_kt) &&
		   scaled(f[10], 60 * units::feet_per_metre, out.gnss_rate_fpm) &&
		   decimal(f[11], out.flarm_turn_rate_dps) &&
		   integer(f[13], out.flarm_urgency) &&
		   decimal(f[14], out.flarm_acc_pos_hor_m) &&
		   decimal(f[15], out.flarm_acc_pos_ver_m) &&
		   decimal(f[16], out.flarm_acc_vel_mps) &&
		   integer(f[17], out.flarm_sil) && integer(f[18], out.flarm_sda) &&
		   integer(f[19], out.flarm_nic) &&
		   integer(f[20], out.flarm_fix_secs) &&
		   integer(f[21], out.flarm_age_ms) &&
		   integer(f[22], out.flarm_version) &&
		   integer(f[23], out.flarm_version_max) &&
		   integer(f[24], out.rssi_dbm) && integer(f[25], out.flarm_frames);
}

bool decode_stats(const char* text, size_t length, Stats& out) {
	Field f[kMaxFields];
	if (!starts(text, length, TRAFFIC_STATS_PREFIX) ||
		!split(text, length, sizeof TRAFFIC_STATS_PREFIX - 1, kStatsFields,
			   f)) {
		return false;
	}
	out = Stats{};
	return integer(f[0], out.version) && integer(f[1], out.demods_per_second) &&
		   integer(f[2], out.raw_squitter_fps) &&
		   integer(f[3], out.squitter_fps) && integer(f[4], out.raw_es_fps) &&
		   integer(f[5], out.es_fps) && integer(f[6], out.raw_uat_fps) &&
		   integer(f[7], out.uat_fps) && integer(f[8], out.aircraft) &&
		   integer(f[9], out.tscal) && integer(f[10], out.uptime_s);
}

bool relative_position(const Target& target, float ref_latitude,
					   float ref_longitude, float& distance_nm,
					   float& bearing_deg) {
	if (!target.position_valid) {
		return false;
	}
	const auto position = navigation::great_circle(
		ref_latitude, ref_longitude, target.latitude, target.longitude);
	distance_nm = position.distance_nm;
	bearing_deg = position.bearing_deg;
	return true;
}

namespace {
int distinct_fresh_position_ages(const Tracked& tracked, int window_s,
								 int* moved) {
	int count = 0;
	for (int age = 0; age < window_s && age < kHistorySeconds; age++) {
		const Position& at = tracked.past[age];
		const bool same =
			count > 0 &&
			at.latitude == tracked.past[moved[count - 1]].latitude &&
			at.longitude == tracked.past[moved[count - 1]].longitude;
		if (at.sample == Sample::fresh && !same) {
			moved[count++] = age;
		}
	}
	return count;
}
}

float turn_rate_dps(const Tracked& tracked) {
	float rate = 0;
	if (tracked.target.source == Source::flarm) {
		rate = tracked.target.flarm_turn_rate_dps;
	} else {
		int moved[kTurnWindowS];
		const int count =
			distinct_fresh_position_ages(tracked, kTurnWindowS, moved);
		if (count < 3) {
			return 0;
		}
		// Two legs that share the place in the middle, as long as the window
		// allows: the noise of a place weighs less on a long leg. The course
		// of a leg is the one in its middle, so the two lie half the window
		// apart.
		const int newest = moved[0];
		const int middle = moved[count / 2];
		const int oldest = moved[count - 1];
		const auto course = [&](int from, int to) {
			return navigation::great_circle(tracked.past[from].latitude,
											tracked.past[from].longitude,
											tracked.past[to].latitude,
											tracked.past[to].longitude)
				.bearing_deg;
		};
		const float turned = std::remainder(
			course(middle, newest) - course(oldest, middle), 360.0f);
		rate = turned / (static_cast<float>(oldest - newest) / 2.0f);
	}
	return std::fabs(rate) < kTurnDeadBandDps ? 0 : rate;
}

Motion motion(const Tracked& tracked) {
	const Target& target = tracked.target;
	Motion out{.speed_valid = target.speed_valid,
			   .direction_valid = target.direction_valid,
			   .speed_kt = static_cast<float>(target.speed_kt),
			   .track_deg = target.track_deg};
	int moved[kMotionWindowS];
	const int count =
		distinct_fresh_position_ages(tracked, kMotionWindowS, moved);
	if ((!out.speed_valid || !out.direction_valid) && count >= 2) {
		const Position& newest = tracked.past[moved[0]];
		const Position& oldest = tracked.past[moved[count - 1]];
		auto [distance_nm, bearing_deg] =
			navigation::great_circle(oldest.latitude, oldest.longitude,
									 newest.latitude, newest.longitude);
		if (!out.speed_valid) {
			out.speed_kt = distance_nm * 3600 /
						   static_cast<float>(moved[count - 1] - moved[0]);
			out.speed_valid = true;
		}
		if (!out.direction_valid) {
			out.track_deg = bearing_deg;
			out.direction_valid = true;
		}
	}
	if (target.baro_rate_valid || target.gnss_rate_valid) {
		out.vertical_fpm = target.baro_rate_valid ? target.baro_rate_fpm
												  : target.gnss_rate_fpm;
		return out;
	}
	if (!target.baro_alt_valid && !target.gnss_alt_valid) {
		return out;
	}
	const auto datum =
		target.baro_alt_valid ? AltitudeDatum::barometric : AltitudeDatum::gnss;
	int newest = -1, oldest = -1;
	for (int age = 0; age < kMotionWindowS; age++) {
		const Position& position = tracked.past[age];
		if (position.sample == Sample::fresh &&
			position.altitude_datum == datum) {
			newest = newest < 0 ? age : newest;
			oldest = age;
		}
	}
	if (newest >= 0 && oldest - newest >= kRateSpanS) {
		const int64_t climbed_ft = int64_t{tracked.past[newest].altitude_ft} -
								   tracked.past[oldest].altitude_ft;
		out.vertical_fpm = static_cast<float>(climbed_ft) * 60 /
						   static_cast<float>(oldest - newest);
	}
	return out;
}

const char* source_name(Source source) {
	switch (source) {
		case Source::adsb:
			return "1090 ADS-B";
		case Source::adsb_other:
			return "1090 ADS-B OTHER";
		case Source::adsr:
			return "ADS-R";
		case Source::tisb:
			return "TIS-B";
		case Source::flarm:
			return "FLARM";
		default:
			return "1090 OTHER";
	}
}

// ADSBee emitter category codes map onto the DO-260 set and class letter: 1..7
// are A1..A7, 9..15 are B1..B7, 17..20 are C1..C4. 0 is A0, no information.
void category_name(uint8_t category, char out[4]) {
	if (category == 0) {
		(void)std::snprintf(out, 4, "A0");
	} else if (category <= 7) {
		(void)std::snprintf(out, 4, "A%u", category);
	} else if (category >= 9 && category <= 15) {
		(void)std::snprintf(out, 4, "B%u", category - 8);
	} else if (category >= 17 && category <= 20) {
		(void)std::snprintf(out, 4, "C%u", category - 16);
	} else {
		(void)std::snprintf(out, 4, "%u", category);
	}
}

const char* flarm_aircraft_name(uint8_t aircraft_type) {
	static const char* const names[] = {
		"UNDEFINED",
		"GLIDER",
		"TOWPLANE",
		"HELICOPTER",
		"PARACHUTE",
		"DROPPLANE",
		"HANGGLIDER",
		"PARAGLIDER",
		"SINGLE-ENGINE PISTON",
		"HEAVY/COMPLEX FIXED WING",
		"GYROCOPTER",
		"BALLOON",
		"AIRSHIP",
		"UAV",
		"AIRFIELD",
		"STATIC OBSTACLE",
		"EVTOL/UAM",
		"UAV OPEN",
		"UAV SPECIFIC",
		"UAV CERTIFIED",
	};
	return aircraft_type < sizeof names / sizeof names[0] ? names[aircraft_type]
														  : "?";
}

const char* flarm_id_name(FlarmAddressType id) {
	return id == FlarmAddressType::icao ? "ICAO" : "SYNTHETIC";
}

const char* flarm_movement_name(FlarmMovement movement) {
	static const char* const names[] = {"UNKNOWN", "GROUND", "FLYING",
										"CIRCLING"};
	const auto code = static_cast<uint8_t>(movement);
	return code < 4 ? names[code] : "?";
}

const char* flarm_urgency_name(FlarmUrgency urgency) {
	static const char* const names[] = {"NORMAL", "HIGH", "PAN-PAN", "MAYDAY"};
	const auto code = static_cast<uint8_t>(urgency);
	return code < 4 ? names[code] : "?";
}

void flag_words(uint32_t flags, char* out, size_t capacity) {
	static const struct {
		uint32_t bit;
		const char* word;
	} words[] = {
		{1u << 0, "AIRBORNE"},
		{1u << 8, "MILITARY"},
		{1u << 9, "B2 GROUND VEHICLE"},
		{1u << 10, "1090ES IN"},
		{1u << 11, "UAT IN"},
		{1u << 12, "TCAS"},
		{1u << 13, "SINGLE ANTENNA"},
		{1u << 14, "HEADING"},
		{1u << 15, "MAGNETIC"},
		{1u << 16, "IDENT"},
		{1u << 17, "ALERT"},
		{1u << 18, "TCAS RA"},
		{1u << 19, "NON-TRANSPONDER"},
	};
	out[0] = 0;
	for (const auto& w : words) {
		if (flags & w.bit) {
			append(out, capacity, w.word);
		}
	}
}

Integrity decode_nicnac(uint16_t nicnac) {
	return Integrity{
		static_cast<uint8_t>(nicnac & 0xF),
		static_cast<uint8_t>((nicnac >> 4) & 1),
		static_cast<uint8_t>((nicnac >> 8) & 0xF),
		static_cast<uint8_t>((nicnac >> 5) & 7),
		static_cast<uint8_t>((nicnac >> 14) & 3),
		static_cast<uint8_t>((nicnac >> 12) & 3),
	};
}

Dimensions decode_acdims(uint32_t acdims) {
	return Dimensions{
		static_cast<uint8_t>(acdims & 3),
		static_cast<uint8_t>((acdims >> 2) & 0x7F),
		static_cast<uint8_t>((acdims >> 9) & 0x7F),
		static_cast<int8_t>((acdims >> 16) & 0xFF),
	};
}
}
