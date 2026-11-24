#include "charger_gen.h"
#include "../airgrep_ui.h"

static void charger_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void charger_lv_obj_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void charger_body_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void charger_fill_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void charger_fill_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void charger_terminal_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void charger_line_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void charger_bolt_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void charger_bolt_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * charger_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(NULL);
        lv_obj_set_name_static(lv_obj_0, "charger_#");
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_subject_add_observer_obj(color_bg, charger_lv_obj_0_style_bg_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(color_label, charger_lv_obj_0_style_text_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_text_font(lv_obj_0, font_display, 0);

        lv_obj_t * body = lv_obj_create(lv_obj_0);
        lv_obj_set_name(body, "body");
        lv_obj_set_x(body, 152);
        lv_obj_set_y(body, 64);
        lv_obj_set_width(body, 480);
        lv_obj_set_height(body, 240);
        lv_obj_set_scrollable(body, false);
        lv_obj_set_style_radius(body, 56, 0);
        lv_obj_set_style_border_width(body, 8, 0);
        lv_subject_add_observer_obj(color_label, charger_body_style_border_color_binding_cb, body, NULL);
        lv_obj_set_style_pad_all(body, 8, 0);
        lv_obj_remove_theme(body, LV_PART_MAIN);
        lv_obj_t * well = lv_obj_create(body);
        lv_obj_set_name(well, "well");
        lv_obj_set_width(well, lv_pct(100));
        lv_obj_set_height(well, lv_pct(100));
        lv_obj_set_scrollable(well, false);
        lv_obj_set_style_radius(well, 40, 0);
        lv_obj_set_style_clip_corner(well, true, 0);
        lv_obj_remove_theme(well, LV_PART_MAIN);
        lv_obj_t * percent = lv_obj_create(well);
        lv_obj_set_name(percent, "percent");
        lv_obj_set_width(percent, LV_SIZE_CONTENT);
        lv_obj_set_height(percent, LV_SIZE_CONTENT);
        lv_obj_set_align(percent, LV_ALIGN_CENTER);
        lv_obj_set_flex_flow(percent, LV_FLEX_FLOW_ROW);
        lv_obj_set_scrollable(percent, false);
        lv_obj_set_style_pad_column(percent, 16, 0);
        lv_obj_set_style_translate_y(percent, -1, 0);
        lv_obj_remove_theme(percent, LV_PART_MAIN);
        lv_obj_t * number = lv_label_create(percent);
        lv_obj_set_name(number, "number");
        lv_label_bind_text(number, subject_battery, "%d");

        lv_obj_t * lv_label_0 = lv_label_create(percent);
        lv_label_set_text(lv_label_0, "%");

        lv_obj_t * fill = lv_obj_create(well);
        lv_obj_set_name(fill, "fill");
        lv_obj_set_height(fill, lv_pct(100));
        lv_obj_set_scrollable(fill, false);
        lv_obj_set_style_radius(fill, 8, 0);
        lv_subject_add_observer_obj(color_green, charger_fill_style_bg_color_binding_cb, fill, NULL);
        lv_obj_set_style_bg_opa(fill, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(color_on_green, charger_fill_style_text_color_binding_cb, fill, NULL);
        lv_obj_remove_theme(fill, LV_PART_MAIN);
        lv_obj_t * percent_on = lv_obj_create(fill);
        lv_obj_set_name(percent_on, "percent_on");
        lv_obj_set_width(percent_on, LV_SIZE_CONTENT);
        lv_obj_set_height(percent_on, LV_SIZE_CONTENT);
        lv_obj_set_align(percent_on, LV_ALIGN_LEFT_MID);
        lv_obj_set_flex_flow(percent_on, LV_FLEX_FLOW_ROW);
        lv_obj_set_scrollable(percent_on, false);
        lv_obj_set_style_pad_column(percent_on, 16, 0);
        lv_obj_set_style_translate_y(percent_on, -1, 0);
        lv_obj_remove_theme(percent_on, LV_PART_MAIN);
        lv_obj_t * number_on = lv_label_create(percent_on);
        lv_obj_set_name(number_on, "number_on");
        lv_label_bind_text(number_on, subject_battery, "%d");

        lv_obj_t * lv_label_1 = lv_label_create(percent_on);
        lv_label_set_text(lv_label_1, "%");

        lv_obj_t * terminal = lv_obj_create(lv_obj_0);
        lv_obj_set_name(terminal, "terminal");
        lv_obj_set_x(terminal, 640);
        lv_obj_set_y(terminal, 148);
        lv_obj_set_width(terminal, 16);
        lv_obj_set_height(terminal, 72);
        lv_obj_set_style_radius(terminal, 8, 0);
        lv_subject_add_observer_obj(color_label, charger_terminal_style_bg_color_binding_cb, terminal, NULL);
        lv_obj_set_style_bg_opa(terminal, (255 * 100 / 100), 0);
        lv_obj_remove_theme(terminal, LV_PART_MAIN);

        lv_obj_t * line = lv_obj_create(lv_obj_0);
        lv_obj_set_name(line, "line");
        lv_obj_set_y(line, 347);
        lv_obj_set_width(line, lv_pct(100));
        lv_obj_set_height(line, 38);
        lv_obj_set_flex_flow(line, LV_FLEX_FLOW_ROW);
        lv_obj_set_scrollable(line, false);
        lv_obj_set_style_flex_main_place(line, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_cross_place(line, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_column(line, 16, 0);
        lv_obj_set_style_text_font(line, font_data, 0);
        lv_subject_add_observer_obj(color_label_2, charger_line_style_text_color_binding_cb, line, NULL);
        lv_obj_remove_theme(line, LV_PART_MAIN);
        lv_obj_t * bolt = lv_image_create(line);
        lv_obj_set_name(bolt, "bolt");
        lv_image_set_src(bolt, icon_bolt_line);
        lv_subject_add_observer_obj(subject_battery_state, charger_bolt_hidden_binding_cb, bolt, NULL);
        lv_subject_add_observer_obj(color_label_2, charger_bolt_style_image_recolor_binding_cb, bolt, NULL);
        lv_obj_set_style_image_recolor_opa(bolt, (255 * 100 / 100), 0);

        lv_obj_t * text = lv_label_create(line);
        lv_obj_set_name(text, "text");
        lv_label_set_text(text, "");

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void charger_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_bg), 0);
}

static void charger_lv_obj_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label), 0);
}

static void charger_body_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_color(color_label), 0);
}

static void charger_fill_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_green), 0);
}

static void charger_fill_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_on_green), 0);
}

static void charger_terminal_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_label), 0);
}

static void charger_line_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

static void charger_bolt_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_battery_state) != 2);
}

static void charger_bolt_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label_2), 0);
}

