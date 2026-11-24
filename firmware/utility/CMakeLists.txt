# What runs on a computer: the airgrep Utility application and the C++ tools that talk to a device.
# Needs Ninja, the Xcode Command Line Tools and, from Homebrew, libusb, clang-format and llvm. No IDF.
#
#   cmake -S utility -B utility/build -G Ninja && cmake --build utility/build
#
# This file is the one build.
cmake_minimum_required(VERSION 3.27)
# Homebrew builds its libraries for the macOS they are installed on, so the application runs on nothing
# older.
if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    execute_process(COMMAND sw_vers -productVersion OUTPUT_VARIABLE macos OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX REPLACE "^([0-9]+).*" "\\1.0" macos "${macos}")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${macos}" CACHE STRING "The oldest macOS the build runs on" FORCE)
endif ()
set(version_file "${CMAKE_CURRENT_SOURCE_DIR}/../version.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${version_file}")
file(STRINGS "${version_file}" release)
project(airgrep-utility VERSION "${release}" LANGUAGES C CXX OBJCXX Swift)

# The user interface is drawn in software, so it is built for speed unless asked otherwise.
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif ()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# For clangd, see ../.clangd.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(libusb REQUIRED IMPORTED_TARGET libusb-1.0)

get_filename_component(FIRMWARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(DISPLAY_DIR "${FIRMWARE_DIR}/components/display")
set(AIRGREP_DIR "${FIRMWARE_DIR}/components/airgrep")
set(warnings -Wall -Wextra -Wshadow -Wshorten-64-to-32 -Werror)
# The Editor's templates declare what a file may not use, and carry a number through a pointer. Its
# files are held to every other warning.
set(generated_warnings -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-function
        -Wno-shorten-64-to-32)
# What the LVGL Editor and the font tool write. Neither the format check nor clang-tidy reads it.
set(not_hand_written "_gen\\.[ch]$|/images/[^/]+_data\\.c$|/ui/fonts/")
# The application carries the libraries it loads.
set(APP "airgrep Utility")
set(APP_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(APP_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/${APP}.app/Contents/Frameworks")

# The firmware's component manifest names the LVGL version for both builds. A tag can move, so the
# commit of that version is fetched, and LVGL says which version it is.
file(STRINGS "${DISPLAY_DIR}/idf_component.yml" lvgl_version REGEX "version:")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" lvgl_version "${lvgl_version}")
include(FetchContent)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_TAG 80ca777e37a2b176770726a02e07a6fb79ef0b39)
FetchContent_MakeAvailable(lvgl)
if (NOT "$ENV{LVGL_VERSION}" STREQUAL "${lvgl_version}")
    message(FATAL_ERROR "components/display/idf_component.yml names LVGL ${lvgl_version}, the commit that "
            "utility/CMakeLists.txt fetches is LVGL $ENV{LVGL_VERSION}. Set GIT_TAG to the commit of tag "
            "v${lvgl_version}: git ls-remote https://github.com/lvgl/lvgl.git v${lvgl_version}")
endif ()
# A Debug build is for stepping through the application. LVGL draws in software and stays fast.
target_compile_options(lvgl PRIVATE $<$<CONFIG:Debug>:-O2>)

# The conflict engine of the display: NASA's DAIDALUS, the firmware component, as the firmware builds it without files.
set(DAIDALUS_FILE_IO OFF)
set(DAIDALUS_BUILD_EXAMPLES OFF)
set(DAIDALUS_BUILD_TESTS ON)
add_subdirectory("${FIRMWARE_DIR}/components/daidalus" daidalus EXCLUDE_FROM_ALL)
target_compile_options(daidalus PRIVATE $<$<CONFIG:Debug>:-O2>)

# Every target from here on is linted: clang-tidy runs with ../.clang-tidy beside each compilation, and
# a finding stops the build. LVGL and DAIDALUS are defined above and are not linted. Swift has no
# clang-tidy. Homebrew's llvm comes first: the clang-tidy of the IDF does not know macOS.
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)
if (NOT CLANG_TIDY_EXECUTABLE)
    message(FATAL_ERROR "clang-tidy not found. Install it with: brew install llvm")
endif ()
# With -p it reads each command from the compilation database, and so the headers of the compiler that
# builds. Without, it takes the C++ library of its own llvm.
set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}" -p "${CMAKE_BINARY_DIR}")
set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}" -p "${CMAKE_BINARY_DIR}")
set(CMAKE_OBJCXX_CLANG_TIDY "${CLANG_TIDY_EXECUTABLE}" -p "${CMAKE_BINARY_DIR}")

