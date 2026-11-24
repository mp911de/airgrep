# One display variant per build, the directory in variants/ that CONFIG_AIRGREP_DISPLAY names.
# Requirements cannot depend on sdkconfig, the sources can: without a variant this
# component is empty. LVGL is a dependency of every build, so that no build removes what
# another one downloaded. Without a variant it compiles and the linker leaves it out.
if (CONFIG_AIRGREP_DISPLAY)
    set(variant "variants/${CONFIG_AIRGREP_DISPLAY}")
    if (NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/${variant}/screen.cpp")
        message(FATAL_ERROR "CONFIG_AIRGREP_DISPLAY names no directory in components/display/variants")
    endif ()
    include("${CMAKE_CURRENT_LIST_DIR}/${variant}/sources.cmake" OPTIONAL)
    set(srcs "common.cpp" "${variant}/screen.cpp" ${variant_sources})
    set(includes "include" "${variant}")
    if (CONFIG_AIRGREP_BOARD_PLUS AND CONFIG_AIRGREP_DISPLAY STREQUAL "800x480-touch" AND CONFIG_LV_USE_BUILTIN_MALLOC)
        set(linker_fragments "${variant}/psram.lf")
    endif ()
endif ()
# The ViewState speaks of conflicts in the terms of the conflict engine, the daidalus component.
idf_component_register(SRCS ${srcs} INCLUDE_DIRS ${includes} PRIV_INCLUDE_DIRS "." REQUIRES daidalus PRIV_REQUIRES airgrep
        LDFRAGMENTS ${linker_fragments})
if (variant_sources)
    target_include_directories(${COMPONENT_LIB} PRIVATE ${variant_includes})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
endif ()
