#include "adsbee_link.hpp"
#include <cstdlib>
#include <cstring>
#include "board.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LINK_UART UART_NUM_1
#define LINK_BAUD 115200
#define RX_BUF 2048
#define TX_BUF 512
#define SUBSCRIBERS 4
// A message the loop cannot dispatch within this time is dropped and freed.
#define DISPATCH_TIMEOUT_MS 1000

namespace adsbee {
	namespace {
		static const char* TAG = "adsbee";

		struct Subscriber {
			Predicate predicate;
			Handler handler;
			void* ctx;
		};

		Subscriber s_subscribers[SUBSCRIBERS];
		size_t s_subscriber_count;
		decoder_t s_decoder;
		QueueHandle_t s_events;
		loop::Handle s_request;

		// Runs on the loop.
		void dispatch(const Message& message) {
			for (size_t i = 0; i < s_subscriber_count; i++) {
				if (s_subscribers[i].predicate(message)) {
					s_subscribers[i].handler(message, s_subscribers[i].ctx);
				}
			}
		}

		void on_message(const char* text, size_t len, decoder_kind_t kind, void*) {
			Message* m = static_cast<Message*>(std::malloc(sizeof(Message) + len + 1));
			if (!m) {
				ESP_LOGW(TAG, "out of memory, dropped %u bytes", (unsigned)len);
				return;
			}
			m->kind = kind;
			m->len = len;
			std::memcpy(m->text, text, len + 1);
			loop::Action action = [m](loop::Handle, loop::Value&) {
				dispatch(*m);
				std::free(m);
				return loop::Result::done;
			};
			loop::ErrorListener on_error = [m](loop::Outcome) { std::free(m); };
			if (!loop::submit(loop::Event{"adsbee rx", action, DISPATCH_TIMEOUT_MS}, {},
							on_error).valid()) {
				std::free(m);
			}
		}

		void link_task(void*) {
			char chunk[128];
			uart_event_t event;
			for (;;) {
				if (xQueueReceive(s_events, &event, portMAX_DELAY) != pdTRUE) {
					continue;
				}
				switch (event.type) {
				case UART_DATA:
					for (size_t left = event.size; left > 0;) {
						int n = uart_read_bytes(
							LINK_UART, chunk, left < sizeof chunk ? left : sizeof chunk,
							0);
						if (n <= 0) {
							break;
						}
						decoder_feed(&s_decoder, chunk, n);
						left -= n;
					}
					break;
				case UART_PARITY_ERR:
				case UART_FRAME_ERR:
				case UART_FIFO_OVF:
				case UART_BUFFER_FULL:
					ESP_LOGW(TAG, "uart error %d, resync", event.type);
					uart_flush_input(LINK_UART);
					xQueueReset(s_events);
					decoder_error(&s_decoder);
					break;
				default:
					break;
				}
			}
		}

		bool is_at_reply(const Message& m) {
			return m.kind == DECODER_AT_REPLY;
		}

		void on_at_reply(const Message& m, void*) {
			if (!loop::pending(s_request)) {
				ESP_LOGD(TAG, "unsolicited %s", m.text);
				return;
			}
			loop::Handle h = s_request;
			s_request = {};
			loop::complete(h, loop::Value{.p = const_cast<Message*>(&m)});
		}
	} // namespace

	bool subscribe(Predicate predicate, Handler handler, void* ctx) {
		if (s_subscriber_count == SUBSCRIBERS) {
			return false;
		}
		s_subscribers[s_subscriber_count++] = Subscriber{predicate, handler, ctx};
		return true;
	}

	esp_err_t start() {
		if (!subscribe(is_at_reply, on_at_reply, nullptr)) {
			ESP_LOGE(TAG, "subscriber table full, AT replies not handled");
			return ESP_ERR_NO_MEM;
		}
		decoder_init(&s_decoder, on_message, nullptr);

		const uart_config_t config = {
			.baud_rate = LINK_BAUD,
			.data_bits = UART_DATA_8_BITS,
			.parity = UART_PARITY_EVEN,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.rx_flow_ctrl_thresh = 0,
			.rx_glitch_filt_thresh = 0,
			.source_clk = UART_SCLK_DEFAULT,
			.flags = {},
		};
		esp_err_t err = uart_driver_install(LINK_UART, RX_BUF, TX_BUF, 20, &s_events, 0);
		if (err == ESP_OK) {
			err = uart_param_config(LINK_UART, &config);
		}
		if (err == ESP_OK) {
			err = uart_set_pin(LINK_UART, BOARD_PIN_UART_TX, BOARD_PIN_UART_RX,
								UART_PIN_NO_CHANGE,
								UART_PIN_NO_CHANGE);
		}
		if (err == ESP_OK) {
			xTaskCreate(link_task, "adsbee", 4096, nullptr, 10, nullptr);
			ESP_LOGI(TAG, "uart1 rx %d tx %d, 115200 8E1", BOARD_PIN_UART_RX,
					BOARD_PIN_UART_TX);
		}
		return err;
	}

	loop::Handle request(const char* command, loop::SuccessListener on_reply,
						loop::ErrorListener on_error, uint32_t timeout_ms) {
		loop::Action action = [command](loop::Handle h, loop::Value&) {
			if (loop::pending(s_request)) {
				ESP_LOGW(TAG, "request outstanding, dropped %s", command);
				return loop::Result::error;
			}
			if (uart_write_bytes(LINK_UART, command, std::strlen(command)) < 0 ||
				uart_write_bytes(LINK_UART, "\n", 1) < 0) {
				return loop::Result::error;
			}
			s_request = h;
			return loop::Result::pending;
		};
		return loop::submit(loop::Event{"adsbee at", action, timeout_ms}, on_reply,
							on_error);
	}
} // namespace adsbee