add_library(conflict STATIC "${FIRMWARE_DIR}/components/daidalus/conflicts/engine.cpp"
        "${FIRMWARE_DIR}/components/daidalus/conflicts/traffic_filter.cpp")
target_include_directories(conflict PUBLIC "${FIRMWARE_DIR}/components/daidalus" PRIVATE "${AIRGREP_DIR}")
target_link_libraries(conflict PRIVATE daidalus::daidalus)
# NASA's headers are not held to the project's warnings.
set_target_properties(daidalus PROPERTIES SYSTEM TRUE)
target_compile_options(conflict PRIVATE ${warnings})

# The hardware free part of the firmware a host shares with it: the States as the device holds them,
# the protocol that carries them and the ViewState a display shows of them.
add_library(firmware STATIC
        "${FIRMWARE_DIR}/main/daidalus/detection.cpp" "${FIRMWARE_DIR}/main/display/display_mapping.cpp"
        "${FIRMWARE_DIR}/main/daidalus/monitor.cpp"
        "${FIRMWARE_DIR}/main/usb/usb_protocol.cpp"
        "${FIRMWARE_DIR}/main/usb/usb_commands.cpp"
        "${AIRGREP_DIR}/settings/settings_model.cpp"
        "${AIRGREP_DIR}/system/health.cpp"
        "${AIRGREP_DIR}/system/power_mode.c"
        "${AIRGREP_DIR}/navigation/great_circle.cpp"
        "${AIRGREP_DIR}/traffic/traffic_protocol.cpp"
        "${AIRGREP_DIR}/traffic/traffic_table.cpp"
        "${AIRGREP_DIR}/tz/tz_lookup.c")
target_include_directories(firmware PUBLIC
        "${FIRMWARE_DIR}/main" "${DISPLAY_DIR}/include" "${DISPLAY_DIR}/variants" "${AIRGREP_DIR}"
        "${AIRGREP_DIR}/adsbee" "${AIRGREP_DIR}/baro" "${AIRGREP_DIR}/battery" "${AIRGREP_DIR}/charger"
        "${AIRGREP_DIR}/i2c_expander" "${AIRGREP_DIR}/system" "${AIRGREP_DIR}/traffic" "${AIRGREP_DIR}/usb"
        "${AIRGREP_DIR}/wifi")
target_compile_options(firmware PRIVATE ${warnings})
target_link_libraries(firmware PUBLIC conflict)

# Everything that talks to a device, over USB or to an emulated one that the firmware's protocol code
# serves. One library, so the application and the displays it loads share one conversation.
add_library(core SHARED
        device/session.cpp
        device/usb.cpp
        device/device_state.cpp
        device/emulator.cpp
        device/maneuvers.cpp
        device/inspect.cpp
        device/dump.cpp
        device/fixtures.cpp
        remote/connection.cpp
        remote/conversation.cpp
        remote/loop.cpp
        remote/conflicts.cpp
        remote/zones.cpp
        app/airgrep.cpp)
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(core PUBLIC firmware PRIVATE PkgConfig::libusb)
target_compile_options(core PRIVATE ${warnings})
set_target_properties(core PROPERTIES
        OUTPUT_NAME airgrep-core
        LIBRARY_OUTPUT_DIRECTORY "${APP_LIBRARIES}")

# Each display variant as a library the airgrep Utility loads. All share the LVGL build.
function(read_notice name path)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${path}")
    file(READ "${path}" text)
    set(${name} "${text}" PARENT_SCOPE)
endfunction()

