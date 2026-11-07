#include "console.h"
#include <stdio.h>
#include <string.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "sdkconfig.h"

static struct {
	struct arg_str* tag;
	struct arg_str* level;
	struct arg_end* end;
} s_log_level_args;

static const char* s_level_names[] = {
	"none", "error", "warn", "info", "debug", "verbose"
};

// Same command as ESP-IDF's console example, cmd_system_common.c.
static int log_level(int argc, char** argv) {
	if (arg_parse(argc, argv, (void**)&s_log_level_args) != 0) {
		arg_print_errors(stderr, s_log_level_args.end, argv[0]);
		return 1;
	}
	const char* tag = s_log_level_args.tag->sval[0];
	const char* name = s_log_level_args.level->sval[0];
	esp_log_level_t level;
	for (level = ESP_LOG_NONE; level <= ESP_LOG_VERBOSE; level++) {
		if (strcmp(name, s_level_names[level]) == 0) {
			break;
		}
	}
	if (level > ESP_LOG_VERBOSE) {
		printf("Invalid log level '%s', choose from none|error|warn|info|debug|verbose\n",
				name);
		return 1;
	}
	if (level > CONFIG_LOG_MAXIMUM_LEVEL) {
		printf("Log level %s exceeds CONFIG_LOG_MAXIMUM_LEVEL %s\n", s_level_names[level],
				s_level_names[CONFIG_LOG_MAXIMUM_LEVEL]);
		return 1;
	}
	esp_log_level_set(tag, level);
	return 0;
}

static esp_err_t register_log_level(void) {
	s_log_level_args.tag = arg_str1(NULL, NULL, "<tag|*>", "Log tag, or * for all tags");
	s_log_level_args.level =
		arg_str1(NULL, NULL, "<none|error|warn|info|debug|verbose>", "Level");
	s_log_level_args.end = arg_end(2);
	const esp_console_cmd_t cmd = {
		.command = "log_level",
		.help = "Set the log level for all tags or one tag",
		.hint = NULL,
		.func = &log_level,
		.argtable = &s_log_level_args,
	};
	return esp_console_cmd_register(&cmd);
}

esp_err_t console_start(void) {
	esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	repl_config.prompt = "airgrep>";
	esp_console_dev_usb_serial_jtag_config_t dev_config =
		ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
	esp_console_repl_t* repl = NULL;
	esp_err_t err =
		esp_console_new_repl_usb_serial_jtag(&dev_config, &repl_config, &repl);
	if (err == ESP_OK) {
		err = esp_console_register_help_command();
	}
	if (err == ESP_OK) {
		err = register_log_level();
	}
	if (err == ESP_OK) {
		err = esp_console_start_repl(repl);
	}
	return err;
}
