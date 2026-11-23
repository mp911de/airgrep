# Requirements cannot depend on sdkconfig, the sources can: the benchmarks of the conflict engine
# exist where the engine does, in a build with a display.
set(srcs "hwtest.cpp")
if (CONFIG_AIRGREP_DISPLAY)
    list(APPEND srcs "scenario.cpp" "benchmark.cpp")
endif ()
idf_component_register(SRCS ${srcs} INCLUDE_DIRS "include" PRIV_REQUIRES loop airgrep esp_timer esp_system display daidalus)
if (CONFIG_AIRGREP_DISPLAY)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE HWTEST_BENCHMARKS=1)
endif ()
