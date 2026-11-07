#include "adsbee_link.h"
#include <string.h>
#include "board.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "line_splitter.h"

#define LINK_UART UART_NUM_1
#define LINK_BAUD 115200
#define RX_BUF 2048
#define TX_BUF 512

static const char* TAG = "adsbee";
static adsbee_line_cb s_on_line;
static line_splitter_t s_splitter;
static QueueHandle_t s_events;

static void forward(const char* line, size_t len, bool csbee, void* ctx) {
	s_on_line(line, len, csbee);
}

static void link_task(void* arg) {
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
					LINK_UART, chunk, left < sizeof chunk ? left : sizeof chunk, 0);
				if (n <= 0) {
					break;
				}
				line_splitter_feed(&s_splitter, chunk, n);
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
			line_splitter_error(&s_splitter);
			break;
		default:
			break;
		}
	}
}

esp_err_t adsbee_link_start(adsbee_line_cb on_line) {
	s_on_line = on_line;
	line_splitter_init(&s_splitter, forward, NULL);

	const uart_config_t config = {
		.baud_rate = LINK_BAUD,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_EVEN,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	esp_err_t err = uart_driver_install(LINK_UART, RX_BUF, TX_BUF, 20, &s_events, 0);
	if (err == ESP_OK) {
		err = uart_param_config(LINK_UART, &config);
	}
	if (err == ESP_OK) {
		err = uart_set_pin(LINK_UART, BOARD_PIN_UART_TX, BOARD_PIN_UART_RX,
		                   UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	}
	if (err == ESP_OK) {
		xTaskCreate(link_task, "adsbee", 4096, NULL, 10, NULL);
		ESP_LOGI(TAG, "uart1 rx %d tx %d, 115200 8E1", BOARD_PIN_UART_RX,
		         BOARD_PIN_UART_TX);
	}
	return err;
}

esp_err_t adsbee_link_send(const char* command) {
	int n = uart_write_bytes(LINK_UART, command, strlen(command));
	if (n < 0 || uart_write_bytes(LINK_UART, "\n", 1) < 0) {
		return ESP_FAIL;
	}
	return ESP_OK;
}
