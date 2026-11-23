#include "page_wifi_gen.h"
#include "../airgrep_ui.h"

static void page_wifi_lv_label_0_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_1_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_1_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_2_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_2_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_2_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_3_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_3_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_4_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_4_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_qr_box_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_5_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_5_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_qr_off_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_strike_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_strike_style_outline_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_6_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_wifi_lv_label_6_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * page_wifi_create(lv_obj_t * parent)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * page_0 = page_create(parent, 2);
        lv_obj_set_name_static(page_0, "page_wifi_#");

        nav_bar_create(page_0, "wifi_battery", 2, 0);

        lv_obj_t * sheet = lv_obj_create(page_0);
        lv_obj_set_name(sheet, "sheet");
        lv_obj_set_width(sheet, lv_pct(100));
        lv_obj_set_flex_grow(sheet, 1);
        lv_obj_set_flex_flow(sheet, LV_FLEX_FLOW_ROW);
        lv_obj_set_scrollable(sheet, false);
        lv_obj_set_style_pad_column(sheet, 16, 0);
        lv_obj_remove_theme(sheet, LV_PART_MAIN);
        lv_obj_t * left = lv_obj_create(sheet);
        lv_obj_set_name(left, "left");
        lv_obj_set_flex_grow(left, 1);
        lv_obj_set_height(left, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollable(left, false);
        lv_obj_remove_theme(left, LV_PART_MAIN);
        section_header_create(left, "section_wifi");

        lv_obj_t * list_group_0 = list_group_create(left);
        lv_obj_t * list_group_0_rows = list_group_get_rows(list_group_0);
        if (list_group_0_rows) {
        lv_obj_t * list_row_0 = list_row_create(list_group_0_rows, "network", true);
        lv_obj_t * list_row_0_value = list_row_get_value(list_row_0);
        if (list_row_0_value) {
        lv_obj_t * lv_label_0 = lv_label_create(list_row_0_value);
        lv_subject_add_observer_obj(subject_wifi_network, page_wifi_lv_label_0_text_binding_cb, lv_label_0, NULL);
        lv_subject_add_observer_obj(subject_wifi, page_wifi_lv_label_0_style_text_color_binding_cb, lv_label_0, NULL);
        lv_subject_add_observer_obj(color_label, page_wifi_lv_label_0_style_text_color_binding_cb, lv_label_0, NULL);
        lv_subject_add_observer_obj(color_label_2, page_wifi_lv_label_0_style_text_color_binding_cb, lv_label_0, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_0` doesn't exist");
        }

        lv_obj_t * list_row_1 = list_row_create(list_group_0_rows, "password", false);
        lv_obj_t * list_row_1_value = list_row_get_value(list_row_1);
        if (list_row_1_value) {
        lv_obj_t * lv_label_1 = lv_label_create(list_row_1_value);
        lv_subject_add_observer_obj(subject_wifi_password, page_wifi_lv_label_1_text_binding_cb, lv_label_1, NULL);
        lv_subject_add_observer_obj(subject_wifi, page_wifi_lv_label_1_style_text_color_binding_cb, lv_label_1, NULL);
        lv_subject_add_observer_obj(color_label, page_wifi_lv_label_1_style_text_color_binding_cb, lv_label_1, NULL);
        lv_subject_add_observer_obj(color_label_2, page_wifi_lv_label_1_style_text_color_binding_cb, lv_label_1, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_1` doesn't exist");
        }
        } else {
        LV_LOG_WARN("`rows` slot of `list_group_0` doesn't exist");
        }

        section_header_create(left, "section_battery");

        lv_obj_t * list_group_1 = list_group_create(left);
        lv_obj_t * list_group_1_rows = list_group_get_rows(list_group_1);
        if (list_group_1_rows) {
        lv_obj_t * list_row_2 = list_row_create(list_group_1_rows, "charge", true);
        lv_obj_t * list_row_2_value = list_row_get_value(list_row_2);
        if (list_row_2_value) {
        lv_obj_t * lv_label_2 = lv_label_create(list_row_2_value);
        lv_subject_add_observer_obj(subject_battery, page_wifi_lv_label_2_text_binding_cb, lv_label_2, NULL);
        lv_subject_add_observer_obj(subject_battery_state, page_wifi_lv_label_2_hidden_binding_cb, lv_label_2, NULL);
        lv_subject_add_observer_obj(subject_battery, page_wifi_lv_label_2_style_text_color_binding_cb, lv_label_2, NULL);
        lv_subject_add_observer_obj(color_red, page_wifi_lv_label_2_style_text_color_binding_cb, lv_label_2, NULL);
        lv_subject_add_observer_obj(color_label, page_wifi_lv_label_2_style_text_color_binding_cb, lv_label_2, NULL);

        lv_obj_t * lv_label_3 = lv_label_create(list_row_2_value);
        lv_label_set_translation_tag(lv_label_3, "no_battery");
        lv_subject_add_observer_obj(subject_battery_state, page_wifi_lv_label_3_hidden_binding_cb, lv_label_3, NULL);
        lv_subject_add_observer_obj(color_label_2, page_wifi_lv_label_3_style_text_color_binding_cb, lv_label_3, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_2` doesn't exist");
        }

        lv_obj_t * list_row_3 = list_row_create(list_group_1_rows, "remaining", false);
        lv_obj_t * list_row_3_value = list_row_get_value(list_row_3);
        if (list_row_3_value) {
        lv_obj_t * lv_label_4 = lv_label_create(list_row_3_value);
        lv_subject_add_observer_obj(subject_battery_remaining, page_wifi_lv_label_4_text_binding_cb, lv_label_4, NULL);
        lv_subject_add_observer_obj(subject_battery_state, page_wifi_lv_label_4_style_text_color_binding_cb, lv_label_4, NULL);
        lv_subject_add_observer_obj(color_label_2, page_wifi_lv_label_4_style_text_color_binding_cb, lv_label_4, NULL);
        lv_subject_add_observer_obj(color_label, page_wifi_lv_label_4_style_text_color_binding_cb, lv_label_4, NULL);
        } else {
        LV_LOG_WARN("`value` slot of `list_row_3` doesn't exist");
        }
        } else {
        LV_LOG_WARN("`rows` slot of `list_group_1` doesn't exist");
        }

        lv_obj_t * join = lv_obj_create(sheet);
        lv_obj_set_name(join, "join");
        lv_obj_set_width(join, 264);
        lv_obj_set_height(join, lv_pct(100));
        lv_obj_set_flex_flow(join, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollable(join, false);
        lv_obj_set_style_flex_cross_place(join, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_track_place(join, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(join, 24, 0);
        lv_obj_set_style_pad_row(join, 10, 0);
        lv_obj_remove_theme(join, LV_PART_MAIN);
        lv_obj_t * qr_box = lv_obj_create(join);
        lv_obj_set_name(qr_box, "qr_box");
        lv_obj_set_width(qr_box, 232);
        lv_obj_set_height(qr_box, 232);
        lv_subject_add_observer_obj(subject_wifi, page_wifi_qr_box_hidden_binding_cb, qr_box, NULL);
        lv_obj_set_scrollable(qr_box, false);
        lv_obj_set_style_pad_all(qr_box, 12, 0);
        lv_obj_set_style_radius(qr_box, RADIUS, 0);
        lv_obj_set_style_bg_color(qr_box, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_opa(qr_box, (255 * 100 / 100), 0);
        lv_obj_remove_theme(qr_box, LV_PART_MAIN);
        lv_obj_t * wifi_qr = lv_qrcode_create(qr_box);
        lv_obj_set_name(wifi_qr, "wifi_qr");
        lv_qrcode_set_size(wifi_qr, 165);
        lv_obj_set_align(wifi_qr, LV_ALIGN_CENTER);
        lv_qrcode_set_dark_color(wifi_qr, lv_color_hex(0x000000));
        lv_qrcode_set_light_color(wifi_qr, lv_color_hex(0xffffff));
        lv_qrcode_set_quiet_zone(wifi_qr, false);

        lv_obj_t * lv_label_5 = lv_label_create(join);
        lv_label_set_translation_tag(lv_label_5, "scan_to_join");
        lv_subject_add_observer_obj(subject_wifi, page_wifi_lv_label_5_hidden_binding_cb, lv_label_5, NULL);
        lv_obj_set_style_text_font(lv_label_5, font_small, 0);
        lv_subject_add_observer_obj(color_label_2, page_wifi_lv_label_5_style_text_color_binding_cb, lv_label_5, NULL);

        lv_obj_t * qr_off = lv_obj_create(join);
        lv_obj_set_name(qr_off, "qr_off");
        lv_obj_set_width(qr_off, 232);
        lv_obj_set_height(qr_off, 232);
        lv_subject_add_observer_obj(subject_wifi, page_wifi_qr_off_hidden_binding_cb, qr_off, NULL);
        lv_obj_set_scrollable(qr_off, false);
        lv_obj_remove_theme(qr_off, LV_PART_MAIN);
        lv_obj_t * lv_image_0 = lv_image_create(qr_off);
        lv_image_set_src(lv_image_0, icon_qrcode);
        lv_obj_set_align(lv_image_0, LV_ALIGN_CENTER);
        lv_subject_add_observer_obj(color_grid, page_wifi_lv_image_0_style_image_recolor_binding_cb, lv_image_0, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_0, (255 * 100 / 100), 0);

        lv_obj_t * strike = lv_obj_create(qr_off);
        lv_obj_set_name(strike, "strike");
        lv_obj_set_width(strike, 210);
        lv_obj_set_height(strike, 10);
        lv_obj_set_align(strike, LV_ALIGN_CENTER);
        lv_obj_set_style_radius(strike, 5, 0);
        lv_subject_add_observer_obj(color_grid, page_wifi_strike_style_bg_color_binding_cb, strike, NULL);
        lv_obj_set_style_bg_opa(strike, (255 * 100 / 100), 0);
        lv_obj_set_style_outline_width(strike, 6, 0);
        lv_subject_add_observer_obj(color_bg_grouped, page_wifi_strike_style_outline_color_binding_cb, strike, NULL);
        lv_obj_set_style_transform_pivot_x(strike, 105, 0);
        lv_obj_set_style_transform_pivot_y(strike, 5, 0);
        lv_obj_set_style_transform_rotation(strike, -450, 0);
        lv_obj_remove_theme(strike, LV_PART_MAIN);

        lv_obj_t * lv_label_6 = lv_label_create(join);
        lv_label_set_translation_tag(lv_label_6, "wifi_off");
        lv_subject_add_observer_obj(subject_wifi, page_wifi_lv_label_6_hidden_binding_cb, lv_label_6, NULL);
        lv_obj_set_style_text_font(lv_label_6, font_small, 0);
        lv_subject_add_observer_obj(color_label_2, page_wifi_lv_label_6_style_text_color_binding_cb, lv_label_6, NULL);

        the_root = page_0;
    }
    #endif

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void page_wifi_lv_label_0_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_wifi_network));
}

static void page_wifi_lv_label_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_int(subject_wifi) == 1 ? lv_subject_get_color(color_label) : lv_subject_get_color(color_label_2), 0);
}

static void page_wifi_lv_label_1_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_wifi_password));
}

static void page_wifi_lv_label_1_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_int(subject_wifi) == 1 ? lv_subject_get_color(color_label) : lv_subject_get_color(color_label_2), 0);
}

static void page_wifi_lv_label_2_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    char eval_buf_1[LV_XML_EVAL_STRING_BUF_SIZE];
    lv_snprintf(eval_buf_1, sizeof(eval_buf_1), "%d%s", (int)(lv_subject_get_int(subject_battery)), " %");
    lv_label_set_text(obj, eval_buf_1);
}

static void page_wifi_lv_label_2_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_battery_state) == 0);
}

static void page_wifi_lv_label_2_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_int(subject_battery) < 20 ? lv_subject_get_color(color_red) : lv_subject_get_color(color_label), 0);
}

static void page_wifi_lv_label_3_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_battery_state) != 0);
}

static void page_wifi_lv_label_3_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

static void page_wifi_lv_label_4_text_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_label_set_text(obj, lv_subject_get_string(subject_battery_remaining));
}

static void page_wifi_lv_label_4_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_int(subject_battery_state) == 0 ? lv_subject_get_color(color_label_2) : lv_subject_get_color(color_label), 0);
}

static void page_wifi_qr_box_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_wifi) == 0);
}

static void page_wifi_lv_label_5_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_wifi) == 0);
}

static void page_wifi_lv_label_5_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

static void page_wifi_qr_off_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_wifi) == 1);
}

static void page_wifi_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_grid), 0);
}

static void page_wifi_strike_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_grid), 0);
}

static void page_wifi_strike_style_outline_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_outline_color(obj, lv_subject_get_color(color_bg_grouped), 0);
}

static void page_wifi_lv_label_6_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_wifi) == 1);
}

static void page_wifi_lv_label_6_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label_2), 0);
}

