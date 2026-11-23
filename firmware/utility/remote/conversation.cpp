#include "conversation.hpp"

#include <algorithm>

#include "traffic/traffic_table.hpp"

namespace remote {
namespace {
using namespace std::chrono_literals;
using device::Component;
using usb::protocol::Service;

constexpr auto kHelloEvery = 1000ms;
// Firmware older than the service field only shows by its answer whether it
// knows READ.
bool readable(device::Session& session, Service service) {
	if (service != Service::unknown) {
		return service == Service::present;
	}
	try {
		session.read(Component::battery);
		return true;
	} catch (const device::Refused& refused) {
		if (refused.status == usb::protocol::Status::unknown_command) {
			return false;
		}
		throw;
	}
}

// Empty for a device that stores no settings under the key: its firmware is
// built for another display, or is older than the settings commands.
std::vector<uint8_t> stored_settings(device::Session& session,
									 const std::string& key) {
	try {
		return session.read_settings(key);
	} catch (const device::Refused& refused) {
		if (refused.status == usb::protocol::Status::unknown_settings ||
			refused.status == usb::protocol::Status::unknown_command) {
			return {};
		}
		throw;
	}
}
}

Conversation::Conversation(device::Transport& transport)
	: session_(transport),
	  hello_(session_.hello()),
	  can_read_(readable(session_, hello_.service)) {}

Conversation::Step Conversation::step(Time now, Asked asked) {
	if (asked.settings_write) {
		session_.write_settings(asked.settings_write->key,
								asked.settings_write->bytes);
	}
	if (const std::optional<adsbee::Gnss>& gnss = asked.gnss) {
		session_.send(device::Command::update_gnss, device::bytes_of(*gnss));
	}
	if (asked.settings_key && !asked.stop) {
		return {.next = now,
				.settings = can_read_
								? stored_settings(session_, *asked.settings_key)
								: std::vector<uint8_t>{}};
	}
	if (asked.restart) {
		if (*asked.restart == Restart::factory_reset) {
			session_.erase_settings();
		}
		session_.send(*asked.restart == Restart::bootloader
						  ? device::Command::enter_bootloader
						  : device::Command::reboot);
		return {};
	}
	if (asked.stop) {
		return {};
	}
	const bool watching = can_read_ && asked.interval > 0ms;
	if (!watching) {
		last_picture_start_.reset();
	} else if (!last_picture_start_ ||
			   now >= *last_picture_start_ + asked.interval) {
		device::DeviceState picture = session_.read_device_state(
			device::kDisplayComponents, traffic::kListed);
		hello_ = picture.hello;
		next_hello_ = now + kHelloEvery;
		last_picture_start_ = now;
		return {.next = std::min(now + asked.interval, next_hello_),
				.picture = std::move(picture)};
	}
	if (now >= next_hello_) {
		hello_ = session_.hello();
		next_hello_ = now + kHelloEvery;
	}
	return {.next = watching ? std::min(*last_picture_start_ + asked.interval,
										next_hello_)
							 : next_hello_};
}
}
