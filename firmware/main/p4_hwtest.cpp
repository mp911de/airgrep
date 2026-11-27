#include "console.h"
#include "usb/usb_device.hpp"

extern "C" esp_err_t console_register(const esp_console_cmd_t* command) {
	return esp_console_cmd_register(command);
}

extern "C" void app_main(void) {
	ESP_ERROR_CHECK(usb::start());
	esp_console_repl_config_t config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	config.prompt = "airgrep>";
	config.task_stack_size = 8192;
	const esp_console_dev_uart_config_t uart =
		ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
	esp_console_repl_t* repl = nullptr;
	ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart, &config, &repl));
	ESP_ERROR_CHECK(console_register_hwtest());
	ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
