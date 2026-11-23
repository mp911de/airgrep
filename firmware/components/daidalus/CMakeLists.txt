cmake_minimum_required(VERSION 3.20)

function(fetch_daidalus)
    include(FetchContent)
    FetchContent_Declare(daidalus
        GIT_REPOSITORY https://github.com/mp911de/daidalus.git
        GIT_TAG c957b881b6dc842c2e4542bb8c301cde9f9f962c)
    FetchContent_MakeAvailable(daidalus)
    set(daidalus_root "${daidalus_SOURCE_DIR}" PARENT_SCOPE)
    set(daidalus_include "${daidalus_SOURCE_DIR}/C++/include" PARENT_SCOPE)
    include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/cmake/sources.cmake")
    if(DAIDALUS_FILE_IO)
        list(APPEND DAIDALUS_CORE_SOURCES ${DAIDALUS_IO_SOURCES})
    endif()
    set(daidalus_sources "${DAIDALUS_CORE_SOURCES}" PARENT_SCOPE)
endfunction()

# airgrep: `real` of C++/include/Real.h is float. No computation may then run in double unseen.
function(daidalus_single_precision target)
    target_compile_definitions(${target} PUBLIC DAIDALUS_SINGLE_PRECISION)
    target_compile_options(${target} PRIVATE -Werror=double-promotion -Wfloat-conversion)
endfunction()

if(ESP_PLATFORM)
    # airgrep: conflicts are computed to be shown. A device without a display computes none, and
    # for it this component is empty, as the display component is. Requirements cannot depend on
    # sdkconfig, so both registrations name them.
    if(NOT CONFIG_AIRGREP_DISPLAY)
        idf_component_register(PRIV_REQUIRES airgrep)
        return()
    endif()
    set(DAIDALUS_FILE_IO ${CONFIG_DAIDALUS_FILE_IO})
    fetch_daidalus()
    # airgrep: conflicts/ is the engine the firmware calls, conflicts/conflicts.hpp its interface.
    # NASA's sources stay behind it.
    idf_component_register(SRCS ${daidalus_sources} "conflicts/engine.cpp" "conflicts/traffic_filter.cpp"
            INCLUDE_DIRS "${daidalus_include}" "." PRIV_REQUIRES airgrep)
    set_property(TARGET ${COMPONENT_LIB} APPEND PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${daidalus_include}")
    if(NOT CONFIG_COMPILER_CXX_EXCEPTIONS)
        message(FATAL_ERROR "DAIDALUS requires CONFIG_COMPILER_CXX_EXCEPTIONS=y")
    endif()
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC DAIDALUS_FILE_IO=$<BOOL:${DAIDALUS_FILE_IO}>)
    target_compile_options(${COMPONENT_LIB} PRIVATE -fno-fast-math)
    if(CONFIG_DAIDALUS_SINGLE_PRECISION)
        daidalus_single_precision(${COMPONENT_LIB})
    endif()
    return()
endif()

project(daidalus VERSION 2.0.4 LANGUAGES CXX)
option(DAIDALUS_FILE_IO "Build configuration and encounter file support" ON)
option(DAIDALUS_BUILD_EXAMPLES "Build upstream command-line examples" ON)
option(DAIDALUS_BUILD_TESTS "Build native validation" ON)
option(DAIDALUS_SINGLE_PRECISION "Compute in float instead of double" OFF)

fetch_daidalus()
add_library(daidalus STATIC ${daidalus_sources})
add_library(daidalus::daidalus ALIAS daidalus)
target_include_directories(daidalus SYSTEM PUBLIC "${daidalus_include}")
target_compile_features(daidalus PUBLIC cxx_std_17)
target_compile_definitions(daidalus PUBLIC DAIDALUS_FILE_IO=$<BOOL:${DAIDALUS_FILE_IO}>)
target_compile_options(daidalus PRIVATE -fno-fast-math)
if(DAIDALUS_SINGLE_PRECISION)
    daidalus_single_precision(daidalus)
endif()
set_target_properties(daidalus PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(DAIDALUS_BUILD_EXAMPLES AND DAIDALUS_FILE_IO)
    foreach(example DaidalusExample DaidalusAlerting DaidalusBatch DAAEcho)
        add_executable(${example} "${daidalus_root}/C++/examples/${example}.cpp")
        target_link_libraries(${example} PRIVATE daidalus)
    endforeach()
endif()

if(DAIDALUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
