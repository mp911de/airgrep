#include "page_system_gen.h"
#include "../airgrep_ui.h"

static void page_system_time_zone_automatic_checked_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_time_zone_automatic_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_time_zone_automatic_style_bg_color_binding_cb_2(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_set_subject_cb(lv_event_t * e);
static void page_system_time_zone_clickable_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_0_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_time_zone_chevron_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_1_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_2_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_2_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_3_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_3_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_4_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_4_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_5_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_5_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_image_0_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_image_1_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_image_1_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_image_2_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_image_2_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_image_3_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_image_3_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_6_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_6_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_7_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_7_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_8_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_8_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_9_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_9_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_10_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_10_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_11_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_11_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_12_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_12_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_13_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_13_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_14_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_15_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_lv_label_16_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_track_history_checked_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_track_history_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_track_history_style_bg_color_binding_cb_2(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_set_subject_cb_2(lv_event_t * e);
static void page_system_conflict_indicator_checked_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_conflict_indicator_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_conflict_indicator_style_bg_color_binding_cb_2(lv_observer_t * observer, lv_subject_t * subject);
static void page_system_set_subject_cb_3(lv_event_t * e);

lv_obj_t * page_system_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * page_0 = page_create(parent, 1);
        lv_obj_set_name_static(page_0, "page_system_#");

        nav_bar_create(page_0, "system", 1, 0);

        lv_obj_t * sheet = lv_obj_create(page_0);
        lv_obj_set_name(sheet, "sheet");
        lv_obj_set_width(sheet, lv_pct(100));
        lv_obj_set_flex_grow(sheet, 1);
        lv_obj_set_flex_flow(sheet, LV_FLEX_FLOW_ROW);
        lv_obj_set_scroll_dir(sheet, LV_DIR_VER);
        lv_obj_set_scroll_elastic(sheet, false);
        lv_obj_set_scrollbar_mode(sheet, LV_SCROLLBAR_MODE_ACTIVE);
        lv_obj_set_style_pad_column(sheet, 16, 0);
        lv_obj_remove_theme(sheet, LV_PART_MAIN);
        lv_obj_t * left = lv_obj_create(sheet);
        lv_obj_set_name(left, "left");
        lv_obj_set_flex_grow(left, 1);
        lv_obj_set_height(left, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollable(left, false);
        lv_obj_remove_theme(left, LV_PART_MAIN);
        section_header_create(left, "section_time_zone");

        lv_obj_t * list_group_0 = list_group_create(left);
        lv_obj_t * list_group_0_rows = list_group_get_rows(list_group_0);
        if (list_group_0_rows) {
        lv_obj_t * list_row_0 = list_row_create(list_group_0_rows, "automatic", true);
        lv_obj_t * list_row_0_value = list_row_get_value(list_row_0);
        if (list_row_0_value) {
        lv_obj_t * time_zone_automatic = lv_switch_create(list_row_0_value);
        lv_obj_set_name(time_zone_automatic, "time_zone_automatic");
        lv_obj_set_width(time_zone_automatic, 76);
        lv_obj_set_height(time_zone_automatic, 44);
        lv_obj_set_style_translate_y(time_zone_automatic, 2, 0);
        lv_subject_add_observer_obj(subject_time_zone_automatic, page_system_time_zone_automatic_checked_binding_cb, time_zone_automatic, NULL);
        lv_subject_add_observer_obj(color_control, page_system_time_zone_automatic_style_bg_color_binding_cb, time_zone_automatic, NULL);
        lv_obj_set_style_bg_opa(time_zone_automatic, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(time_zone_automatic, 22, 0);
        lv_subject_add_observer_obj(color_green, page_system_time_zone_automatic_style_bg_color_binding_cb_2, time_zone_automatic, NULL);
        lv_obj_set_style_bg_opa(time_zone_automatic, (255 * 100 / 100), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_radius(time_zone_automatic, 22, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(time_zone_automatic, lv_color_hex(0xffffff), LV_PART_KNOB);
        lv_obj_set_style_bg_opa(time_zone_automatic, (255 * 100 / 100), LV_PART_KNOB);
        lv_obj_set_style_radius(time_zone_automatic, 22, LV_PART_KNOB);
        lv_obj_set_style_pad_all(time_zone_automatic, -3, LV_PART_KNOB);
        lv_obj_remove_theme(time_zone_automatic, LV_PART_MAIN);
        lv_obj_remove_theme(time_zone_automatic, LV_PART_INDICATOR);
        lv_obj_remove_theme(time_zone_automatic, LV_PART_KNOB);
        lv_obj_add_event_cb(time_zone_automatic, page_system_set_subject_cb, LV_EVENT_CLICKED, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_0` doesn't exist");
        }

        lv_obj_t * time_zone = list_row_create(list_group_0_rows, "time_zone", false);
        lv_obj_set_name(time_zone, "time_zone");
        lv_subject_add_observer_obj(subject_time_zone_automatic, page_system_time_zone_clickable_binding_cb, time_zone, NULL);
        lv_obj_t * time_zone_value = list_row_get_value(time_zone);
        if (time_zone_value) {
        lv_obj_t * lv_label_0 = lv_label_create(time_zone_value);
        lv_subject_add_observer_obj(subject_zone, page_system_lv_label_0_text_binding_cb, lv_label_0, NULL);
        lv_subject_add_observer_obj(subject_time_zone_automatic, page_system_lv_label_0_style_text_color_binding_cb, lv_label_0, NULL);
        lv_subject_add_observer_obj(color_label_2, page_system_lv_label_0_style_text_color_binding_cb, lv_label_0, NULL);
        lv_subject_add_observer_obj(color_label, page_system_lv_label_0_style_text_color_binding_cb, lv_label_0, NULL);

        lv_obj_t * time_zone_chevron = chevron_create(time_zone_value);
        lv_obj_set_name(time_zone_chevron, "time_zone_chevron");
        lv_subject_add_observer_obj(subject_time_zone_automatic, page_system_time_zone_chevron_hidden_binding_cb, time_zone_chevron, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `time_zone` doesn't exist");
        }
        } else {
        LV_LOG_WARN("`rows` slot of `list_group_0` doesn't exist");
        }

        section_header_create(left, "section_health");

        lv_obj_t * health_list = list_group_create(left);
        lv_obj_set_name(health_list, "health_list");
        lv_obj_t * health_list_rows = list_group_get_rows(health_list);
        if (health_list_rows) {
        lv_obj_t * all_systems = list_row_create(health_list_rows, "all_systems", false);
        lv_obj_set_name(all_systems, "all_systems");
        lv_obj_t * all_systems_value = list_row_get_value(all_systems);
        if (all_systems_value) {
        lv_obj_t * lv_label_1 = lv_label_create(all_systems_value);
        lv_label_set_translation_tag(lv_label_1, "ok");
        lv_subject_add_observer_obj(color_green, page_system_lv_label_1_style_text_color_binding_cb, lv_label_1, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `all_systems` doesn't exist");
        }
        } else {
        LV_LOG_WARN("`rows` slot of `health_list` doesn't exist");
        }

        section_header_create(left, "section_gps");

        lv_obj_t * list_group_1 = list_group_create(left);
        lv_obj_t * list_group_1_rows = list_group_get_rows(list_group_1);
        if (list_group_1_rows) {
        lv_obj_t * list_row_1 = list_row_create(list_group_1_rows, "fix", true);
        lv_obj_t * list_row_1_value = list_row_get_value(list_row_1);
        if (list_row_1_value) {
        lv_obj_t * lv_label_2 = lv_label_create(list_row_1_value);
        lv_label_set_text(lv_label_2, "–");
        lv_subject_add_observer_obj(subject_gps, page_system_lv_label_2_hidden_binding_cb, lv_label_2, NULL);
        lv_subject_add_observer_obj(color_label_2, page_system_lv_label_2_style_text_color_binding_cb, lv_label_2, NULL);

        lv_obj_t * lv_label_3 = lv_label_create(list_row_1_value);
        lv_label_set_text(lv_label_3, "2D");
        lv_subject_add_observer_obj(subject_gps, page_system_lv_label_3_hidden_binding_cb, lv_label_3, NULL);
        lv_subject_add_observer_obj(color_yellow, page_system_lv_label_3_style_text_color_binding_cb, lv_label_3, NULL);

        lv_obj_t * lv_label_4 = lv_label_create(list_row_1_value);
        lv_label_set_text(lv_label_4, "3D");
        lv_subject_add_observer_obj(subject_gps, page_system_lv_label_4_hidden_binding_cb, lv_label_4, NULL);
        lv_subject_add_observer_obj(color_green, page_system_lv_label_4_style_text_color_binding_cb, lv_label_4, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_1` doesn't exist");
        }

        lv_obj_t * list_row_2 = list_row_create(list_group_1_rows, "satellites", true);
        lv_obj_t * list_row_2_value = list_row_get_value(list_row_2);
        if (list_row_2_value) {
        lv_obj_t * lv_label_5 = lv_label_create(list_row_2_value);
        lv_label_set_text(lv_label_5, "–");
        lv_subject_add_observer_obj(subject_gps_symbol, page_system_lv_label_5_hidden_binding_cb, lv_label_5, NULL);
        lv_subject_add_observer_obj(color_label_2, page_system_lv_label_5_style_text_color_binding_cb, lv_label_5, NULL);

        lv_obj_t * lv_image_0 = lv_image_create(list_row_2_value);
        lv_image_set_src(lv_image_0, icon_signal_0);
        lv_subject_add_observer_obj(subject_signal, page_system_lv_image_0_hidden_binding_cb, lv_image_0, NULL);
        lv_obj_set_style_translate_y(lv_image_0, 2, 0);
        lv_subject_add_observer_obj(color_label, page_system_lv_image_0_style_image_recolor_binding_cb, lv_image_0, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_0, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_1 = lv_image_create(list_row_2_value);
        lv_image_set_src(lv_image_1, icon_signal_1);
        lv_subject_add_observer_obj(subject_signal, page_system_lv_image_1_hidden_binding_cb, lv_image_1, NULL);
        lv_obj_set_style_translate_y(lv_image_1, 2, 0);
        lv_subject_add_observer_obj(color_label, page_system_lv_image_1_style_image_recolor_binding_cb, lv_image_1, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_1, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_2 = lv_image_create(list_row_2_value);
        lv_image_set_src(lv_image_2, icon_signal_2);
        lv_subject_add_observer_obj(subject_signal, page_system_lv_image_2_hidden_binding_cb, lv_image_2, NULL);
        lv_obj_set_style_translate_y(lv_image_2, 2, 0);
        lv_subject_add_observer_obj(color_label, page_system_lv_image_2_style_image_recolor_binding_cb, lv_image_2, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_2, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_3 = lv_image_create(list_row_2_value);
        lv_image_set_src(lv_image_3, icon_signal_3);
        lv_subject_add_observer_obj(subject_signal, page_system_lv_image_3_hidden_binding_cb, lv_image_3, NULL);
        lv_obj_set_style_translate_y(lv_image_3, 2, 0);
        lv_subject_add_observer_obj(color_label, page_system_lv_image_3_style_image_recolor_binding_cb, lv_image_3, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_3, (255 * 100 / 100), 0);

        lv_obj_t * lv_label_6 = lv_label_create(list_row_2_value);
        lv_subject_add_observer_obj(subject_satellites, page_system_lv_label_6_text_binding_cb, lv_label_6, NULL);
        lv_subject_add_observer_obj(subject_gps_symbol, page_system_lv_label_6_hidden_binding_cb, lv_label_6, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_2` doesn't exist");
        }

        lv_obj_t * list_row_3 = list_row_create(list_group_1_rows, "antenna", false);
        lv_obj_t * list_row_3_value = list_row_get_value(list_row_3);
        if (list_row_3_value) {
        lv_obj_t * lv_label_7 = lv_label_create(list_row_3_value);
        lv_label_set_translation_tag(lv_label_7, "antenna_unknown");
        lv_subject_add_observer_obj(subject_antenna, page_system_lv_label_7_hidden_binding_cb, lv_label_7, NULL);
        lv_subject_add_observer_obj(color_yellow, page_system_lv_label_7_style_text_color_binding_cb, lv_label_7, NULL);

        lv_obj_t * lv_label_8 = lv_label_create(list_row_3_value);
        lv_label_set_translation_tag(lv_label_8, "antenna_active_ok");
        lv_subject_add_observer_obj(subject_antenna, page_system_lv_label_8_hidden_binding_cb, lv_label_8, NULL);
        lv_subject_add_observer_obj(color_green, page_system_lv_label_8_style_text_color_binding_cb, lv_label_8, NULL);

        lv_obj_t * lv_label_9 = lv_label_create(list_row_3_value);
        lv_label_set_translation_tag(lv_label_9, "ok");
        lv_subject_add_observer_obj(subject_antenna, page_system_lv_label_9_hidden_binding_cb, lv_label_9, NULL);
        lv_subject_add_observer_obj(color_green, page_system_lv_label_9_style_text_color_binding_cb, lv_label_9, NULL);

        lv_obj_t * lv_label_10 = lv_label_create(list_row_3_value);
        lv_label_set_translation_tag(lv_label_10, "antenna_short");
        lv_subject_add_observer_obj(subject_antenna, page_system_lv_label_10_hidden_binding_cb, lv_label_10, NULL);
        lv_subject_add_observer_obj(color_red, page_system_lv_label_10_style_text_color_binding_cb, lv_label_10, NULL);

        lv_obj_t * lv_label_11 = lv_label_create(list_row_3_value);
        lv_label_set_translation_tag(lv_label_11, "antenna_disconnected");
        lv_subject_add_observer_obj(subject_antenna, page_system_lv_label_11_hidden_binding_cb, lv_label_11, NULL);
        lv_subject_add_observer_obj(color_yellow, page_system_lv_label_11_style_text_color_binding_cb, lv_label_11, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_3` doesn't exist");
        }
        } else {
        LV_LOG_WARN("`rows` slot of `list_group_1` doesn't exist");
        }

        section_header_create(left, "section_info");

        lv_obj_t * list_group_2 = list_group_create(left);
        lv_obj_t * list_group_2_rows = list_group_get_rows(list_group_2);
        if (list_group_2_rows) {
        lv_obj_t * list_row_4 = list_row_create(list_group_2_rows, "device_id", true);
        lv_obj_t * list_row_4_value = list_row_get_value(list_row_4);
        if (list_row_4_value) {
        lv_obj_t * lv_label_12 = lv_label_create(list_row_4_value);
        lv_subject_add_observer_obj(subject_device_id, page_system_lv_label_12_text_binding_cb, lv_label_12, NULL);
        lv_obj_set_style_text_font(lv_label_12, font_text, 0);
        lv_subject_add_observer_obj(color_label_2, page_system_lv_label_12_style_text_color_binding_cb, lv_label_12, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_4` doesn't exist");
        }

        lv_obj_t * list_row_5 = list_row_create(list_group_2_rows, "version", true);
        lv_obj_t * list_row_5_value = list_row_get_value(list_row_5);
        if (list_row_5_value) {
        lv_obj_t * lv_label_13 = lv_label_create(list_row_5_value);
        lv_subject_add_observer_obj(subject_version, page_system_lv_label_13_text_binding_cb, lv_label_13, NULL);
        lv_subject_add_observer_obj(color_label_2, page_system_lv_label_13_style_text_color_binding_cb, lv_label_13, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_5` doesn't exist");
        }

        lv_obj_t * licenses = list_row_create(list_group_2_rows, "licenses", false);
        lv_obj_set_name(licenses, "licenses");
        lv_obj_t * licenses_value = list_row_get_value(licenses);
        if (licenses_value) {
        chevron_create(licenses_value);
        } else {
        LV_LOG_WARN("`value` slot of `licenses` doesn't exist");
        }
        } else {
        LV_LOG_WARN("`rows` slot of `list_group_2` doesn't exist");
        }

        lv_obj_t * right = lv_obj_create(sheet);
        lv_obj_set_name(right, "right");
        lv_obj_set_flex_grow(right, 1);
        lv_obj_set_height(right, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollable(right, false);
        lv_obj_remove_theme(right, LV_PART_MAIN);
        section_header_create(right, "section_filter");

        lv_obj_t * list_group_3 = list_group_create(right);
        lv_obj_t * list_group_3_rows = list_group_get_rows(list_group_3);
        if (list_group_3_rows) {
        lv_obj_t * altitude_filter = list_row_create(list_group_3_rows, "altitude", false);
        lv_obj_set_name(altitude_filter, "altitude_filter");
        lv_obj_t * altitude_filter_value = list_row_get_value(altitude_filter);
        if (altitude_filter_value) {
        lv_obj_t * lv_label_14 = lv_label_create(altitude_filter_value);
        lv_subject_add_observer_obj(subject_altitude_filter, page_system_lv_label_14_text_binding_cb, lv_label_14, NULL);

        chevron_create(altitude_filter_value);
        } else {
        LV_LOG_WARN("`value` slot of `altitude_filter` doesn't exist");
        }
        } else {
        LV_LOG_WARN("`rows` slot of `list_group_3` doesn't exist");
        }

        section_header_create(right, "section_ownship");

        lv_obj_t * list_group_4 = list_group_create(right);
        lv_obj_t * list_group_4_rows = list_group_get_rows(list_group_4);
        if (list_group_4_rows) {
        lv_obj_t * callsign = list_row_create(list_group_4_rows, "callsign", true);
        lv_obj_set_name(callsign, "callsign");
        lv_obj_t * callsign_value = list_row_get_value(callsign);
        if (callsign_value) {
        lv_obj_t * lv_label_15 = lv_label_create(callsign_value);
        lv_subject_add_observer_obj(subject_callsign, page_system_lv_label_15_text_binding_cb, lv_label_15, NULL);

        chevron_create(callsign_value);
        } else {
        LV_LOG_WARN("`value` slot of `callsign` doesn't exist");
        }

        lv_obj_t * icao_address = list_row_create(list_group_4_rows, "icao_address", false);
        lv_obj_set_name(icao_address, "icao_address");
        lv_obj_t * icao_address_value = list_row_get_value(icao_address);
        if (icao_address_value) {
        lv_obj_t * lv_label_16 = lv_label_create(icao_address_value);
        lv_subject_add_observer_obj(subject_icao, page_system_lv_label_16_text_binding_cb, lv_label_16, NULL);

        chevron_create(icao_address_value);
        } else {
        LV_LOG_WARN("`value` slot of `icao_address` doesn't exist");
        }
        } else {
        LV_LOG_WARN("`rows` slot of `list_group_4` doesn't exist");
        }

        section_header_create(right, "section_display");

        lv_obj_t * list_group_5 = list_group_create(right);
        lv_obj_t * list_group_5_rows = list_group_get_rows(list_group_5);
        if (list_group_5_rows) {
        lv_obj_t * list_row_6 = list_row_create(list_group_5_rows, "theme", true);
        lv_obj_t * list_row_6_value = list_row_get_value(list_row_6);
        if (list_row_6_value) {
        segmented_theme_create(list_row_6_value);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_6` doesn't exist");
        }

        lv_obj_t * list_row_7 = list_row_create(list_group_5_rows, "track_history", true);
        lv_obj_t * list_row_7_value = list_row_get_value(list_row_7);
        if (list_row_7_value) {
        lv_obj_t * track_history = lv_switch_create(list_row_7_value);
        lv_obj_set_name(track_history, "track_history");
        lv_obj_set_width(track_history, 76);
        lv_obj_set_height(track_history, 44);
        lv_obj_set_style_translate_y(track_history, 1, 0);
        lv_subject_add_observer_obj(subject_track_history, page_system_track_history_checked_binding_cb, track_history, NULL);
        lv_subject_add_observer_obj(color_control, page_system_track_history_style_bg_color_binding_cb, track_history, NULL);
        lv_obj_set_style_bg_opa(track_history, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(track_history, 22, 0);
        lv_subject_add_observer_obj(color_green, page_system_track_history_style_bg_color_binding_cb_2, track_history, NULL);
        lv_obj_set_style_bg_opa(track_history, (255 * 100 / 100), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_radius(track_history, 22, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(track_history, lv_color_hex(0xffffff), LV_PART_KNOB);
        lv_obj_set_style_bg_opa(track_history, (255 * 100 / 100), LV_PART_KNOB);
        lv_obj_set_style_radius(track_history, 22, LV_PART_KNOB);
        lv_obj_set_style_pad_all(track_history, -3, LV_PART_KNOB);
        lv_obj_remove_theme(track_history, LV_PART_MAIN);
        lv_obj_remove_theme(track_history, LV_PART_INDICATOR);
        lv_obj_remove_theme(track_history, LV_PART_KNOB);
        lv_obj_add_event_cb(track_history, page_system_set_subject_cb_2, LV_EVENT_CLICKED, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_7` doesn't exist");
        }

        lv_obj_t * list_row_8 = list_row_create(list_group_5_rows, "symbols", true);
        lv_obj_t * list_row_8_value = list_row_get_value(list_row_8);
        if (list_row_8_value) {
        segmented_symbols_create(list_row_8_value);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_8` doesn't exist");
        }

        lv_obj_t * list_row_9 = list_row_create(list_group_5_rows, "conflict_indicator", false);
        lv_obj_t * list_row_9_value = list_row_get_value(list_row_9);
        if (list_row_9_value) {
        lv_obj_t * conflict_indicator = lv_switch_create(list_row_9_value);
        lv_obj_set_name(conflict_indicator, "conflict_indicator");
        lv_obj_set_width(conflict_indicator, 76);
        lv_obj_set_height(conflict_indicator, 44);
        lv_obj_set_style_translate_y(conflict_indicator, 1, 0);
        lv_subject_add_observer_obj(subject_conflict_indicator, page_system_conflict_indicator_checked_binding_cb, conflict_indicator, NULL);
        lv_subject_add_observer_obj(color_control, page_system_conflict_indicator_style_bg_color_binding_cb, conflict_indicator, NULL);
        lv_obj_set_style_bg_opa(conflict_indicator, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(conflict_indicator, 22, 0);
        lv_subject_add_observer_obj(color_green, page_system_conflict_indicator_style_bg_color_binding_cb_2, conflict_indicator, NULL);
        lv_obj_set_style_bg_opa(conflict_indicator, (255 * 100 / 100), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_radius(conflict_indicator, 22, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(conflict_indicator, lv_color_hex(0xffffff), LV_PART_KNOB);
        lv_obj_set_style_bg_opa(conflict_indicator, (255 * 100 / 100), LV_PART_KNOB);
        lv_obj_set_style_radius(conflict_indicator, 22, LV_PART_KNOB);
        lv_obj_set_style_pad_all(conflict_indicator, -3, LV_PART_KNOB);
        lv_obj_remove_theme(conflict_indicator, LV_PART_MAIN);
        lv_obj_remove_theme(conflict_indicator, LV_PART_INDICATOR);
        lv_obj_remove_theme(conflict_indicator, LV_PART_KNOB);
        lv_obj_add_event_cb(conflict_indicator, page_system_set_subject_cb_3, LV_EVENT_CLICKED, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_9` doesn't exist");
        }
        } else {
        LV_LOG_WARN("`rows` slot of `list_group_5` doesn't exist");
        }

        the_root = page_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void page_system_time_zone_automatic_checked_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_checked(obj, lv_subject_get_int(subject_time_zone_automatic) == 1);
}

static void page_system_time_zone_automatic_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_control), 0);
}

static void page_system_time_zone_automatic_style_bg_color_binding_cb_2(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_green), LV_PART_INDICATOR | LV_STATE_CHECKED);
}

static void page_system_set_subject_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_subject_set_int(subject_time_zone_automatic, !lv_subject_get_int(subject_time_zone_automatic));
}

static void page_system_time_zone_clickable_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_clickable(obj, lv_subject_get_int(subject_time_zone_automatic) == 0);
}

static void page_system_lv_label_0_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_zone));
}

static void page_system_lv_label_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_int(subject_time_zone_automatic) == 1 ? lv_subject_get_color(color_label_2) : lv_subject_get_color(color_label), 0);
}

