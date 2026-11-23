#pragma once

#include <memory>
#include <string>

#include "transport.hpp"

namespace device::usb_bus {

struct Found {
	// The factory MAC, twelve lower case hex digits.
	std::string serial;
	bool has_data_interface = false;
};

std::vector<Found> list();
// Claims the data interface of the device with this serial number. Error when
// it is gone, has no data interface or another program holds it.
std::unique_ptr<Transport> open(const std::string& serial);
}
