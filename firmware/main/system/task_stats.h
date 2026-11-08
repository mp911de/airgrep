#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One task as seen in a run time snapshot. Hardware free, so the pairing logic has a host test.
typedef struct {
	const void* handle;
	const char* name;
	uint64_t run_time;
	int core; // -1 for no affinity
	unsigned priority;
	unsigned state; // eRunning, eReady, eBlocked, eSuspended, eDeleted order
	unsigned stack_free; // high water mark in bytes
} task_sample_t;

typedef enum { TASK_ROW_SAMPLED, TASK_ROW_CREATED, TASK_ROW_DELETED } task_row_kind_t;

typedef struct {
	task_row_kind_t kind;
	const task_sample_t* sample; // the later snapshot for sampled and created, the earlier for deleted
	unsigned cpu_percent; // of one core, over the window
} task_row_t;

// Pairs two snapshots by handle, computes the share of elapsed run time per task and sorts by
// CPU descending. Returns the number of rows written, at most capacity.
size_t task_stats_rows(const task_sample_t* before, size_t before_count,
						const task_sample_t* after, size_t after_count,
						uint64_t elapsed, task_row_t* rows, size_t capacity);

// Core load from idle run time: 100 * (1 - idle_delta / elapsed).
unsigned core_load_percent(uint64_t idle_delta, uint64_t elapsed);

#ifdef __cplusplus
}
#endif