static void page_system_time_zone_chevron_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_time_zone_automatic) == 1);
}

static void page_system_lv_label_1_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_green), 0);
}

static void page_system_lv_label_2_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps) != 0);
}

static void page_system_lv_label_2_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

static void page_system_lv_label_3_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps) != 1);
}

static void page_system_lv_label_3_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_yellow), 0);
}

static void page_system_lv_label_4_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps) != 2);
}

static void page_system_lv_label_4_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_green), 0);
}

static void page_system_lv_label_5_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps_symbol) != 5);
}

static void page_system_lv_label_5_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

static void page_system_lv_image_0_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_signal) != 0);
}

static void page_system_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

static void page_system_lv_image_1_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_signal) != 1);
}

static void page_system_lv_image_1_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

static void page_system_lv_image_2_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_signal) != 2);
}

static void page_system_lv_image_2_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

static void page_system_lv_image_3_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_signal) != 3);
}

static void page_system_lv_image_3_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

static void page_system_lv_label_6_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    char eval_buf_1[LV_XML_EVAL_STRING_BUF_SIZE];
    lv_snprintf(eval_buf_1, sizeof(eval_buf_1), "%d", (int)(lv_subject_get_int(subject_satellites)));
    lv_label_set_text(obj, eval_buf_1);
}

