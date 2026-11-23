#include "traffic_header_gen.h"
#include "../airgrep_ui.h"

static void traffic_header_lv_obj_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void traffic_header_lv_obj_0_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * traffic_header_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "traffic_header_#");
        lv_obj_set_width(lv_obj_0, TRAFFIC_WIDTH);
        lv_obj_set_height(lv_obj_0, 32);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_ROW);
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_obj_set_style_flex_cross_place(lv_obj_0, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_track_place(lv_obj_0, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_hor(lv_obj_0, TRAFFIC_PADDING, 0);
        lv_obj_set_style_pad_top(lv_obj_0, 2, 0);
        lv_obj_set_style_pad_column(lv_obj_0, TRAFFIC_COLUMN_GAP, 0);
        lv_obj_set_style_text_font(lv_obj_0, font_small, 0);
        lv_subject_add_observer_obj(color_label_2, traffic_header_lv_obj_0_style_text_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_text_letter_space(lv_obj_0, 1, 0);
        lv_subject_add_observer_obj(color_separator, traffic_header_lv_obj_0_style_border_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_border_width(lv_obj_0, HAIRLINE, 0);
        lv_obj_set_style_border_side(lv_obj_0, LV_BORDER_SIDE_BOTTOM, 0);

        lv_obj_remove_theme(lv_obj_0, LV_PART_MAIN);
        lv_obj_t * lv_label_0 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_0, "TRAFFIC");
        lv_obj_set_width(lv_label_0, TRAFFIC_TITLE_WIDTH);

        lv_obj_t * lv_label_1 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_1, "ALT");
        lv_obj_set_width(lv_label_1, TRAFFIC_ALT_WIDTH);
        lv_obj_set_style_text_align(lv_label_1, LV_TEXT_ALIGN_RIGHT, 0);

        lv_obj_t * lv_obj_1 = lv_obj_create(lv_obj_0);
        lv_obj_set_width(lv_obj_1, TRAFFIC_TREND_WIDTH);
        lv_obj_set_height(lv_obj_1, 1);
        lv_obj_remove_theme(lv_obj_1, LV_PART_MAIN);

        lv_obj_t * lv_label_2 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_2, "BRG");
        lv_obj_set_width(lv_label_2, TRAFFIC_BEARING_WIDTH);
        lv_obj_set_style_text_align(lv_label_2, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t * lv_label_3 = lv_label_create(lv_obj_0);
        lv_label_set_text(lv_label_3, "NM");
        lv_obj_set_width(lv_label_3, TRAFFIC_DISTANCE_WIDTH);
        lv_obj_set_style_text_align(lv_label_3, LV_TEXT_ALIGN_RIGHT, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void traffic_header_lv_obj_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

static void traffic_header_lv_obj_0_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_color(color_separator), 0);
}

