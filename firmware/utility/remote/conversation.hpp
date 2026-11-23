#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "device/device_state.hpp"

namespace remote {
// The requests to one device in their order: HELLO once a second to notice
// that the device is gone, and a picture as often as a display wants one. It
// has no thread and reads no clock: its caller says what time it is.
class Conversation {
   public:
	using Time = std::chrono::steady_clock::time_point;
	enum class Restart { reboot, bootloader, factory_reset };
	struct SettingsWrite {
		std::string key;
		std::vector<uint8_t> bytes;
	};
	// What the user of the device wants at the time of a step.
	struct Asked {
		// A picture every interval, or none for a zero interval.
		std::chrono::milliseconds interval{0};
		std::optional<Restart> restart;
		std::optional<std::string> settings_key;
		std::optional<SettingsWrite> settings_write;
		// GNSS data the device takes in place of what its receiver says.
		std::optional<adsbee::Gnss> gnss;
		// Ends the conversation once the settings are written and the restart
		// is sent.
		bool stop = false;
	};
	struct Step {
		// When the next step is due without anything new asked. Nothing once
		// the conversation ended.
		std::optional<Time> next;
		std::optional<device::DeviceState> picture;
		// What the device stores under the key asked for. Empty when it
		// stores nothing under it.
		std::optional<std::vector<uint8_t>> settings;
	};

	// Opens the session: HELLO, and whether the device knows READ.
	explicit Conversation(device::Transport& transport);
	const device::Hello& hello() const { return hello_; }
	bool supports_component_reads() const { return can_read_; }
	// Sends what is due at this time. A failure of the device is an Error.
	Step step(Time now, Asked asked);

   private:
	device::Session session_;
	device::Hello hello_;
	bool can_read_;
	Time next_hello_{};
	std::optional<Time> last_picture_start_;
};
}
