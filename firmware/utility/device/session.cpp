#include "session.hpp"

#include <format>

#include "inspect.hpp"

namespace device {
namespace {
using namespace std::chrono_literals;
using usb::protocol::crc16;
using usb::protocol::kStatusSize;
using usb::protocol::kVersion;
using usb::protocol::Status;

constexpr auto kTimeout = 3000ms;
// A reset cannot recall what the device already sent. Silence for this long
// means nothing is on its way any more.
constexpr auto kQuiet = 100ms;
constexpr size_t kDrain = 4096;
constexpr size_t kHeader = 6;

uint16_t read16(std::span<const uint8_t> bytes, size_t at) {
	return static_cast<uint16_t>(bytes[at] |
								 static_cast<unsigned>(bytes[at + 1]) << 8U);
}

uint16_t checksum(std::span<const uint8_t> frame) {
	return crc16(frame.data() + kHeader, frame.size() - kHeader,
				 crc16(frame.data(), 4));
}

const char* name(Status status) {
	static constexpr const char* names[] = {"OK",
											"CRC_MISMATCH",
											"VERSION_MISMATCH",
											"UNKNOWN_COMMAND",
											"UNKNOWN_COMPONENT",
											"INVALID_LENGTH",
											"INVALID_RANGE",
											"INTERNAL_ERROR",
											"UNKNOWN"};
	const size_t index = static_cast<size_t>(status);
	return index < std::size(names) ? names[index] : "an unknown status";
}
}

std::vector<uint8_t> request(Command command,
							 std::span<const uint8_t> payload) {
	const size_t size = kHeader + payload.size();
	if (size > UINT16_MAX) {
		throw Error("Request does not fit a frame");
	}
	std::vector<uint8_t> frame{kVersion,
							   static_cast<uint8_t>(size),
							   static_cast<uint8_t>(size >> 8U),
							   static_cast<uint8_t>(command),
							   0,
							   0};
	frame.insert(frame.end(), payload.begin(), payload.end());
	const uint16_t crc = checksum(frame);
	frame[4] = static_cast<uint8_t>(crc);
	frame[5] = static_cast<uint8_t>(crc >> 8U);
	return frame;
}

std::vector<uint8_t> payload_of(std::span<const uint8_t> frame,
								Command command) {
	if (frame.size() < kStatusSize || read16(frame, 1) != frame.size()) {
		throw Error("Invalid response length");
	}
	if (checksum(frame) != read16(frame, 4)) {
		throw Error("Response CRC mismatch");
	}
	auto answered = static_cast<Command>(frame[3]);
	auto status = static_cast<Status>(frame[6]);
	const bool acknowledged = answered == Command::status;
	if (acknowledged && frame.size() != kStatusSize) {
		throw Error("STATUS response contains unexpected payload");
	}
	if (acknowledged && status != Status::ok) {
		throw Refused(status, std::format("Device replied {}", name(status)));
	}
	if (frame[0] != kVersion) {
		throw Error(std::format("Protocol version mismatch: device {}, here {}",
								frame[0], kVersion));
	}
	if (answered != usb::protocol::success_response(command) ||
		status != Status::ok) {
		throw Error("Unexpected response command or status");
	}
	return {frame.begin() + kStatusSize, frame.end()};
}

const char* name(Component component) {
	static constexpr const char* names[] = {
		"",        "adsbee",   "traffic", "barometer", "battery",
		"charger", "expander", "usb_c",   "wifi",      "adsbee_clock"};
	const size_t index = static_cast<size_t>(component);
	return index < std::size(names) ? names[index] : "";
}

Session::Session(Transport& transport) : transport_(transport) {
	transport_.reset_session();
	const auto deadline = std::chrono::steady_clock::now() + kTimeout;
	while (transport_.read(received_, kDrain, kQuiet)) {
		received_.clear();
		if (std::chrono::steady_clock::now() >= deadline) {
			throw Error("Device did not become quiet after the session reset");
		}
	}
}

std::vector<uint8_t> Session::send(Command command,
								   std::span<const uint8_t> payload) {
	received_.clear();
	transport_.write(request(command, payload));
	const auto deadline = std::chrono::steady_clock::now() + kTimeout;
	size_t expected = kStatusSize;
	while (received_.size() < expected) {
		const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
			deadline - std::chrono::steady_clock::now());
		if (left <= 0ms ||
			!transport_.read(received_, expected - received_.size(), left)) {
			throw Error("Response timed out, command outcome unknown");
		}
		if (received_.size() >= 3) {
			expected = read16(received_, 1);
			if (expected < kStatusSize) {
				throw Error("Response frame is smaller than its header");
			}
		}
	}
	if (received_.size() > expected) {
		throw Error("Unexpected bytes after the response frame");
	}
	return payload_of(received_, command);
}

