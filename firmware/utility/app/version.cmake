# Ignore tags so even a tagged release identifies the commit it was built from.
execute_process(COMMAND git describe --always "--exclude=*" --dirty
        OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if (revision)
    string(APPEND release "-g${revision}")
endif ()
file(CONFIGURE OUTPUT "${output}" CONTENT "let utilityVersion = \"${release}\"\n")
