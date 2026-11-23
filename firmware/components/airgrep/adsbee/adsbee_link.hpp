#pragma once

#include <cstddef>
#include <cstdint>

#include "adsbee_decoder.h"
#include "adsbee_protocol.hpp"
#include "esp_err.h"
#include "locked_state.hpp"
#include "loop.hpp"

namespace adsbee {
#define ADSBEE_UART_BAUD 921600
#define ADSBEE_REQUEST_TIMEOUT_MS 1000
#define ADSBEE_REPLY_WINDOW_MS 900
#define ADSBEE_UART_RX_BUFFER 2048
#define ADSBEE_UART_TX_BUFFER 512
#define ADSBEE_UART_QUEUE 20
#define ADSBEE_POLL_MS 100
// Sent after every TX attach. The first copy also flushes noise the floating
// line left in the ADSBee's line assembler, the second one is the one that
// counts.
#define ADSBEE_INIT_COMMAND "AT+PROTOCOL_OUT=COMMS_UART,AIRGREP"
#define ADSBEE_INIT_REPEATS 2
#define ADSBEE_INIT_REPEAT_MS 100
#define ADSBEE_DEVICE_INFO_COMMAND "AT+DEVICE_INFO?"

struct State : StateData, LockedState<State> {};

struct Message {
	decoder_kind_t kind;
	uint16_t len;
	int64_t received_us;
	char text[];  // NUL-terminated
};

using Predicate = bool (*)(const Message&);
using Handler = void (*)(const Message&, void* ctx);

bool subscribe(Predicate predicate, Handler handler, void* context);
// Fresh GNSS reports on the loop. A fix after a gap counts as acquired again.
using GnssHandler = void (*)(const Gnss&, int64_t received_us, bool acquired,
							 void* ctx);
bool subscribe_gnss(GnssHandler handler, void* context);
// Publishes a decoded live or injected report. Call on the event loop.
void ingest_gnss(const Gnss& gnss, int64_t received_us);
// A closed or reset data connection restores live input.
void reset_ingestion();
// Subscribes the TX gate to mode_changed: the ADSBee is powered in ON only.
esp_err_t prepare();
esp_err_t uart_init();
esp_err_t start();

// on_written runs on the loop after the UART accepts the whole line.
loop::Handle request(const char* command, loop::SuccessListener success,
					 loop::ErrorListener failure,
					 uint32_t window_ms = ADSBEE_REQUEST_TIMEOUT_MS,
					 loop::Callable<void()> on_written = {});
bool busy();
bool accepts_requests();
size_t device_info(char* out, size_t capacity);
}
