cmake_minimum_required(VERSION 3.22)

set(version_file "${CMAKE_CURRENT_SOURCE_DIR}/version.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${version_file}")
file(STRINGS "${version_file}" AIRGREP_VERSION)
# The app version carries the release and the commit: 0.1.0-g7e77b6b-dirty. The IDF reads it
# at configure time, so a new commit shows up after the next reconfigure. 32 characters max.
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE AIRGREP_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git status --porcelain --untracked-files=no WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE AIRGREP_CHANGES ERROR_QUIET)
set(PROJECT_VER "${AIRGREP_VERSION}")
if(AIRGREP_COMMIT)
    string(APPEND PROJECT_VER "-g${AIRGREP_COMMIT}")
endif()
if(AIRGREP_CHANGES)
    string(APPEND PROJECT_VER "-dirty")
endif()
if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "Activate ESP-IDF first: source ~/.espressif/tools/activate_idf_v6.1.sh")
endif()
# Each preset mixin names the fragment of one axis, so mixins combine. A later fragment wins.
set(SDKCONFIG_DEFAULTS sdkconfig.defaults ${AIRGREP_BOARD_CONFIG} ${AIRGREP_DISPLAY_CONFIG}
        ${AIRGREP_MODE_CONFIG} ${AIRGREP_CONSOLE_CONFIG} ${AIRGREP_SERVICE_CONFIG} ${AIRGREP_DEBUG_CONFIG})
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if(IDF_TARGET STREQUAL "esp32p4")
    # Component Manager records the resolved target in its lockfile.
    configure_file(dependencies.lock "${CMAKE_BINARY_DIR}/dependencies.lock" COPYONLY)
    idf_build_set_property(DEPENDENCIES_LOCK "${CMAKE_BINARY_DIR}/dependencies.lock")
endif()
idf_build_set_property(MINIMAL_BUILD ON)
# Requirements are resolved in a script-mode pass that cannot see cache variables.
idf_build_set_property(AIRGREP_TEST "${AIRGREP_TEST}")
project(airgrep)

if(CONFIG_AIRGREP_DISPLAY STREQUAL "800x480-touch")
    idf_component_get_property(lvgl_dir lvgl__lvgl COMPONENT_DIR)
    idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
    include(tools/lvgl-glyphs.cmake)
    airgrep_lvgl_glyphs(${lvgl_lib} "${lvgl_dir}")
    if(CONFIG_LV_USE_PPA)
        # LVGL's private draw headers expose the PPA driver and cache API to widgets.
        idf_component_get_property(ppa_lib esp_driver_ppa COMPONENT_LIB)
        idf_component_get_property(mm_lib esp_mm COMPONENT_LIB)
        target_link_libraries(${lvgl_lib} INTERFACE ${ppa_lib} ${mm_lib})
    endif()
endif()

if(CONFIG_IDF_TARGET_ESP32S3 AND CONFIG_AIRGREP_DISPLAY STREQUAL "800x480-touch")
    idf_component_get_property(lvgl_dir lvgl__lvgl COMPONENT_DIR)
    idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
    # LVGL renders full circles through its border rasterizer. Both keep the original arc task.
    set_property(SOURCE "${lvgl_dir}/src/draw/sw/lv_draw_sw_arc.c"
            "${lvgl_dir}/src/draw/sw/lv_draw_sw_border.c" TARGET_DIRECTORY ${lvgl_lib}
            APPEND PROPERTY COMPILE_DEFINITIONS lv_draw_sw_blend=radar_wedge_blend)
endif()

if(CONFIG_AIRGREP_PROFILER AND CONFIG_AIRGREP_BOARD_PLUS AND CONFIG_AIRGREP_DISPLAY STREQUAL "800x480-touch")
    idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
    set(draw_profiler_dir "${CMAKE_CURRENT_SOURCE_DIR}/components/hwtest")
    set(draw_profiler_defs LV_USE_PROFILER=1 LV_PROFILER_DRAW=1
            LV_PROFILER_INCLUDE="render_profile_hooks.h")
    target_include_directories(${lvgl_lib} PRIVATE "${draw_profiler_dir}")
    target_compile_definitions(${lvgl_lib} PRIVATE ${draw_profiler_defs})
    idf_component_get_property(display_lib display COMPONENT_LIB)
    set_source_files_properties(components/display/ui/widgets/radar/radar_wedge_blend.c
            TARGET_DIRECTORY ${display_lib} PROPERTIES INCLUDE_DIRECTORIES "${draw_profiler_dir}"
            COMPILE_DEFINITIONS "${draw_profiler_defs}")
endif()

# The variant is the build directory name, the preset's binaryDir: s3+800x480-touch. Each build also
# leaves its image in build/images, named for the variant and the version the image reports.
get_filename_component(AIRGREP_VARIANT "${CMAKE_BINARY_DIR}" NAME)
idf_component_get_property(airgrep_lib airgrep COMPONENT_LIB)
target_compile_definitions(${airgrep_lib} PRIVATE "AIRGREP_VARIANT=\"${AIRGREP_VARIANT}\"")
set(AIRGREP_IMAGES "${CMAKE_SOURCE_DIR}/build/images")
add_custom_command(TARGET app POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${AIRGREP_IMAGES}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin"
                "${AIRGREP_IMAGES}/${CMAKE_PROJECT_NAME}-${AIRGREP_VARIANT}-${PROJECT_VER}.bin"
        VERBATIM)

