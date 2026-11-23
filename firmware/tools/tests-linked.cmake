# Fails when the linker left a test object out: cmake -D map=FILE -D objects=A.obj;B.obj -P tests-linked.cmake
# The linker drops TEST_CASE registrations from an archive nobody references.
file(READ "${map}" linked)
set(dropped)
foreach (object ${objects})
    string(FIND "${linked}" "${object}" at)
    if (at EQUAL -1)
        list(APPEND dropped "${object}")
    endif ()
endforeach ()
if (dropped)
    list(JOIN dropped " " dropped)
    message(FATAL_ERROR "Dropped by the linker: ${dropped}. "
            "A test of the main component needs WHOLE_ARCHIVE, another one a test component.")
endif ()