Hello Session::hello() {
	std::vector<uint8_t> payload = send(Command::hello);
	if (payload.size() != sizeof(Hello)) {
		throw Error(
			std::format("HELLO has {} bytes, expected {}. Upgrade the "
						"firmware",
						payload.size(), sizeof(Hello)));
	}
	Hello hello;
	std::memcpy(&hello, payload.data(), sizeof hello);
	require_valid(inspect(hello));
	return hello;
}

std::vector<uint8_t> Session::read(Component component) {
	uint8_t id = static_cast<uint8_t>(component);
	return send(Command::read, {&id, 1});
}

void Session::read(Component component, std::span<uint8_t> state) {
	std::vector<uint8_t> bytes = read(component);
	if (bytes.size() != state.size()) {
		throw Error(std::format(
			"{} has {} bytes on the device and {} here. Firmware and utility "
			"are built from different sources",
			name(component), bytes.size(), state.size()));
	}
	std::memcpy(state.data(), bytes.data(), bytes.size());
}

TrafficListing Session::list_traffic(uint16_t limit) {
	auto listing = read_traffic_listing(limit);
	require_valid(inspect(listing));
	return listing;
}

TrafficListing Session::read_traffic_listing(uint16_t limit) {
	TrafficListing listing;
	auto& listed = listing.nearest_targets;
	const uint8_t request[] = {static_cast<uint8_t>(limit),
							   static_cast<uint8_t>(limit >> 8U)};
	std::vector<uint8_t> bytes = send(Command::list_traffic, request);
	using namespace usb::protocol;
	size_t count = bytes.size() >= kListCountsSize
					   ? read16(bytes, kListReturnedOffset)
					   : 0;
	if (bytes.size() != kListCountsSize + count * sizeof(traffic::Tracked) ||
		count > limit) {
		throw Error(std::format(
			"The traffic listing has {} bytes for {} Targets of {} bytes. "
			"Firmware and utility are built from different sources",
			bytes.size(), count, sizeof(traffic::Tracked)));
	}
	listed.resize(count);
	if (count) {
		std::memcpy(listed.data(), bytes.data() + kListCountsSize,
					bytes.size() - kListCountsSize);
	}
	listing.total_targets = read16(bytes, kListPresentOffset);
	if (listing.total_targets < count) {
		throw Error("Traffic listing exceeds total targets");
	}
	return listing;
}

DeviceState Session::read_device_state(std::span<const Component> components,
									   uint16_t target_limit) {
	DeviceState picture;
	picture.components.assign(components.begin(), components.end());
	for (const Component component : components) {
		read(component, picture.states.bytes(component));
	}
	picture.traffic_listing = read_traffic_listing(target_limit);
	picture.hello = hello();
	require_valid(inspect(picture, end_of_uptime_second_us(picture.hello)));
	return picture;
}

namespace {
std::vector<uint8_t> keyed(std::string_view key) {
	if (key.size() >= usb::protocol::kKeySize) {
		throw Error(std::format("{} is too long for a settings key", key));
	}
	std::vector<uint8_t> payload(usb::protocol::kKeySize);
	std::memcpy(payload.data(), key.data(), key.size());
	return payload;
}
}

std::vector<uint8_t> Session::read_settings(std::string_view key) {
	return send(Command::read_settings, keyed(key));
}

void Session::write_settings(std::string_view key,
							 std::span<const uint8_t> settings) {
	std::vector<uint8_t> payload = keyed(key);
	payload.insert(payload.end(), settings.begin(), settings.end());
	send(Command::write_settings, payload);
}

void Session::erase_settings() { send(Command::write_settings); }

void Session::reboot() { send(Command::reboot); }

void Session::enter_bootloader() { send(Command::enter_bootloader); }
}
