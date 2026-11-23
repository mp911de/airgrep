#include "page_zones_gen.h"
#include "../airgrep_ui.h"

lv_obj_t * page_zones_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * page_0 = page_create(parent, 8);
        lv_obj_set_name_static(page_0, "page_zones_#");

        nav_bar_create(page_0, "time_zone", 8, 7);

        lv_obj_t * lv_obj_0 = lv_obj_create(page_0);
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, 32);
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_obj_remove_theme(lv_obj_0, LV_PART_MAIN);

        lv_obj_t * sheet = lv_obj_create(page_0);
        lv_obj_set_name(sheet, "sheet");
        lv_obj_set_width(sheet, lv_pct(100));
        lv_obj_set_flex_grow(sheet, 1);
        lv_obj_set_flex_flow(sheet, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(sheet, LV_DIR_VER);
        lv_obj_set_scroll_elastic(sheet, false);
        lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_ACTIVE);
        lv_obj_remove_theme(sheet, LV_PART_MAIN);
        lv_obj_t * zone_list = list_group_create(sheet);
        lv_obj_set_name(zone_list, "zone_list");

        the_root = page_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

