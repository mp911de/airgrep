#pragma once

#include <cstring>
#include <string>
#include <string_view>

#include "transport.hpp"
#include "usb/usb_components.hpp"
#include "usb/usb_layout.hpp"
#include "usb/usb_protocol.hpp"

namespace device {
using usb::Component;
using usb::protocol::Command;
using usb::protocol::Hello;

struct DeviceState;
struct TrafficListing {
	uint16_t total_targets{};
	std::vector<traffic::Tracked> nearest_targets;
};

struct Refused : Error {
	Refused(usb::protocol::Status with, const std::string& what)
		: Error(what), status(with) {}
	usb::protocol::Status status;
};

std::vector<uint8_t> request(Command command,
							 std::span<const uint8_t> payload = {});
// The payload of a complete response to this command. Error when the frame is
// damaged, of another protocol version, or the device refused.
std::vector<uint8_t> payload_of(std::span<const uint8_t> frame,
								Command command);
const char* name(Component component);

// One data session: requests go out one at a time and each waits for its
// answer. Opening it resets the session on the device.
class Session {
   public:
	explicit Session(Transport& transport);

	std::vector<uint8_t> send(Command command,
							  std::span<const uint8_t> payload = {});
	Hello hello();
	std::vector<uint8_t> read(Component component);
	void read(Component component, std::span<uint8_t> state);
	TrafficListing list_traffic(uint16_t limit);
	// HELLO, the State of these Components and the nearest Targets.
	DeviceState read_device_state(std::span<const Component> components,
								  uint16_t target_limit);
	// The settings the device stores under a key, or their defaults when it
	// stores none. Refused with UNKNOWN by a device that has no struct
	// for the key.
	std::vector<uint8_t> read_settings(std::string_view key);
	// Refused with UNKNOWN when the device holds a struct of another
	// size or version under the key.
	void write_settings(std::string_view key,
						std::span<const uint8_t> settings);
	void erase_settings();
	void reboot();
	void enter_bootloader();

   private:
	TrafficListing read_traffic_listing(uint16_t limit);
	Transport& transport_;
	std::vector<uint8_t> received_;
};
}
