cmake_minimum_required(VERSION 3.22)

project(airgrep VERSION 0.1.0)
# The app version carries the release and the commit: 0.1.0-g7e77b6b-dirty. The IDF reads it
# at configure time, so a new commit shows up after the next reconfigure. 32 characters max.
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE AIRGREP_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git status --porcelain --untracked-files=no WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE AIRGREP_CHANGES ERROR_QUIET)
set(PROJECT_VER "${PROJECT_VERSION}")
if(AIRGREP_COMMIT)
    string(APPEND PROJECT_VER "-g${AIRGREP_COMMIT}")
endif()
if(AIRGREP_CHANGES)
    string(APPEND PROJECT_VER "-dirty")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
# Requirements are resolved in a script-mode pass that cannot see cache variables.
idf_build_set_property(AIRGREP_TEST "${AIRGREP_TEST}")
project(airgrep)

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

# Project components only. IDF compiles with -Werror and its own sources shadow.
idf_build_get_property(components BUILD_COMPONENTS)
foreach(component ${components})
    idf_component_get_property(dir ${component} COMPONENT_DIR)
    idf_component_get_property(lib ${component} COMPONENT_LIB)
    get_target_property(type ${lib} TYPE)
    if(dir MATCHES "^${CMAKE_SOURCE_DIR}/" AND NOT type STREQUAL "INTERFACE_LIBRARY")
        add_dependencies(${lib} format-check)
        target_compile_options(${lib} PRIVATE -Wshadow)
    endif()
endforeach()
