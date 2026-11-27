#include "p4_maintenance.hpp"

#include <cstdio>

#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_partition.h"
#include "system/device_info.h"

namespace {
int hello(int, char**) {
	const auto* app = esp_partition_find_first(
		ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
	const auto* tz = esp_partition_find_first(
		ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(0x40),
		"tzdata");
	if (!app || !tz || app->encrypted || tz->encrypted) {
		return 1;
	}
	std::printf("\nAIRGREP/1\t%s\t%s\t%s\t%s\t%lu\t%lu\t%lu\t%lu\n",
				device_board_name(), device_serial(),
				esp_app_get_description()->version, device_variant(),
				(unsigned long)app->address, (unsigned long)app->size,
				(unsigned long)tz->address, (unsigned long)tz->size);
	return 0;
}
}

esp_err_t start_p4_maintenance() {
	esp_console_repl_config_t config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	config.prompt = "airgrep>";
	const esp_console_dev_uart_config_t uart =
		ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
	esp_console_repl_t* repl = nullptr;
	ESP_RETURN_ON_ERROR(esp_console_new_repl_uart(&uart, &config, &repl),
						"maintenance", "UART");
	esp_console_cmd_t command{};
	command.command = "hello";
	command.help = "Identify firmware and upgrade partitions";
	command.func = hello;
	ESP_RETURN_ON_ERROR(esp_console_cmd_register(&command), "maintenance",
						"hello");
	return esp_console_start_repl(repl);
}
