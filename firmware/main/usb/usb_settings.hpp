#pragma once

#include "settings/settings_model.hpp"
#include "usb_commands.hpp"

namespace usb {
inline constexpr protocol::Stored kSettings{
	SETTINGS_NVS_KEY, sizeof(settings::Settings),
	[](uint8_t* out) {
		settings::Settings settings = settings::defaults();
		std::memcpy(out, &settings, sizeof settings);
	},
	[](const uint8_t* blob) {
		settings::Settings settings{};
		std::memcpy(&settings, blob, sizeof settings);
		return settings::valid(settings);
	}};
}
