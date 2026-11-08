#include "console.h"
#include "commands/cmd_logs.h"
#include "esp_console.h"
#include <stdio.h>

#define CONSOLE_MAX_COMMANDS 16

// Copies of the entries. The string literals they point to live in flash.
static struct {
	const char *command, *hint, *help;
} commands[CONSOLE_MAX_COMMANDS];

static size_t command_count;

esp_err_t console_register(const esp_console_cmd_t* command) {
	if (command_count == CONSOLE_MAX_COMMANDS) {
		return ESP_ERR_NO_MEM;
	}
	esp_err_t err = esp_console_cmd_register(command);
	if (err == ESP_OK) {
		commands[command_count].command = command->command;
		commands[command_count].hint = command->hint ? command->hint : "";
		commands[command_count].help = command->help;
		command_count++;
	}
	return err;
}

// One line per command: name and synopsis padded to a column, then one sentence.
static int help(int argc, char** argv) {
	(void)argv;
	if (argc != 1) {
		printf("usage: help\n");
		return 1;
	}
	for (size_t i = 0; i < command_count; i++) {
		printf("%s%s%s\n  %s\n", commands[i].command, *commands[i].hint ? " " : "",
				commands[i].hint,
				commands[i].help);
		if (i + 1 < command_count) printf("\n");
	}
	return 0;
}

static esp_err_t register_help(void) {
	static const esp_console_cmd_t command = {
		.command = "help", .hint = "", .help = "List the commands with their synopsis.",
		.func = help,
	};
	return console_register(&command);
}

esp_err_t console_start(void) {
	esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	repl_config.prompt = "airgrep>";
	repl_config.task_stack_size = 8192;
	esp_console_dev_usb_serial_jtag_config_t dev_config =
		ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
	esp_console_repl_t* repl = NULL;
	esp_err_t err =
		esp_console_new_repl_usb_serial_jtag(&dev_config, &repl_config, &repl);
	if (err == ESP_OK) {
		err = register_help();
	}
	if (err == ESP_OK) {
		err = console_register_uptime();
	}
	if (err == ESP_OK) {
		err = console_register_tasks();
	}
	if (err == ESP_OK) {
		err = console_register_reboot();
	}
	if (err == ESP_OK) {
		err = console_register_exp();
	}
	if (err == ESP_OK) {
		err = console_register_charger();
	}
	if (err == ESP_OK) {
		err = console_register_battery_level();
	}
	if (err == ESP_OK) {
		err = console_register_baro();
	}
	if (err == ESP_OK) {
		err = console_register_fusb();
	}
	if (err == ESP_OK) {
		err = console_register_beep();
	}
	if (err == ESP_OK) {
		err = console_register_adsbee();
	}
	if (err == ESP_OK) {
		err = console_register_gnss();
	}
	if (err == ESP_OK) {
		err = console_register_clock();
	}
	if (err == ESP_OK) {
		err = logs_register();
	}
	if (err == ESP_OK) {
		err = logs_start();
	}
	if (err == ESP_OK) {
		err = esp_console_start_repl(repl);
		if (err != ESP_OK) logs_stop();
	}
	return err;
}
