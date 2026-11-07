# Requirements cannot depend on sdkconfig, the sources can: without the flag
# this component is header-only and nothing of it reaches the binary.
if (CONFIG_AIRGREP_HWTEST)
    set(srcs "hwtest.cpp")
endif ()
idf_component_register(SRCS ${srcs} INCLUDE_DIRS "include" PRIV_REQUIRES loop airgrep)
