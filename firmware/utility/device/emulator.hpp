#pragma once

#include <deque>
#include <map>
#include <memory>
#include <mutex>

#include "device_state.hpp"
#include "traffic/traffic_table.hpp"

namespace device {
struct EmulatorFlash {
	std::mutex mutex;
	std::map<std::string, std::vector<uint8_t>> values;
};
struct EmulatedDevice {
	Hello hello{};
	States states;
	std::vector<traffic::Position> history = std::vector<traffic::Position>(
		usb::layout::kTargets * traffic::kHistorySeconds);
	size_t history_head{};
	std::shared_ptr<EmulatorFlash> flash = std::make_shared<EmulatorFlash>();
	traffic::Table traffic_table() {
		return {states.traffic.data(), states.traffic.size(), history.data(),
				history_head};
	}
};
EmulatedDevice make_booted_service_device();
void fly(EmulatedDevice& device, double elapsed_flight_s);
// The flight as a device to talk to. It begins when this is called. Every
// flight of this program stores its settings in the same place, as a device
// keeps its own over a restart.
std::unique_ptr<Transport> open_flight();

class Emulator : public Transport {
   public:
	explicit Emulator(EmulatedDevice& device)
		: Emulator(device,
				   device.hello.service == usb::protocol::Service::present) {}
	Emulator(EmulatedDevice& device, bool service_commands)
		: device_(device), decoder_(service_commands) {}
	std::optional<Command> restart_command;

	void reset_session() override;
	void write(std::span<const uint8_t> frame) override;
	bool read(std::vector<uint8_t>& bytes, size_t wanted,
			  std::chrono::milliseconds timeout) override;

   private:
	void reply();
	EmulatedDevice& device_;
	usb::protocol::Decoder decoder_;
	std::vector<uint8_t> input_;
	std::deque<uint8_t> output_;
};
}
