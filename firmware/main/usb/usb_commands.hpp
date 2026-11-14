#pragma once

#include "usb_protocol.hpp"

namespace usb::protocol {

using StateVisitor = void (*)(void* state, void* context);

struct StateAccess {
	size_t size;
	// Runs the visitor synchronously under the owner's lock, or returns false
	// without invoking it when the State is unavailable.
	bool (*visit)(StateVisitor, void* context);
};

struct Services {
	StateAccess (*find)(uint8_t component_id);
	size_t target_size;
	size_t gnss_size;
	// An update applies the entire supplied batch/report or returns false
	// without changing State. Timestamps use AIRGREP's local clock.
	bool (*update_traffic)(const uint8_t* payload, size_t count,
						   int64_t now_us);
	bool (*update_gnss)(const uint8_t* payload, int64_t now_us);
};

// Dispatch only a complete, CRC- and version-validated request. Input and
// response buffers are separate. Returns the full response frame length,
// or zero when capacity cannot hold even a STATUS frame.
size_t execute_data(Command command, const uint8_t* payload, size_t size,
					int64_t now_us, const Services& services, uint8_t* response,
					size_t capacity);

}  // namespace usb::protocol
