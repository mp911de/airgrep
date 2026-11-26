#pragma once

#include "display/touch_settings.hpp"

// What the user chose on this display. The struct is stored as one blob under
// its key, version first, and a host reads and writes the same bytes over USB.
// Layout changes follow the stored-format policy in
// settings/settings_model.hpp.
namespace display::inline touch_172x320 {
// The key of the blob in NVS, and what names it to READ_SETTINGS and
// WRITE_SETTINGS. NVS takes 15 characters.
inline constexpr char kSettingsKey[] = "172x320-touch";
static_assert(sizeof kSettingsKey <= 16);

using display::kSettingsVersion;
using display::SymbolStyle;
using display::Theme;
using Settings = display::TouchSettings<false>;
using display::valid;
}
