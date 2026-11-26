#pragma once

#ifdef __cplusplus
extern "C" {
#endif
void hwtest_draw_begin(const char* name);
void hwtest_draw_end(const char* name);
#ifdef __cplusplus
}
#endif

#define LV_PROFILER_BEGIN hwtest_draw_begin(__func__)
#define LV_PROFILER_END hwtest_draw_end(__func__)
