#pragma once

#include <bitset>

namespace remote::view {
struct SettingsEdits {
	enum Field { theme, tracks, symbols, indicator, radar_range, count };
	std::bitset<count> changed;

	template <class Settings>
	Settings merge(Settings stored, const Settings& local) const {
		if (changed[theme]) {
			stored.theme = local.theme;
		}
		if (changed[tracks]) {
			stored.tracks = local.tracks;
		}
		if (changed[symbols]) {
			stored.symbol_style = local.symbol_style;
		}
		if (changed[indicator]) {
			stored.conflict_indicator = local.conflict_indicator;
		}
		if (changed[radar_range]) {
			stored.radar_range_index = local.radar_range_index;
		}
		return stored;
	}
};
}
