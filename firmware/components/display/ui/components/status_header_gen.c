#include "status_header_gen.h"
#include "../airgrep_ui.h"

static void status_header_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_obj_0_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_label_0_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_label_1_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_label_1_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_health_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_0_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_1_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_1_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_2_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_2_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_3_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_3_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_4_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_4_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_5_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_5_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_6_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_6_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_7_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_7_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_8_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_8_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_9_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_9_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_label_2_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_label_2_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_battery_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_battery_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_battery_terminal_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_10_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_10_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_11_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_11_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_12_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_12_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_13_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void status_header_lv_image_13_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * status_header_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "status_header_#");
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, TOUCH);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_ROW);
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_subject_add_observer_obj(color_bg, status_header_lv_obj_0_style_bg_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(color_separator, status_header_lv_obj_0_style_border_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_border_width(lv_obj_0, HAIRLINE, 0);
        lv_obj_set_style_border_side(lv_obj_0, LV_BORDER_SIDE_BOTTOM, 0);

        lv_obj_remove_theme(lv_obj_0, LV_PART_MAIN);
        lv_obj_t * left = lv_obj_create(lv_obj_0);
        lv_obj_set_name(left, "left");
        lv_obj_set_height(left, lv_pct(100));
        lv_obj_set_flex_grow(left, 1);
        lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
        lv_obj_set_clickable(left, false);
        lv_obj_set_scrollable(left, false);
        lv_obj_set_style_flex_cross_place(left, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_track_place(left, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_hor(left, 20, 0);
        lv_obj_set_style_pad_column(left, 12, 0);
        lv_obj_remove_theme(left, LV_PART_MAIN);
        lv_obj_t * lv_label_0 = lv_label_create(left);
        lv_subject_add_observer_obj(subject_time, status_header_lv_label_0_text_binding_cb, lv_label_0, NULL);
        lv_obj_set_style_text_font(lv_label_0, font_title, 0);

        lv_obj_t * lv_label_1 = lv_label_create(left);
        lv_subject_add_observer_obj(subject_time_zone, status_header_lv_label_1_text_binding_cb, lv_label_1, NULL);
        lv_obj_set_style_translate_x(lv_label_1, -8, 0);
        lv_obj_set_style_translate_y(lv_label_1, 5, 0);
        lv_obj_set_style_text_font(lv_label_1, font_small, 0);
        lv_subject_add_observer_obj(color_label_2, status_header_lv_label_1_style_text_color_binding_cb, lv_label_1, NULL);

        lv_obj_t * health = lv_obj_create(left);
        lv_obj_set_name(health, "health");
        lv_obj_set_style_translate_x(health, -4, 0);
        lv_obj_set_width(health, 12);
        lv_obj_set_height(health, 12);
        lv_obj_set_style_radius(health, 6, 0);
        lv_obj_set_style_bg_opa(health, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(subject_health, status_header_health_style_bg_color_binding_cb, health, NULL);
        lv_subject_add_observer_obj(color_green, status_header_health_style_bg_color_binding_cb, health, NULL);
        lv_subject_add_observer_obj(color_yellow, status_header_health_style_bg_color_binding_cb, health, NULL);
        lv_subject_add_observer_obj(color_red, status_header_health_style_bg_color_binding_cb, health, NULL);
        lv_obj_remove_theme(health, LV_PART_MAIN);

        lv_obj_t * lv_image_0 = lv_image_create(left);
        lv_image_set_src(lv_image_0, icon_gps_search);
        lv_subject_add_observer_obj(subject_gps_symbol, status_header_lv_image_0_hidden_binding_cb, lv_image_0, NULL);
        lv_subject_add_observer_obj(color_yellow, status_header_lv_image_0_style_image_recolor_binding_cb, lv_image_0, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_0, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_1 = lv_image_create(left);
        lv_image_set_src(lv_image_1, icon_gps_2d);
        lv_subject_add_observer_obj(subject_gps_symbol, status_header_lv_image_1_hidden_binding_cb, lv_image_1, NULL);
        lv_subject_add_observer_obj(color_yellow, status_header_lv_image_1_style_image_recolor_binding_cb, lv_image_1, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_1, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_2 = lv_image_create(left);
        lv_image_set_src(lv_image_2, icon_gps_3d);
        lv_subject_add_observer_obj(subject_gps_symbol, status_header_lv_image_2_hidden_binding_cb, lv_image_2, NULL);
        lv_subject_add_observer_obj(color_green, status_header_lv_image_2_style_image_recolor_binding_cb, lv_image_2, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_2, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_3 = lv_image_create(left);
        lv_image_set_src(lv_image_3, icon_gps_antenna);
        lv_subject_add_observer_obj(subject_gps_symbol, status_header_lv_image_3_hidden_binding_cb, lv_image_3, NULL);
        lv_subject_add_observer_obj(color_red, status_header_lv_image_3_style_image_recolor_binding_cb, lv_image_3, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_3, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_4 = lv_image_create(left);
        lv_image_set_src(lv_image_4, icon_gps_antenna);
        lv_subject_add_observer_obj(subject_gps_symbol, status_header_lv_image_4_hidden_binding_cb, lv_image_4, NULL);
        lv_subject_add_observer_obj(color_yellow, status_header_lv_image_4_style_image_recolor_binding_cb, lv_image_4, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_4, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_5 = lv_image_create(left);
        lv_image_set_src(lv_image_5, icon_gps_fault);
        lv_subject_add_observer_obj(subject_gps_symbol, status_header_lv_image_5_hidden_binding_cb, lv_image_5, NULL);
        lv_subject_add_observer_obj(color_red, status_header_lv_image_5_style_image_recolor_binding_cb, lv_image_5, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_5, (255 * 100 / 100), 0);

        lv_obj_t * right = lv_obj_create(lv_obj_0);
        lv_obj_set_name(right, "right");
        lv_obj_set_height(right, lv_pct(100));
        lv_obj_set_flex_grow(right, 1);
        lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
        lv_obj_set_clickable(right, false);
        lv_obj_set_scrollable(right, false);
        lv_obj_set_style_flex_main_place(right, LV_FLEX_ALIGN_END, 0);
        lv_obj_set_style_flex_cross_place(right, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_track_place(right, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_hor(right, 20, 0);
        lv_obj_set_style_pad_top(right, 1, 0);
        lv_obj_set_style_pad_column(right, 12, 0);
        lv_obj_remove_theme(right, LV_PART_MAIN);
        lv_obj_t * lv_image_6 = lv_image_create(right);
        lv_image_set_src(lv_image_6, icon_speaker);
        lv_subject_add_observer_obj(subject_muted, status_header_lv_image_6_hidden_binding_cb, lv_image_6, NULL);
        lv_obj_set_style_translate_x(lv_image_6, 4, 0);
        lv_subject_add_observer_obj(color_label, status_header_lv_image_6_style_image_recolor_binding_cb, lv_image_6, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_6, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_7 = lv_image_create(right);
        lv_image_set_src(lv_image_7, icon_speaker_off);
        lv_subject_add_observer_obj(subject_muted, status_header_lv_image_7_hidden_binding_cb, lv_image_7, NULL);
        lv_obj_set_style_translate_x(lv_image_7, 4, 0);
        lv_subject_add_observer_obj(color_label, status_header_lv_image_7_style_image_recolor_binding_cb, lv_image_7, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_7, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_8 = lv_image_create(right);
        lv_image_set_src(lv_image_8, icon_wifi);
        lv_subject_add_observer_obj(subject_wifi, status_header_lv_image_8_hidden_binding_cb, lv_image_8, NULL);
        lv_obj_set_style_translate_x(lv_image_8, 4, 0);
        lv_subject_add_observer_obj(color_label, status_header_lv_image_8_style_image_recolor_binding_cb, lv_image_8, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_8, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_9 = lv_image_create(right);
        lv_image_set_src(lv_image_9, icon_wifi_off);
        lv_subject_add_observer_obj(subject_wifi, status_header_lv_image_9_hidden_binding_cb, lv_image_9, NULL);
        lv_obj_set_style_translate_x(lv_image_9, 4, 0);
        lv_subject_add_observer_obj(color_label, status_header_lv_image_9_style_image_recolor_binding_cb, lv_image_9, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_9, (255 * 100 / 100), 0);

        lv_obj_t * lv_label_2 = lv_label_create(right);
        lv_subject_add_observer_obj(subject_battery, status_header_lv_label_2_text_binding_cb, lv_label_2, NULL);
        lv_subject_add_observer_obj(subject_battery_state, status_header_lv_label_2_hidden_binding_cb, lv_label_2, NULL);
        lv_obj_set_style_translate_x(lv_label_2, 4, 0);
        lv_obj_set_style_translate_y(lv_label_2, -1, 0);

        lv_obj_t * battery = lv_bar_create(right);
        lv_obj_set_name(battery, "battery");
        lv_obj_set_width(battery, 40);
        lv_obj_set_height(battery, 20);
        lv_bar_bind_value(battery, subject_battery);
        lv_obj_set_style_radius(battery, 6, 0);
        lv_obj_set_style_pad_all(battery, 4, 0);
        lv_obj_set_style_bg_opa(battery, 0, 0);
        lv_obj_set_style_border_width(battery, 2, 0);
        lv_subject_add_observer_obj(subject_battery_state, status_header_battery_style_border_color_binding_cb, battery, NULL);
        lv_subject_add_observer_obj(color_grid, status_header_battery_style_border_color_binding_cb, battery, NULL);
        lv_subject_add_observer_obj(color_label, status_header_battery_style_border_color_binding_cb, battery, NULL);
        lv_obj_set_style_radius(battery, 2, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(battery, (255 * 100 / 100), LV_PART_INDICATOR);
        lv_subject_add_observer_obj(subject_battery, status_header_battery_style_bg_color_binding_cb, battery, NULL);
        lv_subject_add_observer_obj(color_red, status_header_battery_style_bg_color_binding_cb, battery, NULL);
        lv_subject_add_observer_obj(color_green, status_header_battery_style_bg_color_binding_cb, battery, NULL);
        lv_obj_remove_theme(battery, LV_PART_MAIN);
        lv_obj_remove_theme(battery, LV_PART_INDICATOR);

        lv_obj_t * battery_terminal = lv_obj_create(right);
        lv_obj_set_name(battery_terminal, "battery_terminal");
        lv_obj_set_floating(battery_terminal, true);
        lv_obj_set_align(battery_terminal, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(battery_terminal, 4);
        lv_obj_set_width(battery_terminal, 3);
        lv_obj_set_height(battery_terminal, 6);
        lv_obj_set_style_radius(battery_terminal, 1, 0);
        lv_obj_set_style_bg_opa(battery_terminal, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(subject_battery_state, status_header_battery_terminal_style_bg_color_binding_cb, battery_terminal, NULL);
        lv_subject_add_observer_obj(color_grid, status_header_battery_terminal_style_bg_color_binding_cb, battery_terminal, NULL);
        lv_subject_add_observer_obj(color_label, status_header_battery_terminal_style_bg_color_binding_cb, battery_terminal, NULL);
        lv_obj_remove_theme(battery_terminal, LV_PART_MAIN);

        lv_obj_t * lv_image_10 = lv_image_create(right);
        lv_image_set_src(lv_image_10, icon_bolt_cut);
        lv_obj_set_floating(lv_image_10, true);
        lv_obj_set_align(lv_image_10, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lv_image_10, -8);
        lv_subject_add_observer_obj(subject_battery_state, status_header_lv_image_10_hidden_binding_cb, lv_image_10, NULL);
        lv_subject_add_observer_obj(color_bg, status_header_lv_image_10_style_image_recolor_binding_cb, lv_image_10, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_10, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_11 = lv_image_create(right);
        lv_image_set_src(lv_image_11, icon_bolt);
        lv_obj_set_floating(lv_image_11, true);
        lv_obj_set_align(lv_image_11, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lv_image_11, -8);
        lv_subject_add_observer_obj(subject_battery_state, status_header_lv_image_11_hidden_binding_cb, lv_image_11, NULL);
        lv_subject_add_observer_obj(color_label, status_header_lv_image_11_style_image_recolor_binding_cb, lv_image_11, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_11, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_12 = lv_image_create(right);
        lv_image_set_src(lv_image_12, icon_battery_slash_cut);
        lv_obj_set_floating(lv_image_12, true);
        lv_obj_set_align(lv_image_12, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lv_image_12, -3);
        lv_subject_add_observer_obj(subject_battery_state, status_header_lv_image_12_hidden_binding_cb, lv_image_12, NULL);
        lv_subject_add_observer_obj(color_bg, status_header_lv_image_12_style_image_recolor_binding_cb, lv_image_12, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_12, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_13 = lv_image_create(right);
        lv_image_set_src(lv_image_13, icon_battery_slash);
        lv_obj_set_floating(lv_image_13, true);
        lv_obj_set_align(lv_image_13, LV_ALIGN_RIGHT_MID);
        lv_obj_set_x(lv_image_13, -3);
        lv_subject_add_observer_obj(subject_battery_state, status_header_lv_image_13_hidden_binding_cb, lv_image_13, NULL);
        lv_subject_add_observer_obj(color_label, status_header_lv_image_13_style_image_recolor_binding_cb, lv_image_13, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_13, (255 * 100 / 100), 0);

        lv_obj_t * touch_left = lv_obj_create(lv_obj_0);
        lv_obj_set_name(touch_left, "touch_left");
        lv_obj_set_floating(touch_left, true);
        lv_obj_set_align(touch_left, LV_ALIGN_LEFT_MID);
        lv_obj_set_width(touch_left, lv_pct(50));
        lv_obj_set_height(touch_left, lv_pct(100));
        lv_obj_set_clickable(touch_left, true);
        lv_obj_set_scrollable(touch_left, false);
        lv_obj_remove_theme(touch_left, LV_PART_MAIN);
        lv_obj_add_style(touch_left, &style_pressed_clear, LV_STATE_PRESSED);
        lv_obj_add_event_cb(touch_left, header_left_clicked, LV_EVENT_CLICKED, NULL);

        lv_obj_t * touch_right = lv_obj_create(lv_obj_0);
        lv_obj_set_name(touch_right, "touch_right");
        lv_obj_set_floating(touch_right, true);
        lv_obj_set_align(touch_right, LV_ALIGN_RIGHT_MID);
        lv_obj_set_width(touch_right, lv_pct(50));
        lv_obj_set_height(touch_right, lv_pct(100));
        lv_obj_set_clickable(touch_right, true);
        lv_obj_set_scrollable(touch_right, false);
        lv_obj_remove_theme(touch_right, LV_PART_MAIN);
        lv_obj_add_style(touch_right, &style_pressed_clear, LV_STATE_PRESSED);
        lv_obj_add_event_cb(touch_right, header_right_clicked, LV_EVENT_CLICKED, NULL);

        the_root = lv_obj_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void status_header_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_bg), 0);
}

static void status_header_lv_obj_0_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_color(color_separator), 0);
}

static void status_header_lv_label_0_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_time));
}

static void status_header_lv_label_1_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_time_zone));
}

static void status_header_lv_label_1_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

static void status_header_health_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_int(subject_health) == 0 ? lv_subject_get_color(color_green) : lv_subject_get_int(subject_health) == 1 ? lv_subject_get_color(color_yellow) : lv_subject_get_color(color_red), 0);
}

static void status_header_lv_image_0_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps_symbol) != 0);
}

static void status_header_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_yellow), 0);
}

static void status_header_lv_image_1_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps_symbol) != 1);
}

