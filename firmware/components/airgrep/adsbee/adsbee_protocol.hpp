#pragma once

#include <cstddef>
#include <cstdint>

#include "device.h"

namespace adsbee {
#define ADSBEE_RX_TIMEOUT_MS 3000

bool valid_csbee(const char* text, size_t length);
// A command fits the buffer with its terminator and carries no line break
// itself.
bool acceptable_command(const char* command, size_t capacity);

struct Reply {
	char text[2048]{};
	uint16_t length = 0;
	bool append(const char* line, size_t size);
};

enum class FixQuality : int8_t { unreported = -1, none, gps, differential };
enum class AntennaStatus : int8_t {
	unreported = -1,
	initializing,
	unknown,
	ok,
	shorted,
	open
};
enum class AntennaPower : int8_t { unreported = -1, off, on, unknown };
enum class JammingStatus : int8_t {
	unreported = -1,
	unknown,
	none,
	warning,
	critical
};

enum class Fix : uint8_t { none, fix_2d, fix_3d };

struct Gnss {
	Fix fix = Fix::none;
	FixQuality quality = FixQuality::unreported;
	int8_t satellites_used = -1;
	float latitude = 0, longitude = 0;  // degrees, north and east positive
	int32_t altitude_ft = 0;
	float speed_kt = 0, track_deg = 0;
	float hdop = -1;
	bool utc_known = false;
	uint8_t utc_hour = 0, utc_minute = 0, utc_second = 0;
	uint64_t epoch_ms = 0;
	AntennaStatus antenna = AntennaStatus::unreported;
	AntennaPower antenna_power = AntennaPower::unreported;
	JammingStatus jamming = JammingStatus::unreported;
	int16_t max_cno_dbhz = -1;
	int16_t seen = -1, no_signal = -1, searching = -1, acquired = -1,
			unusable = -1, locked = -1, ephemeris = -1, used = -1, healthy = -1,
			unhealthy = -1;
	char sbas_mode[10]{}, sbas_system[10]{};
	int16_t sbas_satellites = -1;
	bool sbas_corrections = false;
	int32_t prc_cm = 0, ic_cm = 0;
	char module[16]{}, software[32]{}, hardware[16]{};
	bool bbr = false;
};

// Parses a CRC-checked #GNSS frame. Use the report only on success.
bool decode_gnss(const char* text, size_t length, Gnss& out);

enum class Ingestion : uint8_t { read = 0, ignore = 1 };

struct StateData {
	bool known = false, tx_attached = false, initialized = false;
	int64_t updated_us = 0, reply_us = 0, error_us = 0;
	EspErrorCode last_error = 0;
	uint32_t telemetry_lines = 0, crc_failures = 0, parity_errors = 0,
			 framing_errors = 0, overruns = 0;
	Gnss gnss;
	int64_t last_gnss_report_us = 0;
	Ingestion traffic_ingestion = Ingestion::read;
	Ingestion gnss_ingestion = Ingestion::read;

	// Diagnostics can still inspect gnss after the last report expires.
	const Gnss& gnss_at(int64_t now_us) const {
		static constexpr Gnss unavailable{};
		return state_usable(last_gnss_report_us, now_us) ? gnss : unavailable;
	}
};

namespace timesync {
struct StateData {
	bool clock_set_from_gnss = false;
	bool burst_running = false;
	int64_t last_sync_us = 0;
	int64_t last_correction_us = 0;
	int64_t rtt_us = 0;
	uint32_t pps = 0;
	uint8_t burst_replies = 0;
	EspErrorCode last_error = 0;
	int64_t error_us = 0;
};
}

// The TX gate. The ADSBee is powered in mode ON only, and TX may drive it only
// while it sends: a frame received since power-up, at most 3 s ago.
class Link {
   public:
	static constexpr int64_t kRxTimeoutUs =
		int64_t{ADSBEE_RX_TIMEOUT_MS} * 1000;
	void set_powered(bool powered, int64_t now_us);
	void received(int64_t received_us);
	bool permits_tx(int64_t now_us) const;

   private:
	int64_t powered_since_us_ = 0, received_us_ = 0;
};
}
