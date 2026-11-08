#include "traffic_protocol.hpp"
#include "adsbee/csbee_fields.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace traffic {
	namespace {
		constexpr size_t kAircraftFields = 20;
		constexpr size_t kFlarmFields = 26;
		constexpr size_t kStatsFields = 11;
		constexpr size_t kMaxFields = 26;
		constexpr float kFeetPerMetre = 3.28084f;
		constexpr float kKnotsPerMps = 1.943844f;
		constexpr float kEarthRadiusNm = 3440.065f;
		constexpr float kDegToRad = 3.14159265f / 180;

		// ADSBee BitFlag positions.
		constexpr uint32_t kFlagAirborne = 1u << 0;
		constexpr uint32_t kFlagBaroAlt = 1u << 1;
		constexpr uint32_t kFlagGnssAlt = 1u << 2;
		constexpr uint32_t kFlagPosition = 1u << 3;
		constexpr uint32_t kFlagDirection = 1u << 4;
		constexpr uint32_t kFlagSpeed = 1u << 5;
		constexpr uint32_t kFlagBaroRate = 1u << 6;
		constexpr uint32_t kFlagGnssRate = 1u << 7;

		// Address qualifiers the ADSBee folds into bits 24 to 27 of the ICAO field. ICAO
		// addressed targets carry none, whatever message type delivered them.
		constexpr unsigned kQualifierAdsbOther = 5, kQualifierAdsr = 6;
		constexpr unsigned kQualifierTisbTrackFile = 7, kQualifierTisbOther = 8;

		using csbee::Field;
		using csbee::split;
		using csbee::copy;
		using csbee::integer;
		using csbee::hex;
		using csbee::decimal;

		bool starts(const char* text, size_t length, const char* prefix) {
			size_t n = std::strlen(prefix);
			return length >= n && std::memcmp(text, prefix, n) == 0;
		}

		void append(char* out, size_t capacity, const char* word) {
			size_t used = std::strlen(out);
			std::snprintf(out + used, capacity - used, "%s%s", used ? ", " : "", word);
		}
	}

	Line classify(const char* text, size_t length) {
		if (starts(text, length, TRAFFIC_AIRCRAFT_PREFIX)) return Line::aircraft;
		if (starts(text, length, TRAFFIC_FLARM_PREFIX)) return Line::flarm;
		if (starts(text, length, TRAFFIC_STATS_PREFIX)) return Line::stats;
		return Line::other;
	}

	bool decode_aircraft(const char* text, size_t length, Target& out) {
		Field f[kMaxFields];
		if (!starts(text, length, TRAFFIC_AIRCRAFT_PREFIX) ||
			!split(text, length, sizeof TRAFFIC_AIRCRAFT_PREFIX - 1, kAircraftFields, f))
			return false;
		out = Target{};
		unsigned long icao = hex(f[0]);
		out.address = icao & 0xFFFFFF;
		switch (icao >> 24) {
		case 0: out.source = Source::adsb;
			break;
		case kQualifierAdsbOther: out.source = Source::adsb_other;
			break;
		case kQualifierAdsr: out.source = Source::adsr;
			break;
		case kQualifierTisbTrackFile:
		case kQualifierTisbOther: out.source = Source::tisb;
			break;
		default: out.source = Source::other;
			break;
		}
		out.flags = uint32_t(hex(f[1]));
		if (!(f[2].length == 1 && f[2].text[0] == '?')) {
			copy(f[2], out.callsign, sizeof out.callsign);
			for (size_t n = std::strlen(out.callsign); n && out.callsign[n - 1] == ' '; n
				--)
				out.callsign[n - 1] = 0;
		}
		out.squawk = f[3].length == 1 && f[3].text[0] == '?'
						? -1
						: int16_t(integer(f[3]));
		out.category = uint8_t(integer(f[4]));
		out.airborne = out.flags & kFlagAirborne;
		out.position_valid = out.flags & kFlagPosition;
		out.latitude = decimal(f[5]);
		out.longitude = decimal(f[6]);
		out.baro_alt_valid = out.flags & kFlagBaroAlt;
		out.baro_alt_ft = int32_t(integer(f[7]));
		out.gnss_alt_valid = out.flags & kFlagGnssAlt;
		out.gnss_alt_ft = int32_t(integer(f[8]));
		out.direction_valid = out.flags & kFlagDirection;
		out.track_deg = decimal(f[9]);
		out.speed_valid = out.flags & kFlagSpeed;
		out.speed_kt = int16_t(integer(f[10]));
		out.baro_rate_valid = out.flags & kFlagBaroRate;
		out.baro_rate_fpm = int16_t(integer(f[11]));
		out.gnss_rate_valid = out.flags & kFlagGnssRate;
		out.gnss_rate_fpm = int16_t(integer(f[12]));
		out.nicnac = uint16_t(hex(f[13]));
		out.acdims = uint32_t(hex(f[14]));
		out.adsb_version = uint8_t(integer(f[15]));
		out.rssi_dbm = int8_t(integer(f[16]));
		out.signal_quality_db = uint8_t(integer(f[17]));
		out.squitter_fps = uint16_t(integer(f[18]));
		out.es_fps = uint16_t(integer(f[19]));
		return true;
	}

	bool decode_flarm(const char* text, size_t length, Target& out) {
		Field f[kMaxFields];
		if (!starts(text, length, TRAFFIC_FLARM_PREFIX) ||
			!split(text, length, sizeof TRAFFIC_FLARM_PREFIX - 1, kFlarmFields, f))
			return false;
		out = Target{};
		out.source = Source::flarm;
		out.address = uint32_t(hex(f[1])) & 0xFFFFFF;
		out.id_type = FlarmAddressType(integer(f[2]) & 1);
		unsigned flags = unsigned(hex(f[3]));
		out.stealth = flags & 1;
		out.no_track = flags & 2;
		out.aircraft_type = uint8_t(integer(f[4]));
		out.position_valid = !f[5].empty() && !f[6].empty();
		out.latitude = decimal(f[5]);
		out.longitude = decimal(f[6]);
		// FLARM altitude is metres above the WGS84 ellipsoid, a geometric altitude.
		out.gnss_alt_valid = true;
		out.gnss_alt_ft = int32_t(std::lround(integer(f[7]) * kFeetPerMetre));
		out.direction_valid = true;
		out.track_deg = decimal(f[8]);
		out.speed_valid = true;
		out.speed_kt = int16_t(std::lround(decimal(f[9]) * kKnotsPerMps));
		out.gnss_rate_valid = true;
		out.gnss_rate_fpm = int16_t(std::lround(decimal(f[10]) * kFeetPerMetre * 60));
		out.turn_rate_dps = decimal(f[11]);
		out.movement = uint8_t(integer(f[12]));
		out.airborne = out.movement >= 2;
		out.urgency = uint8_t(integer(f[13]));
		out.acc_pos_hor_m = decimal(f[14]);
		out.acc_pos_ver_m = decimal(f[15]);
		out.acc_vel_mps = decimal(f[16]);
		out.sil = uint8_t(integer(f[17]));
		out.sda = uint8_t(integer(f[18]));
		out.nic = uint8_t(integer(f[19]));
		out.fix_secs = uint32_t(integer(f[20]));
		out.age_ms = uint32_t(integer(f[21]));
		out.version = uint8_t(integer(f[22]));
		out.version_max = uint8_t(integer(f[23]));
		out.rssi_dbm = int8_t(integer(f[24]));
		out.frames = uint16_t(integer(f[25]));
		return true;
	}

	bool decode_stats(const char* text, size_t length, Stats& out) {
		Field f[kMaxFields];
		if (!starts(text, length, TRAFFIC_STATS_PREFIX) ||
			!split(text, length, sizeof TRAFFIC_STATS_PREFIX - 1, kStatsFields, f))
			return false;
		out = Stats{};
		out.version = uint8_t(integer(f[0]));
		out.demods_per_second = uint16_t(integer(f[1]));
		out.raw_squitter_fps = uint16_t(integer(f[2]));
		out.squitter_fps = uint16_t(integer(f[3]));
		out.raw_es_fps = uint16_t(integer(f[4]));
		out.es_fps = uint16_t(integer(f[5]));
		out.raw_uat_fps = uint16_t(integer(f[6]));
		out.uat_fps = uint16_t(integer(f[7]));
		out.aircraft = uint16_t(integer(f[8]));
		out.tscal = uint32_t(integer(f[9]));
		out.uptime_s = uint32_t(integer(f[10]));
		return true;
	}

	bool relative(const Target& target, float ref_latitude, float ref_longitude,
				float& distance_nm, float& bearing_deg) {
		return target.position_valid && relative(target.latitude, target.longitude,
												ref_latitude, ref_longitude, distance_nm,
												bearing_deg);
	}

	bool relative(float latitude, float longitude, float ref_latitude,
				float ref_longitude,
				float& distance_nm, float& bearing_deg) {
		float lat1 = ref_latitude * kDegToRad, lat2 = latitude * kDegToRad;
		float dlat = lat2 - lat1, dlon = (longitude - ref_longitude) * kDegToRad;
		float a = std::sin(dlat / 2) * std::sin(dlat / 2) +
			std::cos(lat1) * std::cos(lat2) * std::sin(dlon / 2) * std::sin(dlon / 2);
		distance_nm = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a)) * kEarthRadiusNm;
		float y = std::sin(dlon) * std::cos(lat2);
		float x = std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) *
			std::cos(dlon);
		bearing_deg = std::atan2(y, x) / kDegToRad;
		if (bearing_deg < 0) bearing_deg += 360;
		return true;
	}

	const char* source_name(Source source) {
		switch (source) {
		case Source::adsb: return "1090 ADS-B";
		case Source::adsb_other: return "1090 ADS-B OTHER";
		case Source::adsr: return "ADS-R";
		case Source::tisb: return "TIS-B";
		case Source::flarm: return "FLARM";
		default: return "1090 OTHER";
		}
	}

	// ADSBee emitter category codes map onto the DO-260 set and class letter: 1..7 are A1..A7,
	// 9..15 are B1..B7, 17..20 are C1..C4. 0 is A0, no information.
	void category_name(uint8_t category, char out[4]) {
		if (category == 0) std::strcpy(out, "A0");
		else if (category <= 7) std::snprintf(out, 4, "A%u", category);
		else if (category >= 9 && category <= 15) std::snprintf(
			out, 4, "B%u", category - 8);
		else if (category >= 17 && category <= 20) std::snprintf(
			out, 4, "C%u", category - 16);
		else std::snprintf(out, 4, "%u", category);
	}

	const char* flarm_aircraft_name(uint8_t aircraft_type) {
		static const char* const names[] = {
			"UNDEFINED", "GLIDER", "TOWPLANE", "HELICOPTER", "PARACHUTE", "DROPPLANE",
			"HANGGLIDER", "PARAGLIDER", "SINGLE-ENGINE PISTON",
			"HEAVY/COMPLEX FIXED WING",
			"GYROCOPTER", "BALLOON", "AIRSHIP", "UAV", "AIRFIELD", "STATIC OBSTACLE",
			"EVTOL/UAM", "UAV OPEN", "UAV SPECIFIC", "UAV CERTIFIED",
		};
		return aircraft_type < sizeof names / sizeof names[0]
					? names[aircraft_type]
					: "?";
	}

	const char* flarm_id_name(FlarmAddressType id) {
		return id == FlarmAddressType::icao ? "ICAO" : "SYNTHETIC";
	}

	const char* flarm_movement_name(uint8_t movement) {
		static const char* const names[] = {"UNKNOWN", "GROUND", "FLYING", "CIRCLING"};
		return movement < 4 ? names[movement] : "?";
	}

	const char* flarm_urgency_name(uint8_t urgency) {
		static const char* const names[] = {"NORMAL", "HIGH", "PAN-PAN", "MAYDAY"};
		return urgency < 4 ? names[urgency] : "?";
	}

	void flag_words(uint32_t flags, char* out, size_t capacity) {
		static const struct {
			uint32_t bit;
			const char* word;
		} words[] = {
			{1u << 0, "AIRBORNE"}, {1u << 8, "MILITARY"}, {1u << 9, "B2 GROUND VEHICLE"},
			{1u << 10, "1090ES IN"}, {1u << 11, "UAT IN"}, {1u << 12, "TCAS"},
			{1u << 13, "SINGLE ANTENNA"}, {1u << 14, "HEADING"}, {1u << 15, "MAGNETIC"},
			{1u << 16, "IDENT"}, {1u << 17, "ALERT"}, {1u << 18, "TCAS RA"},
			{1u << 19, "NON-TRANSPONDER"},
		};
		out[0] = 0;
		for (const auto& w : words)
			if (flags & w.bit) append(out, capacity, w.word);
	}

	Integrity decode_nicnac(uint16_t nicnac) {
		return Integrity{
			uint8_t(nicnac & 0xF), uint8_t((nicnac >> 4) & 1),
			uint8_t((nicnac >> 8) & 0xF),
			uint8_t((nicnac >> 5) & 7), uint8_t((nicnac >> 14) & 3),
			uint8_t((nicnac >> 12) & 3),
		};
	}

	Dimensions decode_acdims(uint32_t acdims) {
		return Dimensions{
			uint8_t(acdims & 3), uint8_t((acdims >> 2) & 0x7F),
			uint8_t((acdims >> 9) & 0x7F),
			int8_t((acdims >> 16) & 0xFF),
		};
	}
}
