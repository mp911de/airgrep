#include "page_altitude_gen.h"
#include "../airgrep_ui.h"

static void page_altitude_lv_label_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * page_altitude_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * page_0 = page_create(parent, 9);
        lv_obj_set_name_static(page_0, "page_altitude_#");

        lv_obj_t * nav_bar_0 = nav_bar_create(page_0, "altitude", 9, 1);
        lv_obj_t * nav_bar_0_trailing = nav_bar_get_trailing(nav_bar_0);
        if (nav_bar_0_trailing) {
        lv_obj_t * field = entry_field_create(nav_bar_0_trailing);
        lv_obj_set_name(field, "field");

        lv_obj_t * lv_label_0 = lv_label_create(nav_bar_0_trailing);
        lv_label_set_text(lv_label_0, "ft");
        lv_obj_set_style_text_font(lv_label_0, font_title, 0);
        lv_subject_add_observer_obj(color_label_2, page_altitude_lv_label_0_style_text_color_binding_cb, lv_label_0, NULL);
        } else {
        LV_LOG_WARN("`trailing` slot of `nav_bar_0` doesn't exist");
        }

        lv_obj_t * keys = keyboard_create(page_0);
        lv_obj_set_name(keys, "keys");
        keyboard_set_layout(keys, "digits");
        keyboard_set_hint(keys, "altitude_zero");

        the_root = page_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void page_altitude_lv_label_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

