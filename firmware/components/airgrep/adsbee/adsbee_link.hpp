#pragma once

#include <cstddef>
#include <cstdint>
#include "adsbee_decoder.h"
#include "adsbee_protocol.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "loop.hpp"


namespace adsbee {
#define ADSBEE_UART_BAUD 115200
#define ADSBEE_REQUEST_TIMEOUT_MS 1000
#define ADSBEE_REPLY_WINDOW_MS 900
#define ADSBEE_UART_RX_BUFFER 2048
#define ADSBEE_UART_TX_BUFFER 512
#define ADSBEE_UART_QUEUE 20
#define ADSBEE_POLL_MS 100
// Sent after every TX attach. The first copy also flushes noise the floating line left in the
// ADSBee's line assembler, the second one is the one that counts.
#define ADSBEE_INIT_COMMAND "AT+PROTOCOL_OUT=COMMS_UART,AIRGREP"
#define ADSBEE_INIT_REPEATS 2
#define ADSBEE_INIT_REPEAT_MS 100
#define ADSBEE_DEVICE_INFO_COMMAND "AT+DEVICE_INFO?"

struct State {
	bool known = false, tx_attached = false, initialized = false;
	int64_t updated_us = 0, reply_us = 0, error_us = 0;
		esp_err_t last_error = ESP_OK;
		uint32_t telemetry_lines = 0, crc_failures = 0, parity_errors = 0, framing_errors
					= 0, overruns = 0;
	Gnss gnss;
	int64_t gnss_us = 0; // last #GNSS frame, 0 until the first one

	template <typename F>
		static void update(F f) {
			portENTER_CRITICAL(&mutex_);
			f(storage());
			portEXIT_CRITICAL(&mutex_);
		}

		static State copy();

	private:
		static State& storage();
		static portMUX_TYPE mutex_;
	};

	struct Message {
		decoder_kind_t kind;
		uint16_t len;
		int64_t received_us;
		char text[]; // NUL-terminated
	};

	using Predicate = bool (*)(const Message&);
	using Handler = void (*)(const Message&, void* ctx);

	bool subscribe(Predicate predicate, Handler handler, void* ctx);
	// Subscribes to the expander power pin. Call before expander initialization.
	esp_err_t start();
	loop::Handle poll_handle();

	// Listeners run on the loop task. Failures leave the reason in State::last_error.
	loop::Handle request(const char* command, loop::SuccessListener on_reply,
						loop::ErrorListener on_error, uint32_t timeout_ms = ADSBEE_REQUEST_TIMEOUT_MS);
	// Copies the reply to the last explicit AT+DEVICE_INFO?. Returns 0 while unknown.
	size_t device_info(char* out, size_t capacity);
} // namespace adsbee
