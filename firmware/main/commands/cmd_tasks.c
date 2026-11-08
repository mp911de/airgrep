#include "console.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "task_stats.h"

#define TASKS_DEFAULT_WINDOW_S 1
#define TASKS_MAX 32

static struct { struct arg_int* seconds; struct arg_end* end; } args;

#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
static size_t snapshot(task_sample_t* out, TaskStatus_t* status) {
	configRUN_TIME_COUNTER_TYPE total;
	size_t count = uxTaskGetSystemState(status, TASKS_MAX, &total);
	for (size_t i = 0; i < count; i++) {
		out[i] = (task_sample_t){
			status[i].xHandle, status[i].pcTaskName, status[i].ulRunTimeCounter,
			xTaskGetCoreID(status[i].xHandle) == tskNO_AFFINITY ? -1 : (int)xTaskGetCoreID(status[i].xHandle),
			status[i].uxCurrentPriority, status[i].eCurrentState,
			status[i].usStackHighWaterMark * sizeof(StackType_t),
		};
	}
	return count;
}

static int tasks(int argc, char** argv) {
	if (arg_parse(argc, argv, (void**)&args)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	int seconds = args.seconds->count ? args.seconds->ival[0] : TASKS_DEFAULT_WINDOW_S;
	if (seconds < 1 || seconds > 60) {
		printf("usage: tasks [seconds], 1 to 60\n");
		return 1;
	}
	// Two TaskStatus_t arrays stay alive until the rows are printed, the samples point into them.
	TaskStatus_t* status = calloc(2 * TASKS_MAX, sizeof *status);
	task_sample_t* samples = calloc(2 * TASKS_MAX, sizeof *samples);
	task_row_t rows[2 * TASKS_MAX];
	if (!status || !samples) {
		free(status);
		free(samples);
		printf("FAIL %s\n", esp_err_to_name(ESP_ERR_NO_MEM));
		return 1;
	}
	int64_t start = esp_timer_get_time();
	size_t before = snapshot(samples, status);
	vTaskDelay(pdMS_TO_TICKS(seconds * 1000));
	size_t after = snapshot(samples + TASKS_MAX, status + TASKS_MAX);
	uint64_t elapsed = esp_timer_get_time() - start;
	size_t count = task_stats_rows(samples, before, samples + TASKS_MAX, after, elapsed, rows, 2 * TASKS_MAX);
	static const char* states[] = {"RUNNING", "READY", "BLOCKED", "SUSPENDED", "DELETED", "INVALID"};
	printf("%-16s %-5s %-5s %-10s %-5s %s\n", "Task", "Core", "Prio", "State", "CPU", "Stack");
	for (size_t i = 0; i < count; i++) {
		const task_sample_t* s = rows[i].sample;
		char core[12] = "any";
		if (s->core >= 0) snprintf(core, sizeof core, "%d", s->core);
		if (rows[i].kind == TASK_ROW_SAMPLED)
			printf("%-16s %-5s %-5u %-10s %3u %% %u\n", s->name, core, s->priority,
					s->state < 6 ? states[s->state] : "?", rows[i].cpu_percent, s->stack_free);
		else printf("%-16s %-5s %-5u %-10s\n", s->name, core, s->priority,
					rows[i].kind == TASK_ROW_CREATED ? "CREATED" : "DELETED");
	}
	free(status);
	free(samples);
	return 0;
}
#else
static int tasks(int, char**) {
	printf("UNAVAILABLE, CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS is off\n");
	return 1;
}
#endif

esp_err_t console_register_tasks(void) {
	args.seconds = arg_int0(NULL, NULL, "seconds", "Sampling window, default 1");
	args.end = arg_end(2);
	if (arg_nullcheck((void**)&args)) return ESP_ERR_NO_MEM;
	const esp_console_cmd_t command = {
		.command = "tasks",
		.hint = "[seconds]",
		.help = "Sample CPU share, state and stack of every task over a window.",
		.func = tasks,
		.argtable = &args,
	};
	return console_register(&command);
}
