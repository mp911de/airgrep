#include "cause_row_gen.h"
#include "../airgrep_ui.h"

static void cause_row_line_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void cause_row_failure_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void cause_row_caution_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * cause_row_create(lv_obj_t * parent, const char * title, const char * value, bool separator)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "cause_row_#");
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, TOUCH);
        lv_obj_set_clickable(lv_obj_0, false);
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_obj_set_style_pad_left(lv_obj_0, 16, 0);

        lv_obj_remove_theme(lv_obj_0, LV_PART_MAIN);
        lv_obj_t * line = lv_obj_create(lv_obj_0);
        lv_obj_set_name(line, "line");
        lv_obj_set_width(line, lv_pct(100));
        lv_obj_set_height(line, lv_pct(100));
        lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
        lv_obj_set_clickable(line, false);
        lv_obj_set_scrollable(line, false);
        lv_obj_set_style_flex_main_place(line, LV_FLEX_ALIGN_SPACE_BETWEEN, 0);
        lv_obj_set_style_flex_cross_place(line, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_track_place(line, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_right(line, 16, 0);
        lv_subject_add_observer_obj(color_separator, cause_row_line_style_border_color_binding_cb, line, NULL);
        lv_obj_set_style_border_width(line, separator ? 1 : 0, 0);
        lv_obj_set_style_border_side(line, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_remove_theme(line, LV_PART_MAIN);
        lv_obj_t * label = lv_label_create(line);
        lv_obj_set_name(label, "label");
        lv_label_set_text(label, title);

        lv_obj_t * failure = lv_label_create(line);
        lv_obj_set_name(failure, "failure");
        lv_label_set_text(failure, value);
        lv_obj_set_hidden(failure, true);
        lv_obj_set_style_translate_y(failure, -1, 0);
        lv_obj_set_style_text_font(failure, font_title, 0);
        lv_subject_add_observer_obj(color_red, cause_row_failure_style_text_color_binding_cb, failure, NULL);

        lv_obj_t * caution = lv_label_create(line);
        lv_obj_set_name(caution, "caution");
        lv_label_set_text(caution, value);
        lv_obj_set_style_translate_y(caution, -1, 0);
        lv_obj_set_style_text_font(caution, font_title, 0);
        lv_subject_add_observer_obj(color_yellow, cause_row_caution_style_text_color_binding_cb, caution, NULL);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void cause_row_line_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_color(color_separator), 0);
}

static void cause_row_failure_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_red), 0);
}

static void cause_row_caution_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_yellow), 0);
}

