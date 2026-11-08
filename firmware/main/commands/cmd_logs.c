#include "cmd_logs.h"
#include "console.h"
#include "log_ring.h"
#include "log_session.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define RING_SIZE (16 * 1024)
#define MAX_TAGS 16
#define LINE_SIZE 1024

static log_ring_t ring;
static SemaphoreHandle_t lock;
static vprintf_like_t previous;

static struct {
	struct arg_int* count;
	struct arg_str* level;
	struct arg_str* tags;
	struct arg_end* end;
} args;

static int capture(const char* format, va_list ap) {
	static char text[LINE_SIZE];
	xSemaphoreTake(lock, portMAX_DELAY);
	int length = vsnprintf(text, sizeof text, format, ap);
	if (length <= 0) {
		xSemaphoreGive(lock);
		return length;
	}
	size_t stored = (size_t)length;
	if (stored >= sizeof text) {
		static const char suffix[] = " [truncated]\n";
		stored = sizeof text - 1;
		memcpy(text + stored - sizeof suffix + 1, suffix, sizeof suffix);
	}
	log_ring_append(&ring, text, stored);
	xSemaphoreGive(lock);
	return length;
}

esp_err_t logs_start(void) {
	if (lock) return ESP_ERR_INVALID_STATE;
	char* storage = heap_caps_malloc(RING_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!storage) {
		ESP_LOGW("console", "PSRAM log ring unavailable, using internal RAM");
		storage = heap_caps_malloc(RING_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	}
	if (!storage) return ESP_ERR_NO_MEM;
	lock = xSemaphoreCreateMutex();
	if (!lock) {
		free(storage);
		return ESP_ERR_NO_MEM;
	}
	log_ring_init(&ring, storage, RING_SIZE);
	previous = esp_log_set_vprintf(capture);
	return ESP_OK;
}

void logs_stop(void) {
	if (lock) esp_log_set_vprintf(previous);
}

static int logs(int argc, char** argv) {
	static const char* levels[] = {"none", "error", "warn", "info", "debug", "verbose"};
	if (arg_parse(argc, argv, (void**)&args)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	if ((args.count->count && (args.count->ival[0] < 0 || args.level->count)) ||
		(args.tags->count && !args.level->count)) {
		printf("usage: logs [level [tag,...]] | -n N\n");
		return 1;
	}
	int level = 0;
	size_t tag_count = 0;
	char* tag_copy = NULL;
	const char* filter = args.tags->count ? args.tags->sval[0] : NULL;
	if (args.level->count) {
		for (; level <= ESP_LOG_VERBOSE; level++) {
			if (!strcmp(levels[level], args.level->sval[0])) break;
		}
		if (level > CONFIG_LOG_MAXIMUM_LEVEL) {
			printf("unsupported log level: %s\n", args.level->sval[0]);
			return 1;
		}
	}
	if (filter) {
		tag_copy = strdup(filter);
		if (!tag_copy) {
			printf("cannot allocate log filter\n");
			return 1;
		}
		char* next = tag_copy;
		while (next) {
			char* tag = strsep(&next, ",");
			if (!*tag || strpbrk(tag, "* \t\r\n:") || tag_count++ == MAX_TAGS) {
				printf("expected 1 to %d explicit comma-separated tags\n", MAX_TAGS);
				free(tag_copy);
				return 1;
			}
		}
	}
	char* line = malloc(LINE_SIZE);
	int flags = fcntl(STDIN_FILENO, F_GETFL);
	if (!line || flags < 0 || fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0) {
		printf("cannot start log reader\n");
		free(line);
		free(tag_copy);
		return 1;
	}
	if (args.level->count) log_session_begin(level, filter);
	xSemaphoreTake(lock, portMAX_DELAY);
	uint64_t cursor = args.count->count && args.count->ival[0] == 0
						? ring.end
						: log_ring_begin(&ring, filter,
										args.count->count ? args.count->ival[0] : 0);
	uint64_t replay_end = ring.end;
	xSemaphoreGive(lock);
	int result = 0;
	for (;;) {
		if (args.count->count && cursor >= replay_end) break;
		if (!args.count->count) {
			char key;
			ssize_t got = read(STDIN_FILENO, &key, 1);
			if (got > 0) break;
			if (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
				printf("log input failed: %s\n", strerror(errno));
				result = 1;
				break;
			}
		}
		xSemaphoreTake(lock, portMAX_DELAY);
		log_ring_t view = ring;
		if (args.count->count) view.end = replay_end;
		bool found = log_ring_next(&view, &cursor, filter, line, LINE_SIZE);
		xSemaphoreGive(lock);
		if (found) fputs(line, stdout);
		else if (args.count->count) break;
		else vTaskDelay(pdMS_TO_TICKS(20));
	}
	if (args.level->count) log_session_end();
	if (fcntl(STDIN_FILENO, F_SETFL, flags) < 0) {
		printf("cannot restore console input\n");
		result = 1;
	}
	free(line);
	free(tag_copy);
	return result;
}

esp_err_t logs_register(void) {
	args.count = arg_int0("n", NULL, "N", "Replay the last N lines without tailing");
	args.level = arg_str0(NULL, NULL, "level", "Temporary log level");
	args.tags = arg_str0(NULL, NULL, "tag,...",
						"Tags to display and raise for this session");
	args.end = arg_end(3);
	if (arg_nullcheck((void**)&args)) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t command = {
		.command = "logs",
		.hint = "[level [tag,...]] | -n N",
		.help = "Replay logs and tail until a key, or replay the last N lines.",
		.func = logs,
		.argtable = &args,
	};
	return console_register(&command);
}
