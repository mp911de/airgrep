#pragma once

#include <cstddef>
#include <cstdint>

namespace adsbee {
#define ADSBEE_RX_TIMEOUT_MS 3000
#define ADSBEE_LIVENESS_STALE_MS 500

	bool valid_csbee(const char* text, size_t length);
	// A command fits the buffer with its terminator and carries no line break itself.
	bool acceptable_command(const char* command, size_t capacity);

	struct Reply {
		char text[2048]{};
		uint16_t length = 0;
		bool append(const char* line, size_t size);
	};

	// One #GNSS frame, the ADSBee's receiver state pushed once per second. Counts and codes
	// are -1 when the frame left the field empty. Fix fields are only valid with fix set.
	struct Gnss {
		bool fix = false;
		int8_t quality = -1; // GGA: 0 none, 1 GPS, 2 DGPS/SBAS
		int8_t satellites = -1; // used in the fix
		float latitude = 0, longitude = 0; // degrees, north and east positive
		int32_t altitude_ft = 0;
		float speed_kt = 0, track_deg = 0;
		float hdop = -1;
		bool utc_known = false;
		uint8_t utc_hour = 0, utc_minute = 0, utc_second = 0;
		uint64_t epoch_ms = 0;
		int8_t antenna = -1; // MON-RF: INIT DONTKNOW OK SHORT OPEN
		int8_t antenna_power = -1; // MON-RF: OFF ON DONTKNOW
		int8_t jamming = -1; // MON-RF: UNKNOWN NONE WARNING CRITICAL
		int16_t max_cno = -1; // best C/N0 in dBHz
		int16_t seen = -1, no_signal = -1, searching = -1, acquired = -1, unusable = -1,
				locked = -1,
				ephemeris = -1, used = -1, healthy = -1, unhealthy = -1;
		char sbas_mode[10]{}, sbas_system[10]{};
		int16_t sbas_satellites = -1;
		bool sbas_corrections = false;
		int32_t prc_cm = 0, ic_cm = 0;
		char module[16]{}, software[32]{}, hardware[16]{};
		bool bbr = false;
	};

	// Parses a CRC-checked #GNSS frame. False when the prefix or the field count is wrong.
	bool decode_gnss(const char* text, size_t length, Gnss& out);

	struct Power {
		bool on = false;
		int64_t updated_us = 0, error_us = 0;
		bool permits_tx(int64_t now_us) const;
	};

	class Link {
	public:
		static constexpr int64_t kRxTimeoutUs = ADSBEE_RX_TIMEOUT_MS * 1000;
		static constexpr int64_t kStaleTimeoutUs = ADSBEE_LIVENESS_STALE_MS * 1000;
		void update_power(Power power, int64_t now_us);
		void received(int64_t received_us);
		void poll(int64_t now_us);
		bool permits_tx(int64_t now_us) const;

	private:
		Power power_;
		int64_t powered_since_us_ = 0, received_us_ = 0;
	};
}
