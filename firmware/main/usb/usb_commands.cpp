#include "usb_commands.hpp"

#include <cstring>

namespace usb::protocol {
namespace {
uint16_t read16(const uint8_t* bytes) {
	return static_cast<uint16_t>(bytes[0] | (unsigned{bytes[1]} << 8U));
}

void write16(uint8_t* bytes, size_t value) {
	bytes[0] = static_cast<uint8_t>(value);
	bytes[1] = static_cast<uint8_t>(value >> 8U);
}

size_t list_traffic(const uint8_t* payload, size_t size,
					const Services& services, std::span<uint8_t> response) {
	if (size != 2) {
		return encode_status(Status::invalid_length, response);
	}
	if (!services.list_traffic || !services.tracked_size_bytes ||
		response.size() < kStatusSize + kListCountsSize) {
		return encode_status(Status::internal_error, response);
	}
	const size_t room = (response.size() - kStatusSize - kListCountsSize) /
						services.tracked_size_bytes;
	const size_t limit = read16(payload) < room ? read16(payload) : room;
	uint8_t* body = response.data() + kStatusSize;
	auto listed =
		services.list_traffic(services.owner, body + kListCountsSize, limit);
	if (!listed || listed->written > limit ||
		listed->written > listed->available) {
		return encode_status(Status::internal_error, response);
	}
	write16(body + kListPresentOffset, listed->available);
	write16(body + kListReturnedOffset, listed->written);
	return encode_response(
		Command::list_traffic, Status::ok, response,
		kListCountsSize + listed->written * services.tracked_size_bytes);
}

size_t access_settings(Command command, const uint8_t* payload, size_t size,
					   const Services& services, std::span<uint8_t> response) {
	if (command == Command::write_settings && !size) {
		const bool erased =
			services.erase_settings && services.erase_settings(services.owner);
		return encode_status(erased ? Status::ok : Status::internal_error,
							 response);
	}
	if (size < kKeySize ||
		(command == Command::read_settings ? size != kKeySize
										   : size == kKeySize)) {
		return encode_status(Status::invalid_length, response);
	}
	const char* key = reinterpret_cast<const char*>(payload);
	const Stored* stored = nullptr;
	if (!payload[kKeySize - 1]) {
		for (const Stored& known : services.settings) {
			if (!std::strcmp(key, known.key)) {
				stored = &known;
			}
		}
	}
	if (!stored) {
		return encode_status(Status::unknown_settings, response);
	}
	if (command == Command::read_settings) {
		if (stored->size > response.size() - kStatusSize) {
			return encode_status(Status::internal_error, response);
		}
		uint8_t* out = response.data() + kStatusSize;
		auto load = stored->load ? stored->load : services.load_settings;
		if (!load || !load(services.owner, key, out, stored->size) ||
			!stored->valid(out)) {
			stored->defaults(out);
		}
		return encode_response(success_response(command), Status::ok, response,
							   stored->size);
	}
	const uint8_t* blob = payload + kKeySize;
	if (size - kKeySize != stored->size || !stored->valid(blob)) {
		return encode_status(Status::unknown_settings, response);
	}
	auto save = stored->save ? stored->save : services.store_settings;
	const bool kept = save && save(services.owner, key, blob, stored->size);
	return encode_status(kept ? Status::ok : Status::internal_error, response);
}

size_t access_state(Command command, const uint8_t* payload, size_t size,
					const Services& services, std::span<uint8_t> response) {
	if ((command == Command::read && size != 1) ||
		(command == Command::write && size < 5)) {
		return encode_status(Status::invalid_length, response);
	}
	const StateAccess access = services.find
								   ? services.find(services.owner, payload[0])
								   : StateAccess{};
	if (!access.read || !access.write || !access.size) {
		return encode_status(Status::unknown_component, response);
	}
	if (command == Command::read) {
		if (access.size > UINT16_MAX - kStatusSize ||
			access.size > response.size() - kStatusSize) {
			return encode_status(Status::internal_error, response);
		}
		if (!access.read(access.owner,
						 {response.data() + kStatusSize, access.size})) {
			return encode_status(Status::internal_error, response);
		}
		return encode_response(success_response(command), Status::ok, response,
							   access.size);
	}
	if (size - 5 != access.size) {
		return encode_status(Status::invalid_length, response);
	}
	const size_t offset = read16(payload + 1);
	const size_t length = read16(payload + 3);
	if (offset > access.size || length > access.size - offset) {
		return encode_status(Status::invalid_range, response);
	}
	if (length &&
		!access.write(access.owner, offset, {payload + 5 + offset, length})) {
		return encode_status(Status::internal_error, response);
	}
	return encode_status(Status::ok, response);
}
}

size_t execute_data(Command command, const uint8_t* payload, size_t size,
					int64_t now_us, const Services& services, uint8_t* output,
					size_t capacity) {
	const std::span<uint8_t> response{output, capacity};
	if (capacity < kStatusSize) {
		return 0;
	}
	if (size > UINT16_MAX - 6) {
		return encode_status(Status::invalid_length, response);
	}
	switch (command) {
		case Command::read:
		case Command::write:
			return access_state(command, payload, size, services, response);
		case Command::list_traffic:
			return list_traffic(payload, size, services, response);
		case Command::read_settings:
		case Command::write_settings:
			return access_settings(command, payload, size, services, response);
		case Command::update_traffic: {
			if (!services.target_size || !size || size % services.target_size ||
				size / services.target_size > 32) {
				return encode_status(Status::invalid_length, response);
			}
			const bool applied =
				services.update_traffic &&
				services.update_traffic(services.owner, payload,
										size / services.target_size, now_us);
			return encode_status(applied ? Status::ok : Status::internal_error,
								 response);
		}
		case Command::update_gnss: {
			if (!services.gnss_size || size != services.gnss_size) {
				return encode_status(Status::invalid_length, response);
			}
			const bool applied =
				services.update_gnss &&
				services.update_gnss(services.owner, payload, now_us);
			return encode_status(applied ? Status::ok : Status::internal_error,
								 response);
		}
		default:
			return encode_status(Status::unknown_command, response);
	}
}
}
