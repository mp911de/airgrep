# The LVGL Pro project in ui/ is the user interface of this variant.
set(LV_EDITOR_PROJECT_SOURCES)
include("${CMAKE_CURRENT_LIST_DIR}/../../ui/file_list_gen.cmake")
# What the Editor does not list: sources without an XML of their own.
include("${CMAKE_CURRENT_LIST_DIR}/../../ui/user_config.cmake")
# An image is built from its PNG, as globals.xml names it. The copies the Editor writes on export are not
# in the repo and not built.
list(FILTER LV_EDITOR_PROJECT_SOURCES EXCLUDE REGEX "/images/[^/]+_data\\.c$")
set(ui "${CMAKE_CURRENT_LIST_DIR}/../../ui")
include("${CMAKE_CURRENT_LIST_DIR}/../../../../tools/lvgl-glyphs.cmake")
foreach(source IN LISTS LV_EDITOR_PROJECT_SOURCES)
    if(source MATCHES "/fonts/([^/]+_data\\.c)$")
        set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/ui/fonts/${CMAKE_MATCH_1}")
        airgrep_glyph_source("${source}" "${output}")
        list(REMOVE_ITEM LV_EDITOR_PROJECT_SOURCES "${source}")
        list(APPEND LV_EDITOR_PROJECT_SOURCES "${output}")
    endif()
endforeach()
set(image_tool "${CMAKE_CURRENT_LIST_DIR}/../../../../tools/png2lvgl_c_data.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ui}/globals.xml")
file(STRINGS "${ui}/globals.xml" image_tags REGEX "<data name=")
set(images)
foreach (tag ${image_tags})
    if (NOT tag MATCHES "name=\"([^\"]+)\" src_path=\"([^\"]+)\" color_format=\"a8\"")
        message(FATAL_ERROR "ui/globals.xml has an image that tools/png2lvgl_c_data.py cannot make: ${tag}")
    endif ()
    set(image "${CMAKE_CURRENT_BINARY_DIR}/generated/images/${CMAKE_MATCH_1}_data.c")
    set(png "${ui}/${CMAKE_MATCH_2}")
    add_custom_command(OUTPUT "${image}"
            COMMAND python3 "${image_tool}" "${png}" "${image}"
            DEPENDS "${image_tool}" "${png}"
            VERBATIM)
    list(APPEND images "${image}")
endforeach ()
# The greetings of the startup screen come from the catalog in greetings.json.
set(greetings "${CMAKE_CURRENT_BINARY_DIR}/generated/greetings.hpp")
set(greetings_tool "${CMAKE_CURRENT_LIST_DIR}/../../../../tools/generate-greetings.py")
set(greetings_catalog "${CMAKE_CURRENT_LIST_DIR}/../../greetings.json")
# Room for a greeting in characters. The line is 736 px, the screen less 32 px at each side, and a
# character of font_data takes 16 px in running text. A callsign takes up to 8 characters of 30 px, the widest.
set(greeting_longest 46)
set(greeting_longest_named 31)
add_custom_command(OUTPUT "${greetings}"
        COMMAND python3 "${greetings_tool}" "${greetings_catalog}" "${greetings}"
                --longest ${greeting_longest} --longest-named ${greeting_longest_named}
        DEPENDS "${greetings_tool}" "${greetings_catalog}"
        VERBATIM)
set(variant_sources ${LV_EDITOR_PROJECT_SOURCES} ${images} "${greetings}")
if(CONFIG_IDF_TARGET_ESP32S3)
    list(APPEND variant_sources
            "${ui}/widgets/radar/radar_labels_s3.S"
            "${ui}/widgets/radar/radar_wedge_blend.c"
            "${ui}/widgets/radar/radar_wedge_pie.S")
endif()
set(variant_includes "${CMAKE_CURRENT_LIST_DIR}/../../ui" "${CMAKE_CURRENT_BINARY_DIR}/generated")
