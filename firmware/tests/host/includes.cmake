# Fails when a hand-written source of the firmware includes "../". What the LVGL Editor writes on export
# is not hand-written.
file(GLOB_RECURSE sources
        "${FIRMWARE}/main/*.c" "${FIRMWARE}/main/*.cpp" "${FIRMWARE}/main/*.h" "${FIRMWARE}/main/*.hpp"
        "${FIRMWARE}/components/*.c" "${FIRMWARE}/components/*.cpp" "${FIRMWARE}/components/*.h"
        "${FIRMWARE}/components/*.hpp")
list(FILTER sources EXCLUDE REGEX "/components/display/ui/(.*/)?[^/]*_gen\\.[ch]$|/components/display/ui/images/[^/]*_data\\.c$")
set(found "")
foreach (source ${sources})
    file(STRINGS "${source}" lines REGEX "#include \"\\.\\./")
    if (lines)
        string(APPEND found "\n${source}: ${lines}")
    endif ()
endforeach ()
if (found)
    message(FATAL_ERROR "Includes of \"../\":${found}")
endif ()
