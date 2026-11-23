#include "page_licenses_gen.h"
#include "../airgrep_ui.h"

lv_obj_t * page_licenses_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * page_0 = page_create(parent, 3);
        lv_obj_set_name_static(page_0, "page_licenses_#");

        nav_bar_create(page_0, "licenses", 3, 1);

        lv_obj_t * sheet = lv_obj_create(page_0);
        lv_obj_set_name(sheet, "sheet");
        lv_obj_set_width(sheet, lv_pct(100));
        lv_obj_set_flex_grow(sheet, 1);
        lv_obj_set_flex_flow(sheet, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(sheet, LV_DIR_VER);
        lv_obj_set_scroll_elastic(sheet, false);
        lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_ACTIVE);
        lv_obj_remove_theme(sheet, LV_PART_MAIN);
        section_header_create(sheet, "section_components");

        lv_obj_t * license_list = list_group_create(sheet);
        lv_obj_set_name(license_list, "license_list");

        the_root = page_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

