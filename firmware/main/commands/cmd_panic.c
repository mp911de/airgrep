#include <inttypes.h>
#include <stdio.h>

#include "console.h"
#include "esp_core_dump.h"
#include "esp_partition.h"

static int panic_command(int argc, char** argv) {
	(void)argv;
	if (argc != 1) {
		puts("usage: panic");
		return 1;
	}
	if (!esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
								  ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL)) {
		puts(
			"Core dump partition missing. Install the firmware partition "
			"table.");
		return 1;
	}
	esp_err_t result = esp_core_dump_image_check();
	if (result == ESP_ERR_NOT_FOUND) {
		puts("No saved panic.");
		return 0;
	}
	esp_core_dump_summary_t summary = {0};
	if (result == ESP_OK) {
		result = esp_core_dump_get_summary(&summary);
	}
	if (result != ESP_OK) {
		printf("Cannot read core dump: %s\n", esp_err_to_name(result));
		return 1;
	}
	char reason[200] = {0};
	if (esp_core_dump_get_panic_reason(reason, sizeof reason) == ESP_OK) {
		printf("Panic: %s\n", reason);
	}
	printf("Task: %.16s  PC: 0x%08" PRIx32 "  EXCCAUSE: 0x%" PRIx32
		   "  EXCVADDR: 0x%08" PRIx32 "\nELF SHA256: %.*s\nBacktrace:",
		   summary.exc_task, summary.exc_pc, summary.ex_info.exc_cause,
		   summary.ex_info.exc_vaddr, (int)sizeof summary.app_elf_sha256,
		   (const char*)summary.app_elf_sha256);
	for (uint32_t i = 0;
		 i < summary.exc_bt_info.depth &&
		 i < sizeof summary.exc_bt_info.bt / sizeof summary.exc_bt_info.bt[0];
		 ++i) {
		printf(" 0x%08" PRIx32, summary.exc_bt_info.bt[i]);
	}
	puts(summary.exc_bt_info.corrupted ? " (incomplete)" : "");
	return 0;
}

esp_err_t console_register_panic(void) {
	const esp_console_cmd_t command = {
		.command = "panic",
		.help = "Show the last panic saved to flash.",
		.func = panic_command,
	};
	return console_register(&command);
}
