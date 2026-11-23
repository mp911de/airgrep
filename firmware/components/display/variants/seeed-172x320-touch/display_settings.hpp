#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

#include "display/view_state.hpp"

// What the user chose on this display. The struct is stored as one blob under
// its key, version first, and a host reads and writes the same bytes over USB.
// Layout changes follow the stored-format policy in
// settings/settings_model.hpp.
namespace display::inline touch_172x320 {
// The key of the blob in NVS, and what names it to READ_SETTINGS and
// WRITE_SETTINGS. NVS takes 15 characters.
inline constexpr char kSettingsKey[] = "172x320-touch";
static_assert(sizeof kSettingsKey <= 16);
inline constexpr uint16_t kSettingsVersion = 1;

enum class Theme : uint8_t { light, dark };
enum class SymbolStyle : uint8_t { planes, simple };

struct Settings {
	static constexpr bool supports_symbol_style = false;
	uint16_t version = kSettingsVersion;
	Theme theme = Theme::light;
	char language[3] = "en";
	uint8_t reserved_brightness = 255;
	uint8_t reserved_auto_brightness = 0;
	uint8_t radar_range_index = 1;
	uint8_t tracks = 1;
	uint8_t conflict_indicator = 0;
	SymbolStyle symbol_style = SymbolStyle::planes;
};

static_assert(sizeof(Settings) == 12 &&
			  std::has_unique_object_representations_v<Settings>);
static_assert(offsetof(Settings, theme) == 2 &&
			  offsetof(Settings, language) == 3 &&
			  offsetof(Settings, reserved_brightness) == 6 &&
			  offsetof(Settings, radar_range_index) == 8 &&
			  offsetof(Settings, symbol_style) == 11);
static_assert(RANGES_NM[Settings{}.radar_range_index] == 4);

constexpr bool valid(const Settings& settings) {
	const auto letter = [](char c) { return c >= 'a' && c <= 'z'; };
	return settings.version == kSettingsVersion &&
		   settings.theme <= Theme::dark && letter(settings.language[0]) &&
		   letter(settings.language[1]) && !settings.language[2] &&
		   settings.reserved_auto_brightness <= 1 &&
		   settings.radar_range_index < std::size(RANGES_NM) &&
		   settings.tracks <= 1 && settings.conflict_indicator <= 1 &&
		   settings.symbol_style <= SymbolStyle::simple;
}
}
