#include "section_header_gen.h"
#include "../airgrep_ui.h"

static void section_header_lv_label_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * section_header_create(lv_obj_t * parent, const char * title)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "section_header_#");
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, 32);
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_obj_set_style_pad_left(lv_obj_0, 16, 0);
        lv_obj_set_style_pad_top(lv_obj_0, 10, 0);

        lv_obj_remove_theme(lv_obj_0, LV_PART_MAIN);
        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_label_set_translation_tag(lv_label_0, title);
        lv_obj_set_style_text_font(lv_label_0, font_small, 0);
        lv_subject_add_observer_obj(color_label_2, section_header_lv_label_0_style_text_color_binding_cb, lv_label_0, NULL);
        lv_obj_set_style_text_letter_space(lv_label_0, 1, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void section_header_lv_label_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

