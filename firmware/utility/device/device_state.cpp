#include "device_state.hpp"

namespace device {
template <class Held>
static auto state_bytes(Held& states, Component component)
	-> decltype(bytes_of(states.adsbee)) {
	using Byte =
		std::conditional_t<std::is_const_v<Held>, const uint8_t, uint8_t>;
	switch (component) {
		case Component::adsbee:
			return bytes_of(states.adsbee);
		case Component::traffic:
			return {reinterpret_cast<Byte*>(states.traffic.data()),
					states.traffic.size() * sizeof(traffic::Target)};
		case Component::barometer:
			return bytes_of(states.barometer);
		case Component::battery:
			return bytes_of(states.battery);
		case Component::charger:
			return bytes_of(states.charger);
		case Component::expander:
			return bytes_of(states.expander);
		case Component::usb_c:
			return bytes_of(states.usb_c);
		case Component::wifi:
			return bytes_of(states.wifi);
		case Component::adsbee_clock:
			return bytes_of(states.adsbee_clock);
		default:
			return {};
	}
}

std::span<uint8_t> States::bytes(Component component) {
	return state_bytes(*this, component);
}
std::span<const uint8_t> States::bytes(Component component) const {
	return state_bytes(*this, component);
}
}