static void status_header_lv_image_1_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_yellow), 0);
}

static void status_header_lv_image_2_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps_symbol) != 2);
}

static void status_header_lv_image_2_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_green), 0);
}

static void status_header_lv_image_3_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps_symbol) != 3);
}

static void status_header_lv_image_3_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_red), 0);
}

static void status_header_lv_image_4_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps_symbol) != 4);
}

static void status_header_lv_image_4_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_yellow), 0);
}

static void status_header_lv_image_5_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps_symbol) != 5);
}

static void status_header_lv_image_5_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_red), 0);
}

static void status_header_lv_image_6_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_muted) == 1);
}

static void status_header_lv_image_6_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

static void status_header_lv_image_7_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_muted) == 0);
}

static void status_header_lv_image_7_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

static void status_header_lv_image_8_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_wifi) == 0);
}

static void status_header_lv_image_8_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

static void status_header_lv_image_9_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_wifi) == 1);
}

static void status_header_lv_image_9_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

static void status_header_lv_label_2_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    char eval_buf_1[LV_XML_EVAL_STRING_BUF_SIZE];
    lv_snprintf(eval_buf_1, sizeof(eval_buf_1), "%d%s", (int)(lv_subject_get_int(subject_battery)), " %");
    lv_label_set_text(obj, eval_buf_1);
}

static void status_header_lv_label_2_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_battery_state) == 0);
}

static void status_header_battery_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_int(subject_battery_state) == 0 ? lv_subject_get_color(color_grid) : lv_subject_get_color(color_label), 0);
}

static void status_header_battery_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_int(subject_battery) < 20 ? lv_subject_get_color(color_red) : lv_subject_get_color(color_green), LV_PART_INDICATOR);
}

static void status_header_battery_terminal_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_int(subject_battery_state) == 0 ? lv_subject_get_color(color_grid) : lv_subject_get_color(color_label), 0);
}

static void status_header_lv_image_10_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_battery_state) != 2);
}

static void status_header_lv_image_10_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_bg), 0);
}

static void status_header_lv_image_11_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_battery_state) != 2);
}

static void status_header_lv_image_11_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

static void status_header_lv_image_12_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_battery_state) != 0);
}

static void status_header_lv_image_12_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_bg), 0);
}

static void status_header_lv_image_13_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_battery_state) != 0);
}

static void status_header_lv_image_13_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

