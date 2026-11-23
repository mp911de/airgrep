#include "adsbee_link.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>

#include "adsbee_protocol.hpp"
#include "board.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "events/events.hpp"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "subscribers.hpp"

namespace adsbee {
namespace {
constexpr const char* tag = "adsbee";
constexpr uart_port_t uart = UART_NUM_1;
constexpr uart_config_t uart_config = {
	.baud_rate = ADSBEE_UART_BAUD,
	.data_bits = UART_DATA_8_BITS,
	.parity = UART_PARITY_EVEN,
	.stop_bits = UART_STOP_BITS_1,
	.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	.rx_flow_ctrl_thresh = 0,
	.rx_glitch_filt_thresh = 0,
	.source_clk = UART_SCLK_XTAL,
	.flags = {},
};
constexpr gpio_num_t tx = static_cast<gpio_num_t>(BOARD_PIN_UART_TX);

struct Filtered {
	Predicate predicate;
	Handler handler;

	explicit operator bool() const { return predicate && handler; }
	void operator()(const Message& message, void* context) const {
		if (predicate(message)) {
			handler(message, context);
		}
	}
};

Subscribers<Filtered, 4> subscribers;
Subscribers<GnssHandler, 4> gnss_subscribers;
bool fix_held;  // loop task only
decoder_t decoder;
QueueHandle_t events;
bool started, tx_attached;
loop::Handle init_step;
unsigned init_sent;
Link liveness;
loop::Handle periodic;
void update_tx();

void publish_fix(bool valid, AntennaStatus antenna) {
	if (events::update_if_changed(fix_held, valid)) {
		events::publish(events::Event::fix_changed(valid, {antenna}));
	}
}

void expire_fix(int64_t now_us) {
	if (!fix_held) {
		return;
	}
	bool valid = false;
	AntennaStatus antenna = AntennaStatus::unreported;
	State::update([&](const State& state) {
		valid = state.gnss_at(now_us).fix != Fix::none;
		antenna = state.gnss.antenna;
	});
	publish_fix(valid, antenna);
}

constexpr size_t kCommandCapacity = 256;

struct MessageDelete {
	void operator()(Message* message) const { operator delete(message); }
};
using OwnedMessage = std::unique_ptr<Message, MessageDelete>;

struct Request {
	uint32_t window_ms = 0;
	loop::SuccessListener success;
	loop::ErrorListener failure;
	loop::Callable<void()> on_written;
	loop::Handle handle, finish;
	uint32_t uart_errors = 0;
	char command[];
};

// Request state belongs to the loop task. Only uart_errors is shared with the
// link task.
Request* pending;
Reply reply;
std::atomic<uint32_t> uart_errors;

char device_info_text[2048];
std::mutex device_info_mutex;

void record_error(esp_err_t error) {
	int64_t now = esp_timer_get_time();
	State::update([&](State& state) {
		state.last_error = error;
		state.error_us = now;
	});
}

esp_err_t detach() {
	const gpio_config_t config = {
		.pin_bit_mask = 1ULL << tx,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};
	const esp_err_t result = gpio_config(&config);
	tx_attached = false;
	loop::cancel(init_step);
	init_step = {};
	State::update(
		[](State& state) { state.tx_attached = state.initialized = false; });
	if (pending) {
		record_error(ESP_ERR_INVALID_STATE);
		loop::fail(pending->handle, loop::Outcome::error);
	}
	if (result != ESP_OK) {
		record_error(result);
	}
	return result;
}

void release(Request* request) {
	request->~Request();
	operator delete(request);
}

void after_request_cleanup(Request* request, bool replied) {
	if (pending == request) {
		pending = nullptr;
	}
	loop::cancel(request->finish);
	if (replied &&
		std::strcmp(request->command, ADSBEE_DEVICE_INFO_COMMAND) == 0) {
		const std::scoped_lock lock(device_info_mutex);
		std::memcpy(device_info_text, reply.text, reply.length + 1);
	}
	release(request);
}

void finish_request(loop::Handle handle) {
	const Request* request = pending;
	if (!request || request->handle.slot != handle.slot ||
		request->handle.generation != handle.generation) {
		return;
	}
	esp_err_t error = ESP_OK;
	if (request->uart_errors != uart_errors.load()) {
		error = ESP_ERR_INVALID_RESPONSE;
	} else if (!reply.length) {
		error = ESP_ERR_TIMEOUT;
	}
	if (error != ESP_OK) {
		record_error(error);
		loop::fail(handle, error == ESP_ERR_TIMEOUT ? loop::Outcome::timeout
													: loop::Outcome::error);
	} else {
		loop::complete(handle, loop::Value{.p = &reply});
	}
	events::publish(events::Event::adsbee_reply_window_closed());
}

void receive(const Message& message) {
	if (message.kind == DECODER_CSBEE) {
		liveness.received(message.received_us);
		update_tx();
		Gnss gnss;
		if (State::copy().gnss_ingestion == Ingestion::read &&
			decode_gnss(message.text, message.len, gnss)) {
			ingest_gnss(gnss, message.received_us);
		}
	}
	if (message.kind == DECODER_AT_REPLY) {
		ESP_LOGD(tag, "reply %s", message.text);
	}
	if (message.kind == DECODER_AT_REPLY && pending &&
		!reply.append(message.text, message.len)) {
		record_error(ESP_ERR_INVALID_SIZE);
		loop::fail(pending->handle, loop::Outcome::error);
		events::publish(events::Event::adsbee_reply_window_closed());
	}
	subscribers.notify(message);
}

void on_message(const char* text, size_t length, decoder_kind_t kind, void*) {
	bool valid = kind != DECODER_CSBEE || valid_csbee(text, length);
	int64_t now = esp_timer_get_time();
	State::update([&](State& state) {
		if (kind == DECODER_CSBEE) {
			++state.telemetry_lines;
			if (!valid) {
				++state.crc_failures;
			} else {
				state.updated_us = now;
			}
		}
		if (kind == DECODER_AT_REPLY) {
			state.reply_us = now;
		}
	});
	if (!valid) {
		return;
	}
	OwnedMessage owned(static_cast<Message*>(operator new(
		sizeof(Message) + length + 1, std::nothrow)));
	if (!owned) {
		record_error(ESP_ERR_NO_MEM);
		return;
	}
	owned->kind = kind;
	owned->len = length;
	owned->received_us = now;
	std::memcpy(owned->text, text, length + 1);
	// A queued task owns the message: its capture is a plain pointer.
	Message* const message = owned.release();
	auto submit = loop::try_submit(
		{"adsbee rx",
		 [message](loop::Handle, loop::Value&) {
			 const OwnedMessage received(message);
			 receive(*received);
			 return loop::Result::done;
		 },
		 1000},
		{}, [message](loop::Outcome) { const OwnedMessage dropped(message); });
	if (!submit) {
		owned.reset(message);
		record_error(ESP_ERR_NO_MEM);
	}
}

void link_task(void*) {
	char bytes[128];
	uart_event_t event;
	for (;;) {
		if (xQueueReceive(events, &event, portMAX_DELAY) != pdTRUE) {
			continue;
		}
		if (event.type == UART_DATA) {
			for (size_t left = event.size; left;) {
				const int count = uart_read_bytes(
					uart, bytes, left < sizeof bytes ? left : sizeof bytes, 0);
				if (count <= 0) {
					break;
				}
				adsbee_decoder_feed(&decoder, bytes, count);
				left -= count;
			}
		} else if (event.type == UART_PARITY_ERR ||
				   event.type == UART_FRAME_ERR ||
				   event.type == UART_FIFO_OVF ||
				   event.type == UART_BUFFER_FULL) {
			++uart_errors;
			State::update([&](State& state) {
				if (event.type == UART_PARITY_ERR) {
					++state.parity_errors;
				} else if (event.type == UART_FRAME_ERR) {
					++state.framing_errors;
				} else {
					++state.overruns;
				}
			});
			record_error(ESP_FAIL);
			uart_flush_input(uart);
			xQueueReset(events);
			adsbee_decoder_error(&decoder);
		}
	}
}

esp_err_t write_line(const char* text) {
	const int length = static_cast<int>(std::strlen(text));
	if (uart_write_bytes(uart, text, length) != length ||
		uart_write_bytes(uart, "\n", 1) != 1) {
		return ESP_FAIL;
	}
	return ESP_OK;
}

loop::Result init(loop::Handle, loop::Value&) {
	init_step = {};
	if (!tx_attached) {
		return loop::Result::done;
	}
	const esp_err_t result = write_line(ADSBEE_INIT_COMMAND);
	if (result != ESP_OK) {
		record_error(result);
		return loop::Result::done;
	}
	if (++init_sent < ADSBEE_INIT_REPEATS) {
		init_step =
			loop::schedule(ADSBEE_INIT_REPEAT_MS, {"adsbee init", init});
		if (!init_step.valid()) {
			record_error(ESP_ERR_NO_MEM);
		}
		return loop::Result::done;
	}
	State::update([](State& state) { state.initialized = true; });
	auto initialized = [](auto) {
		if (accepts_requests()) {
			events::publish(events::Event::adsbee_initialized());
		}
	};
	if (!request(ADSBEE_DEVICE_INFO_COMMAND, initialized, initialized,
				 ADSBEE_REPLY_WINDOW_MS)
			 .valid()) {
		events::publish(events::Event::adsbee_initialized());
	}
	return loop::Result::done;
}

void update_tx() {
	const int64_t now = esp_timer_get_time();
	bool alive = liveness.permits_tx(now);
	State::update([&](State& state) { state.known = alive; });
	if (!started || !alive) {
		if (tx_attached) {
			detach();
		}
		return;
	}
	if (tx_attached) {
		return;
	}
	// Never reattach while a cancelled command is still shifting out.
	esp_err_t result = uart_wait_tx_done(uart, 0);
	if (result == ESP_OK) {
		result = uart_set_pin(uart, tx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
							  UART_PIN_NO_CHANGE);
	}
	if (result != ESP_OK) {
		record_error(result);
		detach();
		return;
	}
	tx_attached = true;
	State::update([](State& state) { state.tx_attached = true; });
	init_sent = 0;
	if (!loop::try_submit("adsbee init", init)) {
		record_error(ESP_ERR_NO_MEM);
	}
}

loop::Result send(Request* request, loop::Handle handle) {
	update_tx();
	if (!started || !accepts_requests()) {
		record_error(ESP_ERR_INVALID_STATE);
		return loop::Result::error;
	}
	if (pending) {
		record_error(ESP_ERR_INVALID_STATE);
		return loop::Result::error;
	}
	request->handle = handle;
	request->uart_errors = uart_errors.load();
	request->finish = loop::schedule(
		request->window_ms,
		{"adsbee response window", [handle](loop::Handle, loop::Value&) {
			 finish_request(handle);
			 return loop::Result::done;
		 }});
	if (!request->finish.valid()) {
		record_error(ESP_ERR_NO_MEM);
		return loop::Result::error;
	}
	reply.length = 0;
	reply.text[0] = 0;
	const esp_err_t written = write_line(request->command);
	if (written != ESP_OK) {
		record_error(written);
		return loop::Result::error;
	}
	pending = request;
	if (request->on_written) {
		request->on_written();
	}
	return loop::Result::pending;
}

void on_mode(const events::Event& event, void*) {
	liveness.set_powered(event.data.mode.to == POWER_MODE_ON,
						 esp_timer_get_time());
	update_tx();
	if (event.data.mode.from == POWER_MODE_ON) {
		AntennaStatus antenna = AntennaStatus::unreported;
		State::update([&](State& state) {
			state.gnss.fix = Fix::none;
			antenna = state.gnss.antenna;
		});
		publish_fix(false, antenna);
	}
}
}

loop::Handle request(const char* command, loop::SuccessListener success,
					 loop::ErrorListener failure, uint32_t window_ms,
					 loop::Callable<void()> on_written) {
	if (!acceptable_command(command, kCommandCapacity) || !window_ms) {
		record_error(ESP_ERR_INVALID_ARG);
		return {};
	}
	const size_t command_size = std::strlen(command) + 1;
	void* const memory = operator new(sizeof(Request) + command_size,
									  std::nothrow);
	if (!memory) {
		return {};
	}
	auto* request = std::construct_at(static_cast<Request*>(memory));
	std::memcpy(request->command, command, command_size);
	request->window_ms = window_ms;
	request->success = success;
	request->failure = failure;
	request->on_written = on_written;
	loop::Handle handle = loop::submit(
		{"adsbee at",
		 [request](loop::Handle action, loop::Value&) {
			 return send(request, action);
		 }},
		[request](loop::Value value) {
			const loop::SuccessListener on_reply = request->success;
			after_request_cleanup(request, true);
			if (on_reply) {
				on_reply(value);
			}
		},
		[request](loop::Outcome outcome) {
			const loop::ErrorListener on_error = request->failure;
			after_request_cleanup(request, false);
			if (outcome == loop::Outcome::timeout) {
				record_error(ESP_ERR_TIMEOUT);
			}
			if (on_error) {
				on_error(outcome);
			}
		});
	if (!handle.valid()) {
		release(request);
	}
	return handle;
}

esp_err_t prepare() {
	return events::subscribe(events::mask_for(events::Type::mode_changed),
							 on_mode)
			   ? ESP_OK
			   : ESP_ERR_NO_MEM;
}

bool subscribe(Predicate predicate, Handler handler, void* context) {
	return subscribers.add({predicate, handler}, context);
}

bool subscribe_gnss(GnssHandler handler, void* context) {
	return gnss_subscribers.add(handler, context);
}

bool accepts_requests() {
	const auto link = State::copy();
	return link.tx_attached && link.initialized;
}

esp_err_t uart_init() {
	esp_err_t result =
		uart_driver_install(uart, ADSBEE_UART_RX_BUFFER, ADSBEE_UART_TX_BUFFER,
							ADSBEE_UART_QUEUE, &events, 0);
	if (result == ESP_OK) {
		result = uart_param_config(uart, &uart_config);
	}
	if (result == ESP_OK) {
		result = uart_set_pin(uart, UART_PIN_NO_CHANGE, BOARD_PIN_UART_RX,
							  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	}
	if (result != ESP_OK) {
		uart_driver_delete(uart);
		events = nullptr;
	}
	return result;
}

esp_err_t start() {
	if (!events) {
		return ESP_ERR_INVALID_STATE;
	}
	esp_err_t result = detach();
	if (result != ESP_OK) {
		return result;
	}
	adsbee_decoder_init(&decoder, on_message, nullptr);
	periodic = loop::schedule_periodic(
		ADSBEE_POLL_MS, {"adsbee liveness", [](loop::Handle, loop::Value&) {
							 update_tx();
							 expire_fix(esp_timer_get_time());
							 return loop::Result::done;
						 }});
	if (!periodic.valid()) {
		result = ESP_ERR_NO_MEM;
	}
	if (result == ESP_OK && xTaskCreate(link_task, "adsbee", 4096, nullptr, 10,
										nullptr) != pdPASS) {
		result = ESP_ERR_NO_MEM;
	}
	if (result != ESP_OK) {
		loop::cancel(periodic);
		periodic = {};
		record_error(result);
		return result;
	}
	started = true;
	return ESP_OK;
}

bool busy() { return pending != nullptr; }

void ingest_gnss(const Gnss& gnss, int64_t received_us) {
	expire_fix(received_us);
	State::update([&](State& state) {
		state.gnss = gnss;
		state.last_gnss_report_us = received_us;
	});
	if (!state_usable(received_us, esp_timer_get_time())) {
		publish_fix(false, gnss.antenna);
		return;
	}
	const bool has_fix = gnss.fix != Fix::none;
	gnss_subscribers.notify(gnss, received_us, has_fix && !fix_held);
	publish_fix(has_fix, gnss.antenna);
}

void reset_ingestion() {
	State::update([](State& state) {
		state.traffic_ingestion = Ingestion::read;
		state.gnss_ingestion = Ingestion::read;
	});
}

size_t device_info(char* out, size_t capacity) {
	const std::scoped_lock lock(device_info_mutex);
	size_t length = std::strlen(device_info_text);
	if (length >= capacity) {
		length = capacity - 1;
	}
	std::memcpy(out, device_info_text, length);
	out[length] = 0;
	return length;
}
}