static void page_system_lv_label_6_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps_symbol) == 5);
}

static void page_system_lv_label_7_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_antenna) != 0);
}

static void page_system_lv_label_7_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_yellow), 0);
}

static void page_system_lv_label_8_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_antenna) != 1);
}

static void page_system_lv_label_8_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_green), 0);
}

static void page_system_lv_label_9_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_antenna) != 2);
}

static void page_system_lv_label_9_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_green), 0);
}

static void page_system_lv_label_10_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_antenna) != 3);
}

static void page_system_lv_label_10_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_red), 0);
}

static void page_system_lv_label_11_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_antenna) != 4);
}

static void page_system_lv_label_11_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_yellow), 0);
}

static void page_system_lv_label_12_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_device_id));
}

static void page_system_lv_label_12_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

static void page_system_lv_label_13_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_version));
}

static void page_system_lv_label_13_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

static void page_system_lv_label_14_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_altitude_filter));
}

static void page_system_lv_label_15_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_callsign));
}

static void page_system_lv_label_16_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_icao));
}

static void page_system_track_history_checked_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_checked(obj, lv_subject_get_int(subject_track_history) == 1);
}

static void page_system_track_history_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_control), 0);
}

static void page_system_track_history_style_bg_color_binding_cb_2(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_green), LV_PART_INDICATOR | LV_STATE_CHECKED);
}

static void page_system_set_subject_cb_2(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_subject_set_int(subject_track_history, !lv_subject_get_int(subject_track_history));
}

static void page_system_conflict_indicator_checked_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_checked(obj, lv_subject_get_int(subject_conflict_indicator) == 1);
}

static void page_system_conflict_indicator_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_control), 0);
}

static void page_system_conflict_indicator_style_bg_color_binding_cb_2(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_green), LV_PART_INDICATOR | LV_STATE_CHECKED);
}

static void page_system_set_subject_cb_3(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_subject_set_int(subject_conflict_indicator, !lv_subject_get_int(subject_conflict_indicator));
}

