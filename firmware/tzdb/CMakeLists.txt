cmake_minimum_required(VERSION 3.22)
project(airgrep-tzdb LANGUAGES C)

function(table_setting name default description)
    if(DEFINED ENV{${name}})
        set(default "$ENV{${name}}")
    endif()
    set(${name} "${default}" CACHE STRING "${description}")
endfunction()
table_setting(RELEASE 2026f "Boundary release tag or latest")
table_setting(DATASET timezones "Boundary dataset")
table_setting(CELL 0.01 "Raster cell size in degrees")

# The pinned NumPy and pyproj have wheels for Python 3.9 through 3.12.
set(python_versions 3.9...<3.13)
if(NOT Python3_EXECUTABLE AND DEFINED ENV{PYTHON})
    find_program(Python3_EXECUTABLE NAMES "$ENV{PYTHON}" REQUIRED)
endif()
if(Python3_EXECUTABLE)
    set(python_versions 3.9)
endif()
find_package(Python3 ${python_versions} REQUIRED COMPONENTS Interpreter)
set(out "${CMAKE_CURRENT_BINARY_DIR}")
set(venv "${out}/venv")
set(python "${venv}/bin/python")
set(tools "${CMAKE_CURRENT_SOURCE_DIR}/tools")
set(requirements "${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt")
set(interpreter "${out}/CMakeFiles/tzdb-python.txt")
file(CONFIGURE OUTPUT "${interpreter}" CONTENT "${Python3_EXECUTABLE}\n${Python3_VERSION}\n")
add_custom_command(OUTPUT "${venv}/.ready"
        COMMAND "${Python3_EXECUTABLE}" -m venv --clear "${venv}"
        COMMAND "${python}" -m pip install --no-cache-dir -r "${requirements}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${venv}/.ready"
        DEPENDS "${requirements}" "${interpreter}"
        VERBATIM)
add_custom_target(setup DEPENDS "${venv}/.ready")

set(run_python "${CMAKE_COMMAND}" -E env "PYTHONPYCACHEPREFIX=${out}/pycache" "${python}")
set(source_options --release "${RELEASE}" --dataset "${DATASET}")
add_custom_target(fetch
        COMMAND ${run_python} "${tools}/source.py" ${source_options}
        VERBATIM)
add_dependencies(fetch setup)

# Use the same numeric spelling as the generator for inputs such as 0.010.
execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import sys; print(format(float(sys.argv[1]), 'g'))" "${CELL}"
        OUTPUT_VARIABLE step OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
set(raster "${out}/raster-${step}.bin")
set(rows "${out}/tz-${step}-rows.bin")
set(quadtree "${out}/tz-${step}-quadtree.bin")
set(table "${out}/tzdata.bin")
add_custom_target(tables
        COMMAND ${run_python} "${tools}/build_table.py" ${source_options} --cell "${CELL}" --output "${out}"
        BYPRODUCTS "${raster}" "${rows}" "${quadtree}" "${out}/tz-${step}.bin" "${table}" "${out}/sizes.json" "${out}/zones.json"
        VERBATIM)
add_dependencies(tables setup)
add_custom_target(tzdata ALL
        COMMAND "${CMAKE_COMMAND}" -E sha256sum "${table}"
        VERBATIM)
add_dependencies(tzdata tables)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(decoder "${CMAKE_CURRENT_SOURCE_DIR}/../components/airgrep/tz")
foreach(driver test_lookup test_lookup_sanitized)
    add_executable(${driver} decoder/test_lookup.c "${decoder}/tz_lookup.c")
    target_include_directories(${driver} PRIVATE "${decoder}")
    target_compile_options(${driver} PRIVATE -Wall -Wextra -Werror -pedantic)
endforeach()
target_compile_options(test_lookup PRIVATE -O3)
set_target_properties(test_lookup_sanitized PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_compile_options(test_lookup_sanitized PRIVATE -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
target_link_options(test_lookup_sanitized PRIVATE -fsanitize=address,undefined)

enable_testing()
add_test(NAME rows COMMAND test_lookup "${rows}" --check "${raster}")
add_test(NAME quadtree COMMAND test_lookup "${quadtree}" --check "${raster}")
add_custom_target(sanitize
        COMMAND test_lookup_sanitized "${table}" --check "${raster}"
        VERBATIM)
add_dependencies(sanitize tables test_lookup_sanitized)
