cmake_minimum_required(VERSION 3.22)

project(airgrep VERSION 0.1.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
# Requirements are resolved in a script-mode pass that cannot see cache variables.
idf_build_set_property(AIRGREP_TEST "${AIRGREP_TEST}")
project(airgrep)

# Project components only. IDF compiles with -Werror and its own sources shadow.
idf_build_get_property(components BUILD_COMPONENTS)
foreach(component ${components})
    idf_component_get_property(dir ${component} COMPONENT_DIR)
    idf_component_get_property(lib ${component} COMPONENT_LIB)
    get_target_property(type ${lib} TYPE)
    if(dir MATCHES "^${CMAKE_SOURCE_DIR}/" AND NOT type STREQUAL "INTERFACE_LIBRARY")
        target_compile_options(${lib} PRIVATE -Wshadow)
    endif()
endforeach()
