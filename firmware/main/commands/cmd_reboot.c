#include "console.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_console.h"
#include "esp_system.h"

static int reboot(int argc, char** argv) {
	(void)argv;
	if (argc != 1) {
		printf("usage: reboot\n");
		return 1;
	}
	printf("Rebooting...\n");
	if (fflush(stdout) != 0 || fsync(fileno(stdout)) != 0) {
		printf("reboot: output flush failed: %s\n", strerror(errno));
		return 1;
	}
	esp_restart();
	return 0;
}

esp_err_t console_register_reboot(void) {
	const esp_console_cmd_t command = {
		.command = "reboot",
		.hint = "",
		.help = "Flush the console output and restart the device.",
		.func = reboot,
	};
	return console_register(&command);
}
