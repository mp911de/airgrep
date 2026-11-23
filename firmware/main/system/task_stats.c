#include "task_stats.h"

#include <stdlib.h>

static const task_sample_t* find_task(const task_sample_t* samples,
									  size_t count, unsigned number) {
	for (size_t i = 0; i < count; i++) {
		if (samples[i].number == number) {
			return &samples[i];
		}
	}
	return NULL;
}

static int by_cpu_descending(const void* a, const void* b) {
	const task_row_t* x = a;
	const task_row_t* y = b;
	if (x->kind != y->kind) {
		return (int)x->kind - (int)y->kind;
	}
	return (int)y->core_cpu_percent - (int)x->core_cpu_percent;
}

size_t task_stats_rows(const task_sample_t* before, size_t before_count,
					   const task_sample_t* after, size_t after_count,
					   uint64_t elapsed, task_row_t* rows, size_t capacity) {
	size_t count = 0;
	for (size_t j = 0; j < after_count && count < capacity; j++) {
		const task_sample_t* earlier =
			find_task(before, before_count, after[j].number);
		task_row_t* row = &rows[count++];
		row->sample = &after[j];
		row->kind = earlier ? TASK_ROW_SAMPLED : TASK_ROW_CREATED;
		uint64_t spent = earlier ? after[j].run_time - earlier->run_time : 0;
		row->core_cpu_percent =
			elapsed && earlier ? (unsigned)(spent * 100 / elapsed) : 0;
	}
	for (size_t i = 0; i < before_count && count < capacity; i++) {
		if (!find_task(after, after_count, before[i].number)) {
			rows[count++] = (task_row_t){TASK_ROW_DELETED, &before[i], 0};
		}
	}
	qsort(rows, count, sizeof *rows, by_cpu_descending);
	return count;
}
