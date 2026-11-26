# =============================================================================
# user_config.cmake - Add your custom source files here
# =============================================================================
#
# This file is included by the generated CMakeLists.txt and allows you to
# add extra source files to the project without modifying generated files
# (which may be overwritten).
#
# To add your own sources, append them to LV_EDITOR_PROJECT_SOURCES:
#
#   list(APPEND LV_EDITOR_PROJECT_SOURCES
#       ${CMAKE_CURRENT_LIST_DIR}/src/my_widget.c
#       ${CMAKE_CURRENT_LIST_DIR}/src/my_screen.c
#   )
#
# Tip:
#   - Use ${CMAKE_CURRENT_LIST_DIR} to get paths relative to this file
#
# =============================================================================

list(APPEND LV_EDITOR_COMPONENT_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../airgrep")
list(APPEND LV_EDITOR_COMPONENT_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/../include")
list(APPEND LV_EDITOR_PROJECT_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/widgets/radar/render_study.c
    ${CMAKE_CURRENT_LIST_DIR}/widgets/traffic_trend.c
    ${CMAKE_CURRENT_LIST_DIR}/widgets/radar/radar_track.c
    ${CMAKE_CURRENT_LIST_DIR}/widgets/radar/radar_sections.c
    ${CMAKE_CURRENT_LIST_DIR}/widgets/radar/radar_bands.c
    ${CMAKE_CURRENT_LIST_DIR}/widgets/radar/radar_labels.c
    ${CMAKE_CURRENT_LIST_DIR}/widgets/radar/radar_grow.c
    ${CMAKE_CURRENT_LIST_DIR}/widgets/radar/radar_grid.c
    ${CMAKE_CURRENT_LIST_DIR}/widgets/radar/radar_outline.c
    ${CMAKE_CURRENT_LIST_DIR}/widgets/radar/radar_symbol_atlas.c
)
