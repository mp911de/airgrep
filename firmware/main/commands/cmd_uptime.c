#include "console.h"
#include <inttypes.h>
#include <stdio.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "device_info.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "task_stats.h"

#define UPTIME_LABEL_WIDTH 20

static struct { struct arg_lit* raw; struct arg_end* end; } args;

static void heap(const char* label, uint32_t caps, bool raw) {
	size_t free = heap_caps_get_free_size(caps), minimum = heap_caps_get_minimum_free_size(caps),
			total = heap_caps_get_total_size(caps);
	if (raw) printf("%-20s %zu free, %zu minimum, %zu total bytes\n", label, free, minimum, total);
	else printf("%-20s %zu KB free, %zu KB minimum, %zu KB total\n", label, free / 1024, minimum / 1024,
				total / 1024);
}

#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
// Load since the previous uptime call, or since boot on the first one.
static void load(void) {
	static uint64_t idle_at[CONFIG_FREERTOS_NUMBER_OF_CORES], time_at;
	uint64_t now = esp_timer_get_time(), elapsed = now - time_at;
	unsigned sum = 0;
	printf("%-*s", UPTIME_LABEL_WIDTH, "Load");
	for (int core = 0; core < CONFIG_FREERTOS_NUMBER_OF_CORES; core++) {
		uint64_t idle = ulTaskGetIdleRunTimeCounterForCore(core);
		unsigned percent = core_load_percent(idle - idle_at[core], elapsed);
		sum += percent;
		idle_at[core] = idle;
		printf(" core%d %u %%,", core, percent);
	}
	time_at = now;
	printf(" overall %u %%\n", sum / CONFIG_FREERTOS_NUMBER_OF_CORES);
}
#else
static void load(void) {
	printf("%-*s UNAVAILABLE, CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS is off\n", UPTIME_LABEL_WIDTH, "Load");
}
#endif

static int uptime(int argc, char** argv) {
	if (arg_parse(argc, argv, (void**)&args)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	bool raw = args.raw->count;
	device_info_print(UPTIME_LABEL_WIDTH);
	uint64_t seconds = esp_timer_get_time() / 1000000;
	if (raw) printf("%-20s %" PRIu64 " s\n", "Uptime", seconds);
	else printf("%-20s %" PRIu64 "d %02u:%02u:%02u\n", "Uptime", seconds / 86400,
				(unsigned)(seconds / 3600 % 24), (unsigned)(seconds / 60 % 60), (unsigned)(seconds % 60));
	load();
	heap("Internal RAM", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, raw);
	heap("PSRAM", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, raw);
	return 0;
}

esp_err_t console_register_uptime(void) {
	args.raw = arg_lit0(NULL, "raw", "Print seconds and bytes instead of d hh:mm:ss and KB");
	args.end = arg_end(2);
	if (arg_nullcheck((void**)&args)) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t command = {
		.command = "uptime",
		.hint = "[--raw]",
		.help = "Show device identity, uptime, core load and heap.",
		.func = uptime,
		.argtable = &args,
	};
	return console_register(&command);
}
