cmake_minimum_required(VERSION 3.22)

project(airgrep VERSION 0.1.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
# Requirements are resolved in a script-mode pass that cannot see cache variables.
idf_build_set_property(AIRGREP_TEST "${AIRGREP_TEST}")
project(airgrep)
