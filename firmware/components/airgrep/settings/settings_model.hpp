#pragma once

#include <cstddef>
#include <cstdint>

// The settings struct and its evolution, hardware free so the migrator has a
// host test. The struct is stored as one blob, version first.
//
// A layout that was rolled out and flashed is kept readable. To change the
// position, type, unit or meaning of a field, to remove one, or to append
// one, which changes the size of the blob: ask Mark whether the version goes
// up, and add the migration step that converts the layout before it.
namespace settings {
#define SETTINGS_VERSION 1
// The key of the blob in NVS, and what names it to READ_SETTINGS and
// WRITE_SETTINGS over USB.
#define SETTINGS_NVS_KEY "settings"
#define SETTINGS_ZONE_SIZE 64
#define SETTINGS_CALLSIGN_SIZE 9
#define SETTINGS_DEFAULT_TZ_DETECT 1
#define SETTINGS_DEFAULT_BATTERY_MAH 5000
#define SETTINGS_DEFAULT_ALTITUDE_FILTER_FT 3000
#define SETTINGS_MAX_ALTITUDE_FILTER_FT 9999
#define SETTINGS_NO_ADDRESS 0xFFFFFFFFu

struct Settings {
	uint16_t version;
	// IANA name of the chosen zone, as the tz data lists it. Its rule is
	// looked up there, so new tz data applies to it. Empty is UTC.
	char zone[SETTINGS_ZONE_SIZE];
	uint8_t tz_detect;
	// Held the theme, the track display and the radar range. The settings of
	// a display took their place, the bytes keep the layout of version 1.
	uint8_t reserved_display[3];
	uint16_t battery_mah;
	uint32_t own_address;  // 24 bit ICAO address, or SETTINGS_NO_ADDRESS
	char own_callsign[SETTINGS_CALLSIGN_SIZE];  // empty when none is stored
	// Held the look-ahead in minutes. The conflict engine of the display took
	// its place, the byte keeps the layout of version 1.
	uint8_t reserved;
	// Traffic further above or below than this is not shown. 0: no filter.
	uint16_t altitude_filter_ft;
};

static_assert(sizeof(Settings) == 88,
			  "layout changed, bump SETTINGS_VERSION and migrate");
static_assert(offsetof(Settings, battery_mah) == 70 &&
			  offsetof(Settings, own_address) == 72 &&
			  offsetof(Settings, reserved) == 85 &&
			  offsetof(Settings, altitude_filter_ft) == 86);

Settings defaults();

bool valid(const Settings& settings);

// Converts a stored blob of any known version to the current layout.
// False when the size or version is unknown, out is then untouched.
bool migrate(const void* blob, size_t size, Settings& out);

enum class Type : uint8_t { text, boolean, number, address };

struct Field {
	const char* name{};
	Type type{};
	size_t offset{}, size{};
	uint32_t largest = 0;
};

const Field* fields(size_t& count);
const Field* find(const char* name);

// Assigns console text to a field: trims surrounding whitespace, rejects
// values that do not fit. Booleans take on, off, true, false, 1, 0. Numbers
// take what their field holds. False on unknown field or bad value.
// An address is hexadecimal within 24 bits, or NONE.
bool assign(Settings& settings, const char* name, const char* value);
size_t format(const Settings& settings, const Field& field, char* out,
			  size_t capacity);
}
