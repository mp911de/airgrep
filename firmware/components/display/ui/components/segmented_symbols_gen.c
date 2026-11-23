#include "segmented_symbols_gen.h"
#include "../airgrep_ui.h"

static void segmented_symbols_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void segmented_symbols_planes_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void segmented_symbols_planes_style_bg_opa_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void segmented_symbols_set_subject_cb(lv_event_t * e);
static void segmented_symbols_simple_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void segmented_symbols_simple_style_bg_opa_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void segmented_symbols_set_subject_cb_2(lv_event_t * e);

lv_obj_t * segmented_symbols_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "segmented_symbols_#");
        lv_obj_set_width(lv_obj_0, LV_SIZE_CONTENT);
        lv_obj_set_height(lv_obj_0, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_ROW);
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_obj_set_style_pad_all(lv_obj_0, 3, 0);
        lv_obj_set_style_radius(lv_obj_0, RADIUS, 0);
        lv_subject_add_observer_obj(color_control, segmented_symbols_lv_obj_0_style_bg_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_obj_set_style_text_font(lv_obj_0, font_label, 0);

        lv_obj_remove_theme(lv_obj_0, LV_PART_MAIN);
        lv_obj_t * planes = lv_obj_create(lv_obj_0);
        lv_obj_set_name(planes, "planes");
        lv_obj_set_width(planes, LV_SIZE_CONTENT);
        lv_obj_set_height(planes, 44);
        lv_obj_set_clickable(planes, true);
        lv_obj_set_scrollable(planes, false);
        lv_obj_set_style_pad_hor(planes, 20, 0);
        lv_obj_set_style_radius(planes, 9, 0);
        lv_subject_add_observer_obj(color_surface, segmented_symbols_planes_style_bg_color_binding_cb, planes, NULL);
        lv_subject_add_observer_obj(subject_simple_symbols, segmented_symbols_planes_style_bg_opa_binding_cb, planes, NULL);
        lv_obj_remove_theme(planes, LV_PART_MAIN);
        lv_obj_t * lv_label_0 = lv_label_create(planes);
        lv_label_set_translation_tag(lv_label_0, "planes");
        lv_obj_set_align(lv_label_0, LV_ALIGN_CENTER);

        lv_obj_add_event_cb(planes, segmented_symbols_set_subject_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * simple = lv_obj_create(lv_obj_0);
        lv_obj_set_name(simple, "simple");
        lv_obj_set_width(simple, LV_SIZE_CONTENT);
        lv_obj_set_height(simple, 44);
        lv_obj_set_clickable(simple, true);
        lv_obj_set_scrollable(simple, false);
        lv_obj_set_style_pad_hor(simple, 20, 0);
        lv_obj_set_style_radius(simple, 9, 0);
        lv_subject_add_observer_obj(color_surface, segmented_symbols_simple_style_bg_color_binding_cb, simple, NULL);
        lv_subject_add_observer_obj(subject_simple_symbols, segmented_symbols_simple_style_bg_opa_binding_cb, simple, NULL);
        lv_obj_remove_theme(simple, LV_PART_MAIN);
        lv_obj_t * lv_label_1 = lv_label_create(simple);
        lv_label_set_translation_tag(lv_label_1, "simple");
        lv_obj_set_align(lv_label_1, LV_ALIGN_CENTER);

        lv_obj_add_event_cb(simple, segmented_symbols_set_subject_cb_2, LV_EVENT_CLICKED, NULL);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void segmented_symbols_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_control), 0);
}

static void segmented_symbols_planes_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_surface), 0);
}

static void segmented_symbols_planes_style_bg_opa_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_opa(obj, lv_subject_get_int(subject_simple_symbols) == 0 ? 255 : 0, 0);
}

static void segmented_symbols_set_subject_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_subject_set_int(subject_simple_symbols, 0);
}

static void segmented_symbols_simple_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_surface), 0);
}

static void segmented_symbols_simple_style_bg_opa_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_opa(obj, lv_subject_get_int(subject_simple_symbols) == 1 ? 255 : 0, 0);
}

static void segmented_symbols_set_subject_cb_2(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_subject_set_int(subject_simple_symbols, 1);
}

