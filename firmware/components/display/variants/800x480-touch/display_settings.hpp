#pragma once

#include "display/touch_settings.hpp"

namespace display::inline touch_800x480 {
// The key of the blob in NVS, and what names it to READ_SETTINGS and
// WRITE_SETTINGS. NVS takes 15 characters.
inline constexpr char kSettingsKey[] = "800x480-touch";
static_assert(sizeof kSettingsKey <= 16);

using display::kSettingsVersion;
using display::SymbolStyle;
using display::Theme;
using Settings = display::TouchSettings<true>;
using display::valid;
}
