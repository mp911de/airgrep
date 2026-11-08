#pragma once

#include <cstddef>
#include <cstdint>

// The settings struct and its evolution, hardware free so the migrator has a host test.
// The struct is stored as one blob, version first. Changing any field after a layout was
// flashed means a new version and a migration step, see the project CLAUDE.md.
namespace settings {
#define SETTINGS_VERSION 1
#define SETTINGS_TZ_RULE_SIZE 64
#define SETTINGS_DEFAULT_TZ_RULE "UTC0"
#define SETTINGS_DEFAULT_TZ_DETECT 1

	struct Settings {
		uint16_t version;
		char tz_rule[SETTINGS_TZ_RULE_SIZE]; // POSIX TZ string, applied with setenv and tzset
		uint8_t tz_detect; // 1: pick the zone from the GNSS position, tz_rule is the fallback
		uint8_t reserved_; // explicit padding, keeps the blob layout independent of the compiler
	};

	static_assert(sizeof(Settings) == 68, "layout changed, bump SETTINGS_VERSION and migrate");

	Settings defaults();

	// Converts a stored blob of any known version to the current layout.
	// False when the size or version is unknown, out is then untouched.
	bool migrate(const void* blob, size_t size, Settings& out);

	enum class Type : uint8_t { text, boolean };

	struct Field {
		const char* name;
		Type type;
		size_t offset, size;
	};

	const Field* fields(size_t& count);
	const Field* find(const char* name);

	// Assigns console text to a field: trims surrounding whitespace, rejects values that do
	// not fit. Booleans take on, off, true, false, 1, 0. False on unknown field or bad value.
	bool assign(Settings& settings, const char* name, const char* value);
	// Formats a field for the console. Returns the characters written.
	size_t format(const Settings& settings, const Field& field, char* out, size_t capacity);
}
