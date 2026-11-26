#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

#include "display/view_state.hpp"

// Shared stored-settings layout for touch display variants. Layout changes
// follow the stored-format policy in settings/settings_model.hpp.
namespace display {
inline constexpr uint16_t kSettingsVersion = 1;

enum class Theme : uint8_t { light, dark };
enum class SymbolStyle : uint8_t { planes, simple };

template <bool SupportsSymbolStyle>
struct TouchSettings {
	static constexpr bool supports_symbol_style = SupportsSymbolStyle;
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

static_assert(sizeof(TouchSettings<true>) == 12 &&
			  std::has_unique_object_representations_v<TouchSettings<true>>);
static_assert(offsetof(TouchSettings<true>, theme) == 2 &&
			  offsetof(TouchSettings<true>, language) == 3 &&
			  offsetof(TouchSettings<true>, reserved_brightness) == 6 &&
			  offsetof(TouchSettings<true>, radar_range_index) == 8 &&
			  offsetof(TouchSettings<true>, symbol_style) == 11);
static_assert(sizeof(TouchSettings<false>) == sizeof(TouchSettings<true>) &&
			  std::has_unique_object_representations_v<TouchSettings<false>>);
static_assert(RANGES_NM[TouchSettings<true>{}.radar_range_index] == 4);

template <bool SupportsSymbolStyle>
constexpr bool valid(const TouchSettings<SupportsSymbolStyle>& settings) {
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
