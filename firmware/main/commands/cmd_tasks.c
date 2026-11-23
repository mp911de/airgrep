#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "argtable3/argtable3.h"
#include "console.h"
#include "esp_console.h"
#include "esp_private/freertos_idf_additions_priv.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "task_stats.h"

#define TASKS_DEFAULT_WINDOW_S 1
#define TASKS_MAX 32

static struct {
	struct arg_int* seconds;
	struct arg_end* end;
	void* table[2];
} args;

#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
static size_t snapshot(task_sample_t* out, TaskStatus_t* status) {
	configRUN_TIME_COUNTER_TYPE total = 0;
	_Static_assert(configMAX_TASK_NAME_LEN <= sizeof out->name,
				   "task name exceeds snapshot");
#if !CONFIG_FREERTOS_SMP && configNUM_CORES > 1
	prvTakeKernelLock();
#elif CONFIG_FREERTOS_SMP
	taskENTER_CRITICAL();
#else
	vTaskSuspendAll();
#endif
	size_t count = uxTaskGetSystemState(status, TASKS_MAX, &total);
	for (size_t i = 0; i < count; i++) {
		BaseType_t core = xTaskGetCoreID(status[i].xHandle);
		out[i] = (task_sample_t){
			.number = status[i].xTaskNumber,
			.run_time = status[i].ulRunTimeCounter,
			.core = core == tskNO_AFFINITY ? TASK_NO_AFFINITY : (int)core,
			.priority = status[i].uxCurrentPriority,
			.state = status[i].eCurrentState,
			.minimum_free_stack_bytes =
				status[i].usStackHighWaterMark * sizeof(StackType_t),
		};
		strlcpy(out[i].name, status[i].pcTaskName, sizeof out[i].name);
	}
#if !CONFIG_FREERTOS_SMP && configNUM_CORES > 1
	prvReleaseKernelLock();
#elif CONFIG_FREERTOS_SMP
	taskEXIT_CRITICAL();
#else
	xTaskResumeAll();
#endif
	return count;
}

static int tasks(int argc, char** argv) {
	if (arg_parse(argc, argv, args.table)) {
		arg_print_errors(stderr, args.end, argv[0]);
		return 1;
	}
	int seconds =
		args.seconds->count ? args.seconds->ival[0] : TASKS_DEFAULT_WINDOW_S;
	if (seconds < 1 || seconds > 60) {
		printf("usage: tasks [seconds], 1 to 60\n");
		return 1;
	}

	TaskStatus_t* status = calloc(TASKS_MAX, sizeof *status);
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
	size_t after = snapshot(samples + TASKS_MAX, status);
	uint64_t elapsed = esp_timer_get_time() - start;
	size_t count = task_stats_rows(samples, before, samples + TASKS_MAX, after,
								   elapsed, rows, 2 * TASKS_MAX);
	static const char* states[] = {
		[eRunning] = "RUNNING", [eReady] = "READY",
		[eBlocked] = "BLOCKED", [eSuspended] = "SUSPENDED",
		[eDeleted] = "DELETED", [eInvalid] = "INVALID"};
	printf("%-16s %-5s %-5s %-10s %-5s %s\n", "Task", "Core", "Prio", "State",
		   "CPU", "Stack");
	for (size_t i = 0; i < count; i++) {
		const task_sample_t* s = rows[i].sample;
		char core[12] = "any";
		if (s->core != TASK_NO_AFFINITY) {
			(void)snprintf(core, sizeof core, "%d", s->core);
		}
		if (rows[i].kind == TASK_ROW_SAMPLED) {
			printf("%-16s %-5s %-5u %-10s %3u %% %u\n", s->name, core,
				   s->priority,
				   s->state < sizeof states / sizeof *states ? states[s->state]
															 : "?",
				   rows[i].core_cpu_percent, s->minimum_free_stack_bytes);
		} else {
			printf("%-16s %-5s %-5u %-10s\n", s->name, core, s->priority,
				   rows[i].kind == TASK_ROW_CREATED ? "CREATED" : "DELETED");
		}
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
	args.seconds =
		arg_int0(NULL, NULL, "seconds", "Sampling window, default 1");
	args.end = arg_end(2);
	args.table[0] = args.seconds;
	args.table[1] = args.end;
	if (arg_nullcheck(args.table)) {
		return ESP_ERR_NO_MEM;
	}
	const esp_console_cmd_t command = {
		.command = "tasks",
		.hint = "[seconds]",
		.help =
			"Sample CPU share, state and stack of every task over a window.",
		.func = tasks,
		.argtable = (void*)args.table,
	};
	return console_register(&command);
}
