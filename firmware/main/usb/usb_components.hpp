#pragma once

#include "usb_commands.hpp"

namespace usb {
enum class Component : uint8_t {
	adsbee = 0x01,
	traffic = 0x02,
	barometer = 0x03,
	battery = 0x04,
	charger = 0x05,
	expander = 0x06,
	usb_c = 0x07,
	wifi = 0x08,
	adsbee_clock = 0x09,
};

// Execute on the event loop. The listing scratch buffer is shared between
// requests.
const protocol::Services& component_services();
}
