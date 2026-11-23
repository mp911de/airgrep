#include "page_callsign_gen.h"
#include "../airgrep_ui.h"

lv_obj_t * page_callsign_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * page_0 = page_create(parent, 6);
        lv_obj_set_name_static(page_0, "page_callsign_#");

        lv_obj_t * nav_bar_0 = nav_bar_create(page_0, "callsign", 6, 1);
        lv_obj_t * nav_bar_0_trailing = nav_bar_get_trailing(nav_bar_0);
        if (nav_bar_0_trailing) {
        lv_obj_t * field = entry_field_create(nav_bar_0_trailing);
        lv_obj_set_name(field, "field");
        } else {
        LV_LOG_WARN("`trailing` slot of `nav_bar_0` doesn't exist");
        }

        lv_obj_t * keys = keyboard_create(page_0);
        lv_obj_set_name(keys, "keys");
        keyboard_set_layout(keys, "en");

        the_root = page_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

