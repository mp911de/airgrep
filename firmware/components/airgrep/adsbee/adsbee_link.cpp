#include "adsbee_link.hpp"
#include <cstdlib>
#include <cstring>
#include <new>
#include <atomic>

#include "adsbee_protocol.hpp"
#include "board.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "i2c_expander/tca9534.hpp"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace adsbee {
	portMUX_TYPE State::mutex_ = portMUX_INITIALIZER_UNLOCKED;

	State& State::storage() {
		static State state;
		return state;
	}

	State State::copy() {
		State result;
		update([&](State& state) { result = state; });
		return result;
	}

	namespace {
		constexpr const char* tag = "adsbee";
		constexpr uart_port_t uart = UART_NUM_1;
		constexpr uart_config_t uart_config = {
			.baud_rate = ADSBEE_UART_BAUD, .data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_EVEN,
			.stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.rx_flow_ctrl_thresh = 0, .rx_glitch_filt_thresh = 0,
			.source_clk = UART_SCLK_DEFAULT, .flags = {},
		};
		constexpr gpio_num_t tx = static_cast<gpio_num_t>(BOARD_PIN_UART_TX);

		struct Subscriber {
			Predicate predicate;
			Handler handler;
			void* context;
		} subscribers[4];

		size_t subscriber_count;
		decoder_t decoder;
		QueueHandle_t events;
		bool started, tx_attached;
		Link liveness;
		loop::Handle periodic;
		void update_tx();
		struct Request {
			char command[256]{};
			uint32_t window_ms = 0;
			loop::SuccessListener success;
			loop::ErrorListener failure;
			loop::Handle handle, finish;
			Reply reply;
			uint32_t uart_errors = 0;
		};

		// Request state belongs to the loop task. Only uart_errors is shared with the link task.
		Request* pending;
		std::atomic<uint32_t> uart_errors;

		char device_info_text[2048];
		StaticSemaphore_t device_info_storage;
		SemaphoreHandle_t device_info_lock = xSemaphoreCreateMutexStatic(&device_info_storage);

		void record_error(esp_err_t error) {
			int64_t now = esp_timer_get_time();
			State::update([&](State& state) {
				state.last_error = error;
				state.error_us = now;
			});
		}

		esp_err_t detach() {
			const gpio_config_t config = {
				.pin_bit_mask = 1ULL << tx, .mode = GPIO_MODE_INPUT,
				.pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
				.intr_type = GPIO_INTR_DISABLE,
			};
			esp_err_t result = gpio_config(&config);
			tx_attached = false;
			State::update([](State& state) { state.tx_attached = false; });
			if (pending) {
				record_error(ESP_ERR_INVALID_STATE);
				loop::fail(pending->handle, loop::Outcome::error);
			}
			if (result != ESP_OK) record_error(result);
			return result;
		}

		// Ends a request on either path: frees it, cancels the response window and
		// keeps the reply to an explicit AT+DEVICE_INFO?.
		void settle(Request* request, const Message* reply) {
			if (pending == request) pending = nullptr;
			loop::cancel(request->finish);
			if (reply && std::strcmp(request->command, "AT+DEVICE_INFO?") == 0) {
				xSemaphoreTake(device_info_lock, portMAX_DELAY);
				std::memcpy(device_info_text, reply->text, reply->len + 1);
				xSemaphoreGive(device_info_lock);
			}
			delete request;
		}

		void finish_request(loop::Handle handle) {
			Message* message = nullptr;
			esp_err_t error = ESP_OK;
			Request* request = pending;
			if (!request || request->handle.slot != handle.slot ||
				request->handle.generation != handle.generation)
				return;
			if (request->uart_errors != uart_errors.load()) error = ESP_ERR_INVALID_RESPONSE;
			else if (!request->reply.length) error = ESP_ERR_TIMEOUT;
			else {
				message = static_cast<Message*>(std::malloc(
					sizeof(Message) + request->reply.length + 1));
				if (!message) error = ESP_ERR_NO_MEM;
				else {
					message->kind = DECODER_AT_REPLY;
					message->len = request->reply.length;
					message->received_us = esp_timer_get_time();
					std::memcpy(message->text, request->reply.text, message->len + 1);
				}
			}
			if (error != ESP_OK) {
				record_error(error);
				loop::fail(handle, error == ESP_ERR_TIMEOUT
										? loop::Outcome::timeout
										: loop::Outcome::error);
				return;
			}
			loop::complete(handle, loop::Value{.p = message});
			std::free(message);
		}

		void receive(const Message& message) {
			if (message.kind == DECODER_CSBEE) {
				liveness.poll(esp_timer_get_time());
				liveness.received(message.received_us);
				update_tx();
			}
			if (message.kind == DECODER_AT_REPLY) ESP_LOGD(tag, "reply %s", message.text);
			if (message.kind == DECODER_AT_REPLY && pending &&
				!pending->reply.append(message.text, message.len)) {
				record_error(ESP_ERR_INVALID_SIZE);
				loop::fail(pending->handle, loop::Outcome::error);
			}
			for (size_t i = 0; i < subscriber_count; i++)
				if (subscribers[i].predicate(message))
					subscribers[i].handler(
						message, subscribers[i].context);
		}

		void on_message(const char* text, size_t length, decoder_kind_t kind, void*) {
			bool valid = kind != DECODER_CSBEE || valid_csbee(text, length);
			int64_t now = esp_timer_get_time();
			State::update([&](State& state) {
				if (kind == DECODER_CSBEE) {
					++state.telemetry_lines;
					if (!valid) ++state.crc_failures;
					else state.updated_us = now;
				}
				if (kind == DECODER_AT_REPLY) state.reply_us = now;
			});
			if (!valid) return;
			auto message = static_cast<Message*>(std::malloc(
				sizeof(Message) + length + 1));
			if (!message) {
				record_error(ESP_ERR_NO_MEM);
				return;
			}
			message->kind = kind;
			message->len = length;
			message->received_us = now;
			std::memcpy(message->text, text, length + 1);
			if (!loop::hsubmit({
								"adsbee rx", [message](loop::Handle, loop::Value&) {
									receive(*message);
									std::free(message);
									return loop::Result::done;
								},
								1000
							}, {}, [message](loop::Outcome) {
								std::free(message);
							})) {
				std::free(message);
				record_error(ESP_ERR_NO_MEM);
			}
		}

		void link_task(void*) {
			char bytes[128];
			uart_event_t event;
			for (;;) {
				if (xQueueReceive(events, &event, portMAX_DELAY) != pdTRUE) continue;
				if (event.type == UART_DATA) {
					for (size_t left = event.size; left;) {
						int count = uart_read_bytes(uart, bytes,
													left < sizeof bytes
														? left
														: sizeof bytes, 0);
						if (count <= 0) break;
						adsbee_decoder_feed(&decoder, bytes, count);
						left -= count;
					}
				}
				else if (event.type == UART_PARITY_ERR || event.type == UART_FRAME_ERR ||
					event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL) {
					++uart_errors;
					State::update([&](State& state) {
						if (event.type == UART_PARITY_ERR) ++state.parity_errors;
						else if (event.type == UART_FRAME_ERR) ++state.framing_errors;
						else ++state.overruns;
					});
					record_error(ESP_FAIL);
					uart_flush_input(uart);
					xQueueReset(events);
					adsbee_decoder_error(&decoder);
				}
			}
		}

		void update_tx() {
			int64_t now = esp_timer_get_time();
			liveness.poll(now);
			bool alive = liveness.permits_tx(now);
			State::update([&](State& state) { state.known = alive; });
			if (!started || !alive) {
				if (tx_attached) detach();
				return;
			}
			if (!tx_attached) {
				// Never reattach while a cancelled command is still shifting out.
				esp_err_t result = uart_wait_tx_done(uart, 0);
				if (result == ESP_OK)
					result = uart_set_pin(uart, tx, UART_PIN_NO_CHANGE,
										UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
				if (result != ESP_OK) {
					record_error(result);
					detach();
					return;
				}
				tx_attached = true;
				State::update([](State& state) { state.tx_attached = true; });
			}
		}

		// Runs on the loop: claims the single request slot, opens the response
		// window and writes the command. Returns pending until the window closes.
		loop::Result send(Request* request, loop::Handle handle) {
			update_tx();
			if (!started || !tx_attached) {
				record_error(ESP_ERR_INVALID_STATE);
				return loop::Result::error;
			}
			if (pending) {
				record_error(ESP_ERR_INVALID_STATE);
				return loop::Result::error;
			}
			request->handle = handle;
			request->uart_errors = uart_errors.load();
			request->finish = loop::schedule(request->window_ms, {
												"adsbee response window",
												[handle](loop::Handle, loop::Value&) {
													finish_request(handle);
													return loop::Result::done;
												}
											});
			if (!request->finish.valid()) {
				record_error(ESP_ERR_NO_MEM);
				return loop::Result::error;
			}
			pending = request;
			if (uart_write_bytes(uart, request->command, std::strlen(request->command)) <
				0 ||
				uart_write_bytes(uart, "\n", 1) < 0) {
				record_error(ESP_FAIL);
				return loop::Result::error;
			}
			return loop::Result::pending;
		}

		loop::Handle submit_request(const char* command, uint32_t window_ms,
									loop::SuccessListener success,
									loop::ErrorListener failure) {
			if (!acceptable_command(command, sizeof Request::command) || !window_ms) {
				record_error(ESP_ERR_INVALID_ARG);
				return {};
			}
			auto* request = new(std::nothrow) Request;
			if (!request) return {};
			std::strcpy(request->command, command);
			request->window_ms = window_ms;
			request->success = success;
			request->failure = failure;
			loop::Handle handle = loop::submit(
				{
					"adsbee at", [request](loop::Handle action, loop::Value&) {
						return send(request, action);
					}
				},
				[request](loop::Value value) {
					loop::SuccessListener on_reply = request->success;
					settle(request, static_cast<const Message*>(value.p));
					if (on_reply) on_reply(value);
				},
				[request](loop::Outcome outcome) {
					loop::ErrorListener on_error = request->failure;
					settle(request, nullptr);
					if (outcome == loop::Outcome::timeout) record_error(ESP_ERR_TIMEOUT);
					if (on_error) on_error(outcome);
				});
			if (!handle.valid()) {
				delete request;
			}
			return handle;
		}

		void power_sample(const expander::State& state, uint8_t, void*) {
			liveness.update_power({state.input.power, state.updated_us, state.error_us},
								esp_timer_get_time());
			update_tx();
		}
	}

	bool subscribe(Predicate predicate, Handler handler, void* context) {
		if (!predicate || !handler || subscriber_count == 4) return false;
		subscribers[subscriber_count++] = {predicate, handler, context};
		return true;
	}

	loop::Handle poll_handle() { return periodic; }

	esp_err_t start() {
		if (!expander::subscribe(power_sample)) return ESP_ERR_NO_MEM;
		esp_err_t result = detach();
		if (result != ESP_OK) {
			return result;
		}
		adsbee_decoder_init(&decoder, on_message, nullptr);

		result = uart_driver_install(uart, ADSBEE_UART_RX_BUFFER, ADSBEE_UART_TX_BUFFER,
									ADSBEE_UART_QUEUE, &events, 0);
		if (result != ESP_OK) {
			return result;
		}
		result = uart_param_config(uart, &uart_config);
		if (result == ESP_OK) {
			result = uart_set_pin(uart, UART_PIN_NO_CHANGE, BOARD_PIN_UART_RX,
								UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
		}
		if (result == ESP_OK) {
			periodic = loop::schedule_periodic(ADSBEE_POLL_MS, {
													"adsbee liveness",
													[](loop::Handle, loop::Value&) {
														update_tx();
														return loop::Result::done;
													}
												});
			if (!periodic.valid()) {
				result = ESP_ERR_NO_MEM;
			}
		}
		if (result == ESP_OK && xTaskCreate(link_task, "adsbee", 4096, nullptr, 10,
											nullptr) != pdPASS) {
			result = ESP_ERR_NO_MEM;
		}
		if (result != ESP_OK) {
			loop::cancel(periodic);
			periodic = {};
			uart_driver_delete(uart);
			record_error(result);
			return result;
		}
		started = true;
		return ESP_OK;
	}

	loop::Handle request(const char* command, loop::SuccessListener success,
						loop::ErrorListener failure, uint32_t timeout_ms) {
		return submit_request(command, timeout_ms, success, failure);
	}

	size_t device_info(char* out, size_t capacity) {
		xSemaphoreTake(device_info_lock, portMAX_DELAY);
		size_t length = std::strlen(device_info_text);
		if (length >= capacity) length = capacity - 1;
		std::memcpy(out, device_info_text, length);
		out[length] = 0;
		xSemaphoreGive(device_info_lock);
		return length;
	}
}
