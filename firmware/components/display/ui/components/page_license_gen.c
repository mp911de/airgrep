#include "page_license_gen.h"
#include "../airgrep_ui.h"

static void page_license_sheet_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * page_license_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * page_0 = page_create(parent, 4);
        lv_obj_set_name_static(page_0, "page_license_#");

        nav_bar_create(page_0, "licenses", 4, 3);

        lv_obj_t * sheet = lv_obj_create(page_0);
        lv_obj_set_name(sheet, "sheet");
        lv_obj_set_width(sheet, lv_pct(100));
        lv_obj_set_flex_grow(sheet, 1);
        lv_obj_set_scroll_dir(sheet, LV_DIR_VER);
        lv_obj_set_scroll_elastic(sheet, false);
        lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_ACTIVE);
        lv_subject_add_observer_obj(color_surface, page_license_sheet_style_bg_color_binding_cb, sheet, NULL);
        lv_obj_set_style_bg_opa(sheet, (255 * 100 / 100), 0);
        lv_obj_set_style_pad_left(sheet, 24, 0);
        lv_obj_set_style_pad_right(sheet, 32, 0);
        lv_obj_set_style_pad_top(sheet, 21, 0);
        lv_obj_set_style_pad_bottom(sheet, 20, 0);
        lv_obj_remove_theme(sheet, LV_PART_MAIN);
        lv_obj_t * license_text = lv_label_create(sheet);
        lv_obj_set_name(license_text, "license_text");
        lv_obj_set_width(license_text, lv_pct(100));
        lv_obj_set_style_text_line_space(license_text, 3, 0);

        the_root = page_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void page_license_sheet_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_surface), 0);
}

