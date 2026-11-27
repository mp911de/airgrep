# What is checked on a computer without the ESP-IDF build: the firmware's test files that carry a host
# main(), and the rules of the sources. utility/CMakeLists.txt takes this directory in, so its ctest
# runs these too. On its own, where the utility does not build:
#
#   cmake -S tests/host -B build/host -G Ninja && cmake --build build/host && ctest --test-dir build/host
cmake_minimum_required(VERSION 3.27)
project(airgrep-host LANGUAGES C CXX)
enable_testing()

get_filename_component(FIRMWARE "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
# The firmware build lints and formats these files.
set(CMAKE_C_CLANG_TIDY "")
set(CMAKE_CXX_CLANG_TIDY "")

# Unity comes with the ESP-IDF, which has to be on this computer, not activated, or via UNITY_DIR.
if (DEFINED ENV{UNITY_DIR})
    set(unity_dir "$ENV{UNITY_DIR}")
else ()
    if (DEFINED ENV{IDF_PATH})
        set(idf "$ENV{IDF_PATH}")
    else ()
        set(idf "$ENV{HOME}/.espressif/v6.1/esp-idf")
    endif ()
    set(unity_dir "${idf}/components/unity/unity/src")
endif ()
if (NOT EXISTS "${unity_dir}/unity.c")
    message(FATAL_ERROR "No Unity in ${unity_dir}. Set IDF_PATH to the ESP-IDF or UNITY_DIR to Unity's src.")
endif ()
add_library(unity STATIC "${unity_dir}/unity.c")
target_include_directories(unity PUBLIC "${unity_dir}")
set_target_properties(unity PROPERTIES C_STANDARD 11)

# A test file with a host main() behind #ifndef ESP_PLATFORM builds here. The source under test is the
# .c or .cpp beside each header the test includes. One that reaches for IDF headers has no host build.
set(idf_include "#include [<\"](esp_|freertos/|driver/|sdkconfig\\.h|hal/|soc/)")
file(GLOB tests CONFIGURE_DEPENDS
        "${FIRMWARE}/components/*/test/test_*.c" "${FIRMWARE}/components/*/test/test_*.cpp"
        "${FIRMWARE}/main/test/test_*.c" "${FIRMWARE}/main/test/test_*.cpp")
foreach (test ${tests})
    cmake_path(RELATIVE_PATH test BASE_DIRECTORY "${FIRMWARE}" OUTPUT_VARIABLE shown)
    file(READ "${test}" text)
    if (NOT text MATCHES "#ifndef ESP_PLATFORM")
        message(STATUS "No host run of ${shown}: it has no host main()")
        continue ()
    endif ()
    cmake_path(GET test PARENT_PATH test_dir)
    cmake_path(GET test_dir PARENT_PATH component)
    file(GLOB parts LIST_DIRECTORIES true "${component}/*")
    set(searched "${test_dir}" "${component}")
    foreach (part ${parts})
        if (IS_DIRECTORY "${part}")
            list(APPEND searched "${part}")
        endif ()
    endforeach ()
    set(sources "${test}")
    set(read "${test}")
    set(includes "${component}" "${test_dir}")
    file(STRINGS "${test}" lines REGEX "^#include \"[^\"]*\\.h(pp)?\"")
    foreach (line ${lines})
        string(REGEX REPLACE "^#include \"([^\"]*)\".*" "\\1" header "${line}")
        foreach (directory ${searched})
            if (EXISTS "${directory}/${header}")
                file(REAL_PATH "${directory}/${header}" found)
                cmake_path(GET found PARENT_PATH found_dir)
                cmake_path(REMOVE_EXTENSION found LAST_ONLY OUTPUT_VARIABLE beside)
                list(APPEND read "${found}")
                list(APPEND includes "${found_dir}")
                foreach (source "${beside}.c" "${beside}.cpp")
                    if (EXISTS "${source}")
                        list(APPEND sources "${source}")
                        list(APPEND read "${source}")
                    endif ()
                endforeach ()
                break ()
            endif ()
        endforeach ()
    endforeach ()
    set(needs_idf FALSE)
    foreach (file ${read})
        file(STRINGS "${file}" reached REGEX "${idf_include}")
        if (reached)
            set(needs_idf TRUE)
        endif ()
    endforeach ()
    if (needs_idf)
        message(STATUS "No host run of ${shown}: it needs IDF headers")
        continue ()
    endif ()
    cmake_path(GET test STEM name)
    string(REGEX REPLACE "^test_" "" name "${name}")
    list(REMOVE_DUPLICATES sources)
    list(REMOVE_DUPLICATES includes)
    add_executable(firmware_${name} ${sources})
    target_include_directories(firmware_${name} PRIVATE ${includes})
    target_compile_options(firmware_${name} PRIVATE -Wall -Wshadow)
    target_link_libraries(firmware_${name} PRIVATE unity)
    set_target_properties(firmware_${name} PROPERTIES C_STANDARD 11 CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
    add_test(NAME firmware.${name} COMMAND firmware_${name})
    set_tests_properties(firmware.${name} PROPERTIES LABELS firmware)
endforeach ()
# These two read the settings of a display.
foreach (name settings_store usb_commands)
    target_include_directories(firmware_${name} PRIVATE
            "${FIRMWARE}/components/display/include" "${FIRMWARE}/components/display/variants/800x480-touch"
            "${FIRMWARE}/components/daidalus")
endforeach ()

# Encounters in single precision when tests/host is the top-level build (Linux).
if (NOT TARGET daidalus AND EXISTS "${FIRMWARE}/components/daidalus/tests/CMakeLists.txt")
    set(DAIDALUS_FILE_IO OFF)
    set(DAIDALUS_BUILD_EXAMPLES OFF)
    set(DAIDALUS_BUILD_TESTS ON)
    set(DAIDALUS_SINGLE_PRECISION ON)
    add_subdirectory("${FIRMWARE}/components/daidalus" daidalus)
endif ()

# Every tag the test build runs at boot names at least one TEST_CASE.
add_test(NAME firmware.tags COMMAND "${CMAKE_COMMAND}" "-DFIRMWARE=${FIRMWARE}" -P "${CMAKE_CURRENT_LIST_DIR}/tags.cmake")
# No hand-written source includes "../".
add_test(NAME firmware.includes
        COMMAND "${CMAKE_COMMAND}" "-DFIRMWARE=${FIRMWARE}" -P "${CMAKE_CURRENT_LIST_DIR}/includes.cmake")
set_tests_properties(firmware.tags firmware.includes PROPERTIES LABELS firmware)

# Ruff on the Python tools and ShellCheck on the shell scripts. clang-tidy is not here: the builds run it.
set(not_ours "/(build|daidalus|managed_components|venv)/")
file(GLOB_RECURSE scripts CONFIGURE_DEPENDS
        "${FIRMWARE}/main/*.sh" "${FIRMWARE}/components/*.sh" "${FIRMWARE}/tools/*.sh" "${FIRMWARE}/utility/*.sh"
        "${FIRMWARE}/tzdb/*.sh")
list(FILTER scripts EXCLUDE REGEX "${not_ours}")
add_test(NAME lint.ruff
        COMMAND ruff check --no-cache --select F --extend-exclude "build,managed_components,cmake-build-*,venv"
                main components tools utility tzdb
        WORKING_DIRECTORY "${FIRMWARE}")
add_test(NAME lint.shellcheck COMMAND shellcheck ${scripts})
set_tests_properties(lint.ruff lint.shellcheck PROPERTIES LABELS lint)

# A README names files. One that names what is not there is no longer kept.
add_test(NAME docs.paths COMMAND "${FIRMWARE}/../tools/check-docs.py")
set_tests_properties(docs.paths PROPERTIES LABELS docs)