set(notices "${CMAKE_CURRENT_SOURCE_DIR}/remote/notices")
read_notice(LVGL_NOTICE "${lvgl_SOURCE_DIR}/LICENCE.txt")
read_notice(APACHE_NOTICE "${notices}/Apache-2.0.txt")
read_notice(FREERTOS_NOTICE "${notices}/FreeRTOS-MIT.txt")
read_notice(TINYUSB_NOTICE "${notices}/TinyUSB-MIT.txt")
read_notice(DAIDALUS_NOTICE "${notices}/DAIDALUS2-NOSA.txt")
configure_file(remote/licenses.hpp.in licenses.hpp @ONLY)

# The directories of components/display/variants.
set(variants 800x480-touch seeed-172x320-touch)
set(displays)
foreach (variant ${variants})
    set(variant_sources)
    set(variant_includes)
    include("${DISPLAY_DIR}/variants/${variant}/sources.cmake" OPTIONAL)
    # The host runs the generated C widgets with LVGL's XML interpreter disabled.
    list(FILTER variant_sources EXCLUDE REGEX "_xml_parser\\.c$")
    add_library(${variant}-objects OBJECT
            remote/view.cpp
            "${DISPLAY_DIR}/common.cpp"
            "${DISPLAY_DIR}/variants/${variant}/screen.cpp"
            ${variant_sources})
    target_include_directories(${variant}-objects PUBLIC
            "${DISPLAY_DIR}" "${DISPLAY_DIR}/include" "${DISPLAY_DIR}/variants/${variant}" ${variant_includes})
    target_include_directories(${variant}-objects PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_definitions(${variant}-objects PUBLIC AIRGREP_DISPLAY="${variant}" LV_LVGL_H_INCLUDE_SIMPLE)
    target_compile_options(${variant}-objects PRIVATE ${warnings})
    set(generated ${variant_sources})
    list(FILTER generated INCLUDE REGEX "_gen\\.c$")
    set_source_files_properties(${generated} PROPERTIES COMPILE_OPTIONS "${generated_warnings}")
    set(generated ${variant_sources})
    list(FILTER generated INCLUDE REGEX "${not_hand_written}")
    set_source_files_properties(${generated} PROPERTIES SKIP_LINTING ON)
    target_link_libraries(${variant}-objects PUBLIC core lvgl)

    add_library(display-${variant} MODULE app/display.cpp)
    target_compile_options(display-${variant} PRIVATE ${warnings})
    target_link_libraries(display-${variant} PRIVATE ${variant}-objects)
    target_link_options(display-${variant} PRIVATE LINKER:-exported_symbol,_airgrep_display_get_api)
    set_target_properties(display-${variant} PROPERTIES
            PREFIX "" SUFFIX ".dylib"
            LIBRARY_OUTPUT_DIRECTORY "${APP_LIBRARIES}")
    list(APPEND displays display-${variant})
endforeach ()

# The application: SwiftUI around the libraries above. It carries its icon, its credits and the
# partition table of the firmware it was built with, which says where an image is written.
# CMake copies no resources into the bundle of a Swift executable, so each has its own rule.
set(contents_dir "${CMAKE_CURRENT_BINARY_DIR}/${APP}.app/Contents")
set(resources_dir "${contents_dir}/Resources")
set(utility_bundle_id biz.paluch.airgrep.utility)
configure_file(app/Info.plist.in Info.plist)
set(icon "${resources_dir}/AppIcon.icns")
set(logo "${FIRMWARE_DIR}/../assets/logo-1280.png")
add_custom_command(OUTPUT "${icon}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${resources_dir}"
        COMMAND xcrun swift "${CMAKE_CURRENT_SOURCE_DIR}/app/icon.swift" "${logo}" "${icon}"
        DEPENDS app/icon.swift "${logo}"
        VERBATIM)
set(resources "${icon}")
# Ninja needs a rule to restore the plist when the bundle is rebuilt.
add_custom_command(OUTPUT "${contents_dir}/Info.plist"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" "${contents_dir}/Info.plist"
        DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        VERBATIM)
list(APPEND resources "${contents_dir}/Info.plist")
set(copied "${CMAKE_CURRENT_SOURCE_DIR}/app/Credits.rtf" "${FIRMWARE_DIR}/partitions.csv")
# The time zone table of a device, built in ../tzdb. Without it a Remote Display runs on UTC and
# lists no zones, as a device without the table does.
if (EXISTS "${FIRMWARE_DIR}/tzdb/build/tzdata.bin")
    list(APPEND copied "${FIRMWARE_DIR}/tzdb/build/tzdata.bin")
endif ()
foreach (resource ${copied})
    get_filename_component(name "${resource}" NAME)
    add_custom_command(OUTPUT "${resources_dir}/${name}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${resource}" "${resources_dir}/${name}"
            DEPENDS "${resource}"
            VERBATIM)
    list(APPEND resources "${resources_dir}/${name}")
endforeach ()
add_custom_target(resources DEPENDS ${resources})
# Swift 6 checks for data races.
set(CMAKE_Swift_LANGUAGE_VERSION 6)
# What an install and the console are made of, free of the user interface, so a test builds it too.
set(install_sources app/FlashUtil.swift app/SerialPort.swift app/TerminalText.swift)
# Rules of what the application shows and saves, tested the same way.
set(readout_sources app/Usage.swift app/Export.swift)
set(version_source "${CMAKE_CURRENT_BINARY_DIR}/Version.swift")
add_custom_target(utility-version
        COMMAND "${CMAKE_COMMAND}" "-Drelease=${PROJECT_VERSION}" "-Doutput=${version_source}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/app/version.cmake"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        BYPRODUCTS "${version_source}"
        VERBATIM)
add_executable(airgrep-utility MACOSX_BUNDLE
        "${version_source}"
        app/App.swift
        app/Devices.swift
        app/Session.swift
        app/DeviceView.swift
        app/RemoteDisplay.swift
        app/DataWindow.swift
        app/Console.swift
        app/Buzzer.swift
        app/Maintenance.swift
        app/Installation.swift
        app/Presence.swift
        app/EmulatedConsole.swift
        app/Artwork.swift
        app/DockStatus.swift
        app/DockRipple.swift
        ${readout_sources}
        ${install_sources})
target_compile_options(airgrep-utility PRIVATE -parse-as-library)
target_compile_options(airgrep-utility PRIVATE -import-objc-header "${CMAKE_CURRENT_SOURCE_DIR}/app/bridge.h")
target_link_libraries(airgrep-utility PRIVATE core)
# The application loads the displays by name, so nothing but this says it needs them.
add_dependencies(airgrep-utility ${displays} resources utility-version)
set_target_properties(airgrep-utility PROPERTIES
        OUTPUT_NAME "${APP}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${utility_bundle_id}"
        RUNTIME_OUTPUT_DIRECTORY "${APP_DIR}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        BUILD_RPATH "@executable_path/../Frameworks")

# The bundle is signed as a whole once all its parts are in it. macOS refuses a bundle whose
# signature covers the executable alone, and one that holds what the Swift compiler leaves beside
# an executable it builds.
# Sign each library before its dependents link against it.
foreach (library core ${displays})
    add_custom_command(TARGET ${library} POST_BUILD
            COMMAND codesign --force --sign - "$<TARGET_FILE:${library}>" VERBATIM)
endforeach ()
set(signature_files "${contents_dir}/_CodeSignature/CodeResources")
set(signed "${CMAKE_CURRENT_BINARY_DIR}/app.signed")
set(executable "$<TARGET_FILE:airgrep-utility>")
add_custom_command(OUTPUT "${signed}"
        BYPRODUCTS ${signature_files}
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "$<TARGET_FILE_DIR:airgrep-utility>/main.d" "${executable}.dSYM"
        COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:airgrep-utility>"
        COMMAND codesign --verify --deep --strict
                "-R=identifier \"${utility_bundle_id}\" and info[CFBundleIconFile] = \"AppIcon\""
                "$<TARGET_BUNDLE_DIR:airgrep-utility>"
        COMMAND "${CMAKE_COMMAND}" -E touch "${signed}"
        DEPENDS airgrep-utility core ${displays} ${resources} "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        VERBATIM)
add_custom_target(app ALL DEPENDS "${signed}")

enable_testing()
# The firmware's host tests and the rules of the sources, which need no utility.
add_subdirectory("${FIRMWARE_DIR}/tests/host" host)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME usb_layout_python COMMAND "${Python3_EXECUTABLE}" "${FIRMWARE_DIR}/tools/test_usb_layout.py")
foreach (test display_mapping device usb_layout remote remote_conflicts flight)
    add_executable(test_${test} test/test_${test}.cpp)
    target_compile_options(test_${test} PRIVATE ${warnings})
    add_test(NAME ${test} COMMAND test_${test})
endforeach ()
set_tests_properties(remote_conflicts PROPERTIES TIMEOUT 30)
# An install and the console, against a ROM bootloader and a line editor written in the test.
add_executable(test_install test/test_install.swift test/test_image.swift test/test_presence.swift test/test_readout.swift
        app/Maintenance.swift app/Presence.swift ${readout_sources} ${install_sources})
target_compile_options(test_install PRIVATE -parse-as-library)
add_test(NAME install COMMAND test_install)
# The mapping is firmware code: its test needs nothing of a host but a compiler.
target_link_libraries(test_display_mapping PRIVATE firmware)
target_link_libraries(test_device PRIVATE core)
target_link_libraries(test_usb_layout PRIVATE core)
target_link_libraries(test_remote PRIVATE core)
target_link_libraries(test_remote_conflicts PRIVATE core)
target_link_libraries(test_flight PRIVATE core)
# Draws what a display shows of a fixture into a PNG file. A driver compares it.
add_executable(test_render test/render.cpp test/png.cpp)
target_compile_options(test_render PRIVATE ${warnings})
target_link_libraries(test_render PRIVATE core ZLIB::ZLIB)
add_dependencies(test_render ${displays})
# The display of a fixture device: every designed screen against its image, and what gestures do.
add_test(NAME screens COMMAND "${FIRMWARE_DIR}/tools/check-screens.py")
add_test(NAME behaviour COMMAND "${FIRMWARE_DIR}/tools/check-behaviour.py")
set_tests_properties(screens behaviour PROPERTIES LABELS display)
# The application's side of the C interface, against the display library of one variant.
add_executable(test_app test/test_app.cpp)
target_compile_options(test_app PRIVATE ${warnings})
target_link_libraries(test_app PRIVATE core)
add_dependencies(test_app ${displays})
add_test(NAME app COMMAND test_app "$<TARGET_FILE:display-800x480-touch>")
foreach (display IN LISTS displays)
    add_test(NAME abi-${display} COMMAND test_app "$<TARGET_FILE:${display}>" --abi)
endforeach ()
# No object takes a touch from the target it lies in. The objects of a variant are linked in: a display library
# keeps its object tree to itself.
foreach (variant ${variants})
    add_executable(test_touch_${variant} test/test_touch.cpp app/display.cpp)
    target_compile_options(test_touch_${variant} PRIVATE ${warnings})
    target_link_libraries(test_touch_${variant} PRIVATE ${variant}-objects)
    add_test(NAME touch-${variant} COMMAND test_touch_${variant})
endforeach ()
# Where the radar puts tails and dots: pixels only, so it needs no LVGL.
foreach (part track sections bands labels grow)
    add_executable(test_radar_${part} test/test_radar_${part}.cpp "${DISPLAY_DIR}/ui/widgets/radar/radar_${part}.c")
    target_include_directories(test_radar_${part} PRIVATE "${DISPLAY_DIR}/ui" "${AIRGREP_DIR}")
    target_compile_options(test_radar_${part} PRIVATE ${warnings})
    add_test(NAME radar_${part} COMMAND test_radar_${part})
endforeach ()
# Which traffic comes too close: the engine alone, it needs neither LVGL nor a device.
add_executable(test_conflict test/test_conflict.cpp)
target_include_directories(test_conflict PRIVATE "${AIRGREP_DIR}")
target_compile_options(test_conflict PRIVATE ${warnings})
target_link_libraries(test_conflict PRIVATE conflict)
add_test(NAME conflict COMMAND test_conflict)
# The traffic the device's benchmark flies, and the benchmark itself: with any argument it prints
# the numbers of this computer.
add_executable(test_scenario test/test_scenario.cpp
        "${FIRMWARE_DIR}/components/hwtest/scenario.cpp" "${FIRMWARE_DIR}/components/hwtest/benchmark.cpp")
target_include_directories(test_scenario PRIVATE "${AIRGREP_DIR}" "${FIRMWARE_DIR}/components/hwtest/include")
target_compile_options(test_scenario PRIVATE ${warnings})
target_link_libraries(test_scenario PRIVATE conflict)
add_test(NAME scenario COMMAND test_scenario)
# The rounds of the conflict engine as the firmware runs them, against the benchmark under the same sky.
add_executable(test_conflict_monitor test/test_conflict_monitor.cpp
        "${FIRMWARE_DIR}/components/hwtest/scenario.cpp" "${FIRMWARE_DIR}/components/hwtest/benchmark.cpp")
target_include_directories(test_conflict_monitor PRIVATE "${FIRMWARE_DIR}/components/hwtest/include")
target_compile_options(test_conflict_monitor PRIVATE ${warnings})
target_link_libraries(test_conflict_monitor PRIVATE firmware)
add_test(NAME conflict_monitor COMMAND test_conflict_monitor)

# ctest -L picks a group: display, device and what tests/host/ names.
set(display_tests display_mapping app scenario conflict conflict_monitor)
foreach (display IN LISTS displays)
    list(APPEND display_tests abi-${display})
endforeach ()
foreach (variant ${variants})
    list(APPEND display_tests touch-${variant})
endforeach ()
foreach (part track sections bands labels grow)
    list(APPEND display_tests radar_${part})
endforeach ()
set_tests_properties(${display_tests} PROPERTIES LABELS display)
set_tests_properties(usb_layout_python usb_layout device remote remote_conflicts flight install PROPERTIES LABELS device)

# Formatting gates the build: every target waits for the check, LVGL and DAIDALUS included. It reads
# the C and C++ sources that the targets above name, so what is compiled is what is checked, and the
# headers of utility/, which no target names. The files of the Editor and of the build keep their
# own form, and so does DAIDALUS.
find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format REQUIRED)
file(GLOB format_sources CONFIGURE_DEPENDS "*.h" "*.hpp" "app/*.h" "app/*.hpp" "device/*.hpp" "remote/*.hpp" "test/*.hpp")
get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach (target ${targets})
    get_target_property(sources ${target} SOURCES)
    if (sources)
        list(APPEND format_sources ${sources})
    endif ()
endforeach ()
set(format_checked)
foreach (source ${format_sources})
    cmake_path(ABSOLUTE_PATH source NORMALIZE)
    cmake_path(IS_PREFIX CMAKE_CURRENT_BINARY_DIR "${source}" built)
    if (source MATCHES "\\.(c|cpp|h|hpp)$" AND NOT built AND NOT source MATCHES "${not_hand_written}")
        list(APPEND format_checked "${source}")
    endif ()
endforeach ()
list(REMOVE_DUPLICATES format_checked)
add_custom_target(format-check
        COMMAND "${CLANG_FORMAT_EXECUTABLE}" --style=file --dry-run --Werror ${format_checked}
        WORKING_DIRECTORY "${FIRMWARE_DIR}"
        COMMENT "Checking C/C++ formatting"
        VERBATIM)
function(wait_for_format_check directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach (target ${targets})
        get_target_property(type ${target} TYPE)
        if (NOT type STREQUAL "INTERFACE_LIBRARY" AND NOT target STREQUAL "format-check")
            add_dependencies(${target} format-check)
        endif ()
    endforeach ()
    get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach (subdirectory ${subdirectories})
        wait_for_format_check("${subdirectory}")
    endforeach ()
endfunction()
wait_for_format_check("${CMAKE_CURRENT_SOURCE_DIR}")
