#include "choice_row_gen.h"
#include "../airgrep_ui.h"

static void choice_row_line_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void choice_row_note_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void choice_row_check_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * choice_row_create(lv_obj_t * parent, const char * title, const char * detail, bool separator)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "choice_row_#");
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, TOUCH);
        lv_obj_set_clickable(lv_obj_0, true);
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_obj_set_style_pad_left(lv_obj_0, 16, 0);

        lv_obj_remove_theme(lv_obj_0, LV_PART_MAIN);
        lv_obj_add_style(lv_obj_0, &style_pressed_clear, LV_STATE_PRESSED);
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
        lv_subject_add_observer_obj(color_separator, choice_row_line_style_border_color_binding_cb, line, NULL);
        lv_obj_set_style_border_width(line, separator ? 1 : 0, 0);
        lv_obj_set_style_border_side(line, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_remove_theme(line, LV_PART_MAIN);
        lv_obj_t * label = lv_label_create(line);
        lv_obj_set_name(label, "label");
        lv_label_set_text(label, title);

        lv_obj_t * value = lv_obj_create(line);
        lv_obj_set_name(value, "value");
        lv_obj_set_width(value, LV_SIZE_CONTENT);
        lv_obj_set_height(value, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(value, LV_FLEX_FLOW_ROW);
        lv_obj_set_clickable(value, false);
        lv_obj_set_scrollable(value, false);
        lv_obj_set_style_flex_cross_place(value, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_column(value, 10, 0);
        lv_obj_set_style_translate_y(value, -1, 0);
        lv_obj_set_style_text_font(value, font_title, 0);
        lv_obj_remove_theme(value, LV_PART_MAIN);
        lv_obj_t * note = lv_label_create(value);
        lv_obj_set_name(note, "note");
        lv_label_set_text(note, detail);
        lv_subject_add_observer_obj(color_label_2, choice_row_note_style_text_color_binding_cb, note, NULL);

        lv_obj_t * check = lv_image_create(value);
        lv_obj_set_name(check, "check");
        lv_image_set_src(check, icon_chosen);
        lv_subject_add_observer_obj(color_tint, choice_row_check_style_image_recolor_binding_cb, check, NULL);
        lv_obj_set_style_image_recolor_opa(check, (255 * 100 / 100), 0);
        lv_obj_set_style_image_opa(check, 0, 0);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void choice_row_line_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_color(color_separator), 0);
}

static void choice_row_note_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

static void choice_row_check_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_tint), 0);
}

