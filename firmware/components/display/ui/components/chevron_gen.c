#include "chevron_gen.h"
#include "../airgrep_ui.h"

static void chevron_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * chevron_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_image_0 = lv_image_create(parent);
        lv_obj_set_name_static(lv_image_0, "chevron_#");
        lv_image_set_src(lv_image_0, icon_chevron_right);
        lv_subject_add_observer_obj(color_grid, chevron_lv_image_0_style_image_recolor_binding_cb, lv_image_0, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_0, (255 * 100 / 100), 0);
        lv_obj_set_style_translate_y(lv_image_0, 2, 0);

        the_root = lv_image_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void chevron_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_grid), 0);
}

