#pragma once

#include <cstddef>
#include <cstdint>

#include "adsbee_decoder.h"
#include "adsbee_protocol.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
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

enum class Ingestion : uint8_t { read = 0, ignore = 1 };

struct State {
	bool known = false, tx_attached = false, initialized = false;
	int64_t updated_us = 0, reply_us = 0, error_us = 0;
	esp_err_t last_error = ESP_OK;
	uint32_t telemetry_lines = 0, crc_failures = 0, parity_errors = 0,
			 framing_errors = 0, overruns = 0;
	Gnss gnss;
	int64_t gnss_us = 0;  // last #GNSS frame, 0 until the first one
	Ingestion traffic_ingestion = Ingestion::read;
	Ingestion gnss_ingestion = Ingestion::read;

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
	char text[];  // NUL-terminated
};

using Predicate = bool (*)(const Message&);
using Handler = void (*)(const Message&, void* ctx);

bool subscribe(Predicate predicate, Handler handler, void* ctx);
// Every decoded #GNSS frame, after State carries it. Runs on the loop task.
// A flip of the frame's fix field is also published as fix_acquired and
// fix_lost.
using GnssHandler = void (*)(const Gnss&, int64_t received_us, void* ctx);
bool subscribe_gnss(GnssHandler handler, void* ctx);
// Publishes a decoded live or injected report. Call on the event loop.
void ingest_gnss(const Gnss& gnss, int64_t received_us);
// A closed or reset data connection restores live input.
void reset_ingestion();
// Subscribes the TX gate to mode_changed: the ADSBee is powered in ON only.
esp_err_t prepare();
esp_err_t uart_init();
esp_err_t start();

loop::Handle request(const char* command, loop::SuccessListener on_reply,
					 loop::ErrorListener on_error,
					 uint32_t timeout_ms = ADSBEE_REQUEST_TIMEOUT_MS);
bool busy();
// On the loop, when the health check finds the link down or the mode leaves
// ON: a held fix is published as fix_lost, since no frame will report it.
void drop_fix();
// TX attached and the init sequence done, so the link takes requests.
bool ready();
size_t device_info(char* out, size_t capacity);
}  // namespace adsbee
