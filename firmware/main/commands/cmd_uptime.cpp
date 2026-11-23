#include <cinttypes>
#include <cstdio>

#include "argtable.hpp"
#include "console.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "system/cpu_sampler.hpp"
#include "system/device_info.h"
#include "system/health.hpp"
#include "system/system.h"

#define UPTIME_LABEL_WIDTH 20

namespace {
struct {
	struct arg_lit* raw;
	struct arg_end* end;
	void* table[2];
} args;

void heap(const char* label, uint32_t caps, bool raw) {
	const size_t free = heap_caps_get_free_size(caps);
	const size_t minimum = heap_caps_get_minimum_free_size(caps);
	const size_t total = heap_caps_get_total_size(caps);
	if (raw) {
		printf("%-20s %zu free, %zu minimum, %zu total bytes\n", label, free,
			   minimum, total);
	} else {
		printf("%-20s %zu KB free, %zu KB minimum, %zu KB total\n", label,
			   free / 1024, minimum / 1024, total / 1024);
	}
}

#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
// Load since the previous uptime call, or since boot on the first one.
void load() {
	static cpu::Sampler window;
	const auto reading = cpu::sample(window, esp_timer_get_time());
	printf("%-*s", UPTIME_LABEL_WIDTH, "Load");
	for (size_t core = 0; core < reading.cores.size(); core++) {
		printf(" core%zu %u %%,", core, reading.cores[core]);
	}
	printf(" overall %u %%\n", reading.overall);
}
#else
void load() {
	printf("%-*s UNAVAILABLE, CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS is off\n",
		   UPTIME_LABEL_WIDTH, "Load");
}
#endif

int uptime(int argc, char** argv) {
	if (arg_parse(argc, argv, args.table)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	const bool raw = args.raw->count;
	device_info_print(UPTIME_LABEL_WIDTH);
	const uint64_t seconds = esp_timer_get_time() / 1000000;
	if (raw) {
		printf("%-20s %" PRIu64 " s\n", "Uptime", seconds);
	} else {
		printf("%-20s %" PRIu64 "d %02u:%02u:%02u\n", "Uptime", seconds / 86400,
			   (unsigned)(seconds / 3600 % 24), (unsigned)(seconds / 60 % 60),
			   (unsigned)(seconds % 60));
	}
	printf("%-20s %s\n", "Mode", power_mode_name(system_mode()));
	const uint16_t mask = system_health();
	if (raw) {
		printf("%-20s 0x%X\n", "Health", mask);
	} else {
		char text[256];
		health::format(mask, text, sizeof text);
		printf("%-20s %s\n", "Health", text);
	}
	load();
	heap("Internal RAM", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, raw);
	heap("PSRAM", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, raw);
	return 0;
}
}

extern "C" esp_err_t console_register_uptime(void) {
	args.raw = arg_lit0(nullptr, "raw",
						"Print seconds and bytes instead of d hh:mm:ss and KB");
	args.end = arg_end(2);
	args.table[0] = args.raw;
	args.table[1] = args.end;
	if (arg_nullcheck(args.table)) {
		return ESP_ERR_NO_MEM;
	}
	const esp_console_cmd_t command = {
		.command = "uptime",
		.help = "Show device identity, uptime, core load and heap.",
		.hint = "[--raw]",
		.func = uptime,
		.argtable = static_cast<void*>(args.table),
		.func_w_context = nullptr,
		.context = nullptr,
	};
	return console_register(&command);
}
