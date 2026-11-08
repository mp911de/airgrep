#include "console.h"
#include "device_info.h"

#define BANNER_LABEL_WIDTH 16
#include <stdio.h>

void console_banner(void) {
	printf("\n"
			"         __\n"
			"   __   /\\_\\  _ __    __   _ __    __   _____\n"
			" /'__`\\ \\/\\ \\/\\`'__\\/'_ `\\/\\`'__\\/'__`\\/\\ '__`\\\n"
			"/\\ \\L\\.\\_\\ \\ \\ \\ \\//\\ \\L\\ \\ \\ \\//\\  __/\\ \\ \\L\\ \\\n"
			"\\ \\__/.\\_\\\\ \\_\\ \\_\\\\ \\____ \\ \\_\\\\ \\____\\\\ \\ ,__/\n"
			" \\/__/\\/_/ \\/_/\\/_/ \\/___L\\ \\/_/ \\/____/ \\ \\ \\/\n"
			"                      /\\____/             \\ \\_\\\n"
			"                      \\_/__/               \\/_/\n"
			"\n");
	device_info_print(BANNER_LABEL_WIDTH);
	printf("\n");
}
