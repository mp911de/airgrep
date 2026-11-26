#pragma once

#include <algorithm>
#include <string_view>
#include <type_traits>

#include "session.hpp"

namespace device {
inline constexpr Component kComponents[] = {
	Component::adsbee,  Component::traffic, Component::barometer,
	Component::battery, Component::charger, Component::expander,
	Component::usb_c,   Component::wifi,    Component::adsbee_clock};
// The Components a display shows.
inline constexpr Component kDisplayComponents[] = {
	Component::adsbee,  Component::barometer, Component::battery,
	Component::charger, Component::expander,  Component::wifi};

// The State of every Component of one device.
struct States {
	adsbee::StateData adsbee;
	std::vector<traffic::Target> traffic =
		std::vector<traffic::Target>(usb::layout::kTargets);
	barometer::StateData barometer;
	battery::StateData battery;
	charger::StateData charger;
	expander::StateData expander;
	fusb::StateData usb_c;
	wifi::StateData wifi;
	adsbee::timesync::StateData adsbee_clock;
	std::span<uint8_t> bytes(Component component);
	std::span<const uint8_t> bytes(Component component) const;
};

// What one read of a device returned. The State of a Component that was not
// read holds its initial value.
struct DeviceState {
	Hello hello{};
	std::vector<Component> components;
	States states;
	TrafficListing traffic_listing;
};

template <typename State>
auto bytes_of(State& state) {
	using Byte =
		std::conditional_t<std::is_const_v<State>, const uint8_t, uint8_t>;
	return std::span<Byte>{reinterpret_cast<Byte*>(&state), sizeof state};
}

inline void put(char* field, size_t size, std::string_view text) {
	if (size == 0) {
		return;
	}
	const size_t count = std::min(text.size(), size - 1);
	text.copy(field, count);
	field[count] = 0;
}

template <size_t Size>
void put(char (&field)[Size], std::string_view text) {
	put(field, Size, text);
}
}
