#include "usb_commands.hpp"

#include <cstring>

namespace usb::protocol {
namespace {
struct Copy {
	uint8_t* output;
	const uint8_t* input;
	size_t offset, size;
};

void read_state(void* state, void* context) {
	auto& copy = *static_cast<Copy*>(context);
	std::memcpy(copy.output, state, copy.size);
}

void write_state(void* state, void* context) {
	auto& copy = *static_cast<Copy*>(context);
	std::memcpy(static_cast<uint8_t*>(state) + copy.offset,
				copy.input + copy.offset, copy.size);
}

uint16_t read16(const uint8_t* bytes) {
	return bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8);
}

size_t access_state(Command command, const uint8_t* payload, size_t size,
					const Services& services, uint8_t* response,
					size_t capacity) {
	if ((command == Command::read && size != 1) ||
		(command == Command::write && size < 5)) {
		return encode_status(Status::invalid_length, response);
	}
	StateAccess access =
		services.find ? services.find(payload[0]) : StateAccess{};
	if (!access.visit || !access.size) {
		return encode_status(Status::unknown_component, response);
	}
	if (command == Command::read) {
		if (access.size > UINT16_MAX - kStatusSize ||
			access.size > capacity - kStatusSize) {
			return encode_status(Status::internal_error, response);
		}
		Copy copy{response + kStatusSize, nullptr, 0, access.size};
		if (!access.visit(read_state, &copy)) {
			return encode_status(Status::internal_error, response);
		}
		return encode_response(Command::read, Status::ok, response,
							   access.size);
	}
	if (size - 5 != access.size) {
		return encode_status(Status::invalid_length, response);
	}
	size_t offset = read16(payload + 1), length = read16(payload + 3);
	if (offset > access.size || length > access.size - offset) {
		return encode_status(Status::invalid_range, response);
	}
	if (length) {
		Copy copy{nullptr, payload + 5, offset, length};
		if (!access.visit(write_state, &copy)) {
			return encode_status(Status::internal_error, response);
		}
	}
	return encode_status(Status::ok, response);
}
}  // namespace

size_t execute_data(Command command, const uint8_t* payload, size_t size,
					int64_t now_us, const Services& services, uint8_t* response,
					size_t capacity) {
	if (capacity < kStatusSize) {
		return 0;
	}
	if (size > UINT16_MAX - 6) {
		return encode_status(Status::invalid_length, response);
	}
	switch (command) {
		case Command::read:
		case Command::write:
			return access_state(command, payload, size, services, response,
								capacity);
		case Command::update_traffic: {
			if (!services.target_size || !size || size % services.target_size ||
				size / services.target_size > 32) {
				return encode_status(Status::invalid_length, response);
			}
			bool applied = services.update_traffic &&
						   services.update_traffic(
							   payload, size / services.target_size, now_us);
			return encode_status(applied ? Status::ok : Status::internal_error,
								 response);
		}
		case Command::update_gnss: {
			if (!services.gnss_size || size != services.gnss_size) {
				return encode_status(Status::invalid_length, response);
			}
			bool applied =
				services.update_gnss && services.update_gnss(payload, now_us);
			return encode_status(applied ? Status::ok : Status::internal_error,
								 response);
		}
		default:
			return encode_status(Status::unknown_command, response);
	}
}
}  // namespace usb::protocol
