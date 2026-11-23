#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { TASK_NO_AFFINITY = -1 };
typedef struct {
	unsigned number;
	char name[32];
	uint64_t run_time;
	int core;
	unsigned priority;
	unsigned state;
	unsigned minimum_free_stack_bytes;
} task_sample_t;

typedef enum {
	TASK_ROW_SAMPLED,
	TASK_ROW_CREATED,
	TASK_ROW_DELETED
} task_row_kind_t;

typedef struct {
	task_row_kind_t kind;
	const task_sample_t* sample;  // the later snapshot for sampled and
								  // created, the earlier for deleted
	unsigned core_cpu_percent;
} task_row_t;

size_t task_stats_rows(const task_sample_t* before, size_t before_count,
					   const task_sample_t* after, size_t after_count,
					   uint64_t elapsed, task_row_t* rows, size_t capacity);

#ifdef __cplusplus
}
#endif
