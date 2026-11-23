#include "page_time_zone_gen.h"
#include "../airgrep_ui.h"

lv_obj_t * page_time_zone_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * page_0 = page_create(parent, 7);
        lv_obj_set_name_static(page_0, "page_time_zone_#");

        nav_bar_create(page_0, "time_zone", 7, 1);

        lv_obj_t * sheet = lv_obj_create(page_0);
        lv_obj_set_name(sheet, "sheet");
        lv_obj_set_width(sheet, lv_pct(100));
        lv_obj_set_flex_grow(sheet, 1);
        lv_obj_set_flex_flow(sheet, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(sheet, LV_DIR_VER);
        lv_obj_set_scroll_elastic(sheet, false);
        lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_ACTIVE);
        lv_obj_remove_theme(sheet, LV_PART_MAIN);
        section_header_create(sheet, "section_quick_access");

        lv_obj_t * quick_list = list_group_create(sheet);
        lv_obj_set_name(quick_list, "quick_list");

        section_header_create(sheet, "section_regions");

        lv_obj_t * region_list = list_group_create(sheet);
        lv_obj_set_name(region_list, "region_list");

        the_root = page_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

