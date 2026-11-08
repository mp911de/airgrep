#include "task_stats.h"
#include <stdlib.h>

unsigned core_load_percent(uint64_t idle_delta, uint64_t elapsed) {
	if (!elapsed || idle_delta >= elapsed) return 0;
	return (unsigned)((elapsed - idle_delta) * 100 / elapsed);
}

static int by_cpu_descending(const void* a, const void* b) {
	const task_row_t* x = a;
	const task_row_t* y = b;
	if (x->kind != y->kind) return x->kind - y->kind;
	return (int)y->cpu_percent - (int)x->cpu_percent;
}

size_t task_stats_rows(const task_sample_t* before, size_t before_count,
						const task_sample_t* after, size_t after_count,
						uint64_t elapsed, task_row_t* rows, size_t capacity) {
	size_t count = 0;
	for (size_t j = 0; j < after_count && count < capacity; j++) {
		const task_sample_t* earlier = NULL;
		for (size_t i = 0; i < before_count; i++)
			if (before[i].handle == after[j].handle) earlier = &before[i];
		task_row_t* row = &rows[count++];
		row->sample = &after[j];
		row->kind = earlier ? TASK_ROW_SAMPLED : TASK_ROW_CREATED;
		uint64_t spent = earlier ? after[j].run_time - earlier->run_time : 0;
		row->cpu_percent = elapsed && earlier ? (unsigned)(spent * 100 / elapsed) : 0;
	}
	for (size_t i = 0; i < before_count && count < capacity; i++) {
		bool present = false;
		for (size_t j = 0; j < after_count; j++)
			if (before[i].handle == after[j].handle) present = true;
		if (!present) rows[count++] = (task_row_t){TASK_ROW_DELETED, &before[i], 0};
	}
	qsort(rows, count, sizeof *rows, by_cpu_descending);
	return count;
}
