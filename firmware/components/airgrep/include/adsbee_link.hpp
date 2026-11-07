#pragma once

#include <cstddef>
#include <cstdint>
#include "decoder.h"
#include "esp_err.h"
#include "loop.hpp"

// UART link to the ADSBee COMMS port: 115200 baud, 8E1, no flow control.
// A hardware task reads the driver and feeds the decoder. Each decoded
// message is heap-allocated once, dispatched on the loop to the subscribers
// whose predicate accepts it, then freed. Subscribers are not loop listeners.

namespace adsbee {
	struct Message {
		decoder_kind_t kind;
		uint16_t len;
		char text[]; // NUL-terminated
	};

	using Predicate = bool (*)(const Message&);
	using Handler = void (*)(const Message&, void* ctx);

	// Registers a subscriber before start(). False when the table is full.
	bool subscribe(Predicate predicate, Handler handler, void* ctx);

	esp_err_t start();

	// Sends the command with LF appended and completes the promise with
	// Value::p pointing at the reply Message, valid during the success listener.
	// One request is outstanding at a time, a second one fails with error.
	// The action captures the command pointer, so the string must stay alive
	// until the action ran, a literal does.
	loop::Handle request(const char* command, loop::SuccessListener on_reply,
						loop::ErrorListener on_error, uint32_t timeout_ms = 1000);
} // namespace adsbee
