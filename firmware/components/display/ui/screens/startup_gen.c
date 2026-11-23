#include "startup_gen.h"
#include "../airgrep_ui.h"

static void startup_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void startup_lv_obj_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void startup_unit_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * startup_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "startup_#");
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_subject_add_observer_obj(color_bg, startup_lv_obj_0_style_bg_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(color_label, startup_lv_obj_0_style_text_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_text_font(lv_obj_0, font_text, 0);

        lv_obj_t * logo = logo_create(lv_obj_0);
        lv_obj_set_name(logo, "logo");
        lv_obj_set_align(logo, LV_ALIGN_TOP_MID);
        lv_obj_set_y(logo, 112);

        lv_obj_t * unit = lv_label_create(lv_obj_0);
        lv_obj_set_name(unit, "unit");
        lv_label_set_text(unit, "");
        lv_obj_set_align(unit, LV_ALIGN_TOP_MID);
        lv_obj_set_y(unit, 263);
        lv_subject_add_observer_obj(color_label_2, startup_unit_style_text_color_binding_cb, unit, NULL);

        lv_obj_t * greeting = lv_label_create(lv_obj_0);
        lv_obj_set_name(greeting, "greeting");
        lv_label_set_text(greeting, "");
        lv_obj_set_align(greeting, LV_ALIGN_TOP_MID);
        lv_obj_set_y(greeting, 346);
        lv_obj_set_style_text_font(greeting, font_data, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void startup_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_bg), 0);
}

static void startup_lv_obj_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label), 0);
}

static void startup_unit_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

