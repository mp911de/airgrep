#include "list_group_gen.h"
#include "../airgrep_ui.h"

static void list_group_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void list_group_lv_obj_0_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void list_group_rows_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * list_group_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "list_group_#");
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, LV_SIZE_CONTENT);
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_subject_add_observer_obj(color_surface, list_group_lv_obj_0_style_bg_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(color_separator, list_group_lv_obj_0_style_border_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_border_width(lv_obj_0, HAIRLINE, 0);
        lv_obj_set_style_border_side(lv_obj_0, LV_BORDER_SIDE_TOP, 0);

        lv_obj_remove_theme(lv_obj_0, LV_PART_MAIN);
        lv_obj_t * rows = lv_obj_create(lv_obj_0);
        lv_obj_set_name(rows, "rows");
        lv_obj_set_width(rows, lv_pct(100));
        lv_obj_set_height(rows, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(rows, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollable(rows, false);
        lv_subject_add_observer_obj(color_separator, list_group_rows_style_border_color_binding_cb, rows, NULL);
        lv_obj_set_style_border_width(rows, HAIRLINE, 0);
        lv_obj_set_style_border_side(rows, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_remove_theme(rows, LV_PART_MAIN);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

lv_obj_t * list_group_get_rows(lv_obj_t *parent)
{
    return lv_obj_find_by_name(parent, "rows");
}

static void list_group_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_surface), 0);
}

static void list_group_lv_obj_0_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_color(color_separator), 0);
}

static void list_group_rows_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_color(color_separator), 0);
}