# The Nano flashes through its UART bridge. The S3 service boots and flashes over native USB.
idf_build_get_property(python PYTHON)
if(CONFIG_AIRGREP_BOARD_P4_NANO)
    add_custom_target(flash-device
            COMMAND "${python}" "$ENV{IDF_PATH}/tools/idf.py" -B "${CMAKE_BINARY_DIR}" monitor
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            DEPENDS flash USES_TERMINAL VERBATIM)
else()
    add_custom_target(flash-device
            COMMAND "${python}" "${CMAKE_SOURCE_DIR}/tools/airgrep.py" flash "${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin"
            USES_TERMINAL VERBATIM)
    add_dependencies(flash-device app)
endif()

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format REQUIRED)
file(GLOB_RECURSE AIRGREP_FORMAT_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/main/*.c"
        "${CMAKE_SOURCE_DIR}/main/*.cpp"
        "${CMAKE_SOURCE_DIR}/main/*.h"
        "${CMAKE_SOURCE_DIR}/main/*.hpp"
        "${CMAKE_SOURCE_DIR}/components/*.c"
        "${CMAKE_SOURCE_DIR}/components/*.cpp"
        "${CMAKE_SOURCE_DIR}/components/*.h"
        "${CMAKE_SOURCE_DIR}/components/*.hpp")
add_custom_target(format-check
        COMMAND "${CLANG_FORMAT_EXECUTABLE}" --style=file --dry-run --Werror ${AIRGREP_FORMAT_SOURCES}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Checking C/C++ formatting"
        VERBATIM)

# Formatting gates the build: nothing compiles before the check has passed, the IDF's components and
# the bootloader, a build of its own, included.
function(wait_for_format_check directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target ${targets})
        get_target_property(type ${target} TYPE)
        if(type MATCHES "^(STATIC_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$" OR target STREQUAL "bootloader")
            add_dependencies(${target} format-check)
        endif()
    endforeach()
    get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(subdirectory ${subdirectories})
        wait_for_format_check("${subdirectory}")
    endforeach()
endfunction()
wait_for_format_check("${CMAKE_SOURCE_DIR}")

# The firmware builds with the clang toolchain, which the presets select, and runs clang-tidy with
# .clang-tidy on the project components. A finding stops the build. The clang-tidy beside the compiler
# knows its target.
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The firmware builds with the clang toolchain. Configure with a preset of CMakePresets.json.")
endif()
if(CMAKE_VERSION VERSION_LESS 3.27)
    message(FATAL_ERROR "The lint needs CMake 3.27, which keeps clang-tidy off the generated sources")
endif()
get_filename_component(compiler_directory "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy HINTS "${compiler_directory}" NO_DEFAULT_PATH)
if(NOT CLANG_TIDY_EXECUTABLE)
    message(FATAL_ERROR "No clang-tidy beside ${CMAKE_C_COMPILER}. Install the esp-clang of the ESP-IDF tools.")
endif()

# Project components only. IDF compiles with -Werror and its own sources shadow, and so does DAIDALUS:
# of the daidalus component only what is in its directory is held to them, NASA's sources are fetched.
# The managed components are in this directory and are not project code.
idf_build_get_property(components BUILD_COMPONENTS)
set(test_objects)
foreach(component ${components})
    idf_component_get_property(dir ${component} COMPONENT_DIR)
    idf_component_get_property(lib ${component} COMPONENT_LIB)
    get_target_property(type ${lib} TYPE)
    if(dir MATCHES "^${CMAKE_SOURCE_DIR}/(main$|components/)" AND NOT type STREQUAL "INTERFACE_LIBRARY")
        get_target_property(fetched ${lib} SOURCES)
        list(FILTER fetched INCLUDE REGEX "/_deps/")
        get_target_property(ours ${lib} SOURCES)
        list(FILTER ours EXCLUDE REGEX "/_deps/")
        set_source_files_properties(${fetched} TARGET_DIRECTORY ${lib} PROPERTIES SKIP_LINTING ON)
        set_source_files_properties(${ours} TARGET_DIRECTORY ${lib} PROPERTIES COMPILE_OPTIONS -Wshadow)
        set_target_properties(${lib} PROPERTIES
                C_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}" CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}")
        # What the LVGL Editor and the font tool generate is not linted.
        get_target_property(generated ${lib} SOURCES)
        list(FILTER generated INCLUDE REGEX "_gen\\.c$|/images/[^/]+_data\\.c$|/ui/fonts/")
        set_source_files_properties(${generated} TARGET_DIRECTORY ${lib} PROPERTIES SKIP_LINTING ON)
        get_target_property(tests ${lib} SOURCES)
        list(FILTER tests INCLUDE REGEX "/test_[^/]*\\.(c|cpp)$")
        foreach(test ${tests})
            get_filename_component(name "${test}" NAME)
            list(APPEND test_objects "${name}.obj")
        endforeach()
    endif()
endforeach()

# Every test a build compiles is in what it links.
if(test_objects)
    idf_build_get_property(elf EXECUTABLE)
    add_custom_command(TARGET ${elf} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -D "map=${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.map" -D "objects=${test_objects}"
                    -P "${CMAKE_SOURCE_DIR}/tools/tests-linked.cmake"
            VERBATIM)
endif()
