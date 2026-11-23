#pragma once

#include <cstring>
#include <optional>
#include <span>

#include "usb_protocol.hpp"

namespace usb::protocol {

struct StateAccess {
	size_t size;
	void* owner;
	bool (*read)(void*, std::span<uint8_t> out);
	bool (*write)(void*, size_t offset, std::span<const uint8_t> bytes);
};

struct Stored {
	const char* key{};
	size_t size{};
	void (*defaults)(uint8_t* out){};
	bool (*valid)(const uint8_t* blob){};
	bool (*load)(void* owner, const char* key, uint8_t* out,
				 size_t size) = nullptr;
	bool (*save)(void* owner, const char* key, const uint8_t* blob,
				 size_t size) = nullptr;
};

template <typename Settings>
constexpr Stored stored(const char* key) {
	return {key, sizeof(Settings),
			[](uint8_t* out) {
				Settings settings{};
				std::memcpy(out, &settings, sizeof settings);
			},
			[](const uint8_t* blob) {
				Settings settings;
				std::memcpy(&settings, blob, sizeof settings);
				return valid(settings);
			}};
}

struct ListedTraffic {
	uint16_t available;
	size_t written;
};

struct Services {
	void* owner;
	StateAccess (*find)(void* owner, uint8_t component_id);
	size_t target_size;
	size_t gnss_size;
	// An update applies the entire supplied batch/report or returns false
	// without changing State. Timestamps use AIRGREP's local clock.
	bool (*update_traffic)(void* owner, const uint8_t* payload, size_t count,
						   int64_t now_us);
	bool (*update_gnss)(void* owner, const uint8_t* payload, int64_t now_us);

	size_t tracked_size_bytes = 0;
	std::optional<ListedTraffic> (*list_traffic)(void* owner, uint8_t* out,
												 size_t limit) = nullptr;
	std::span<const Stored> settings = {};
	// Reads the size bytes stored under a key. False when there are none of
	// that size.
	bool (*load_settings)(void* owner, const char* key, uint8_t* out,
						  size_t size) = nullptr;
	bool (*store_settings)(void* owner, const char* key, const uint8_t* blob,
						   size_t size) = nullptr;
	// Removes what is stored under every key, also under one this firmware
	// has no struct for.
	bool (*erase_settings)(void* owner) = nullptr;
};

// Dispatch only a complete, CRC- and version-validated request. Input and
// response buffers are separate. Returns the full response frame length,
// or zero when capacity cannot hold even a STATUS frame.
size_t execute_data(Command command, const uint8_t* payload, size_t size,
					int64_t now_us, const Services& services, uint8_t* output,
					size_t capacity);

}
