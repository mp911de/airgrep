#include "home_gen.h"
#include "../airgrep_ui.h"

static void home_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_lv_obj_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_radar_area_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_bar_left_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_bar_left_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_bar_right_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_bar_right_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_no_fix_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_no_fix_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_track_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_track_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_track_style_border_width_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_lv_image_0_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_lv_image_1_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_lv_image_1_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_set_subject_cb(lv_event_t * e);
static void home_zoom_out_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_lv_image_2_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_zoom_in_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void home_lv_image_3_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * home_create(void)
{
    LV_TRACE_OBJ_CREATE("begin");

    static bool screen_created = false;
    if (screen_created) {
        LV_LOG_WARN("`home` is already initialized as a permanent screen. Returning with no change.");
        return home;
    }


    lv_obj_t * the_root = NULL;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = home;
        lv_obj_set_name_static(lv_obj_0, "home_#");
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_subject_add_observer_obj(color_bg, home_lv_obj_0_style_bg_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(color_label, home_lv_obj_0_style_text_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_text_font(lv_obj_0, font_text, 0);

        lv_obj_t * header = status_header_create(lv_obj_0);
        lv_obj_set_name(header, "header");

        lv_obj_t * radar_area = lv_obj_create(lv_obj_0);
        lv_obj_set_name(radar_area, "radar_area");
        lv_obj_set_x(radar_area, 0);
        lv_obj_set_y(radar_area, TOUCH);
        lv_obj_set_width(radar_area, 400);
        lv_obj_set_height(radar_area, 416);
        lv_obj_set_scrollable(radar_area, false);
        lv_subject_add_observer_obj(color_separator, home_radar_area_style_border_color_binding_cb, radar_area, NULL);
        lv_obj_set_style_border_width(radar_area, HAIRLINE, 0);
        lv_obj_set_style_border_side(radar_area, LV_BORDER_SIDE_RIGHT, 0);
        lv_obj_remove_theme(radar_area, LV_PART_MAIN);
        lv_obj_t * radar = radar_create(radar_area);
        lv_obj_set_name(radar, "radar");
        radar_set_range_nm(radar, 4);

        lv_obj_t * bar_left = lv_obj_create(radar_area);
        lv_obj_set_name(bar_left, "bar_left");
        lv_obj_set_width(bar_left, ALERT_STRIP_WIDTH);
        lv_obj_set_height(bar_left, lv_pct(100));
        lv_subject_add_observer_obj(subject_alert, home_bar_left_hidden_binding_cb, bar_left, NULL);
        lv_obj_set_style_bg_opa(bar_left, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(subject_alert_phase, home_bar_left_style_bg_color_binding_cb, bar_left, NULL);
        lv_subject_add_observer_obj(color_red, home_bar_left_style_bg_color_binding_cb, bar_left, NULL);
        lv_subject_add_observer_obj(color_red_dim, home_bar_left_style_bg_color_binding_cb, bar_left, NULL);
        lv_obj_remove_theme(bar_left, LV_PART_MAIN);

        lv_obj_t * bar_right = lv_obj_create(radar_area);
        lv_obj_set_name(bar_right, "bar_right");
        lv_obj_set_width(bar_right, ALERT_STRIP_WIDTH);
        lv_obj_set_height(bar_right, lv_pct(100));
        lv_obj_set_align(bar_right, LV_ALIGN_TOP_RIGHT);
        lv_subject_add_observer_obj(subject_alert, home_bar_right_hidden_binding_cb, bar_right, NULL);
        lv_obj_set_style_bg_opa(bar_right, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(subject_alert_phase, home_bar_right_style_bg_color_binding_cb, bar_right, NULL);
        lv_subject_add_observer_obj(color_red, home_bar_right_style_bg_color_binding_cb, bar_right, NULL);
        lv_subject_add_observer_obj(color_red_dim, home_bar_right_style_bg_color_binding_cb, bar_right, NULL);
        lv_obj_remove_theme(bar_right, LV_PART_MAIN);

        lv_obj_t * no_fix = lv_label_create(radar_area);
        lv_obj_set_name(no_fix, "no_fix");
        lv_label_set_translation_tag(no_fix, "no_gps_fix");
        lv_subject_add_observer_obj(subject_gps, home_no_fix_hidden_binding_cb, no_fix, NULL);
        lv_obj_set_align(no_fix, LV_ALIGN_TOP_MID);
        lv_obj_set_y(no_fix, 104);
        lv_obj_set_style_text_font(no_fix, font_title, 0);
        lv_obj_set_style_text_color(no_fix, lv_color_hex(0x000000), 0);
        lv_subject_add_observer_obj(color_yellow, home_no_fix_style_bg_color_binding_cb, no_fix, NULL);
        lv_obj_set_style_bg_opa(no_fix, (255 * 100 / 100), 0);
        lv_obj_set_style_radius(no_fix, 32, 0);
        lv_obj_set_style_pad_hor(no_fix, 20, 0);
        lv_obj_set_style_pad_top(no_fix, 6, 0);
        lv_obj_set_style_pad_bottom(no_fix, 8, 0);

        lv_obj_t * track = lv_obj_create(radar_area);
        lv_obj_set_name(track, "track");
        lv_obj_set_align(track, LV_ALIGN_TOP_RIGHT);
        lv_obj_set_x(track, -24);
        lv_obj_set_y(track, 8);
        lv_obj_set_width(track, TOUCH);
        lv_obj_set_height(track, TOUCH);
        lv_obj_set_clickable(track, true);
        lv_obj_set_scrollable(track, false);
        lv_obj_set_style_radius(track, 32, 0);
        lv_obj_set_style_bg_opa(track, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(color_control, home_track_style_bg_color_binding_cb, track, NULL);
        lv_subject_add_observer_obj(color_tint, home_track_style_border_color_binding_cb, track, NULL);
        lv_subject_add_observer_obj(subject_track_history, home_track_style_border_width_binding_cb, track, NULL);
        lv_obj_remove_theme(track, LV_PART_MAIN);
        lv_obj_add_style(track, &style_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_0 = lv_image_create(track);
        lv_image_set_src(lv_image_0, icon_track);
        lv_obj_set_align(lv_image_0, LV_ALIGN_CENTER);
        lv_subject_add_observer_obj(subject_track_history, home_lv_image_0_hidden_binding_cb, lv_image_0, NULL);
        lv_subject_add_observer_obj(color_label, home_lv_image_0_style_image_recolor_binding_cb, lv_image_0, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_0, (255 * 100 / 100), 0);

        lv_obj_t * lv_image_1 = lv_image_create(track);
        lv_image_set_src(lv_image_1, icon_track_on);
        lv_obj_set_align(lv_image_1, LV_ALIGN_CENTER);
        lv_subject_add_observer_obj(subject_track_history, home_lv_image_1_hidden_binding_cb, lv_image_1, NULL);
        lv_subject_add_observer_obj(color_tint, home_lv_image_1_style_image_recolor_binding_cb, lv_image_1, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_1, (255 * 100 / 100), 0);

        lv_obj_add_event_cb(track, home_set_subject_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t * zoom_out = lv_obj_create(radar_area);
        lv_obj_set_name(zoom_out, "zoom_out");
        lv_obj_set_align(zoom_out, LV_ALIGN_BOTTOM_LEFT);
        lv_obj_set_x(zoom_out, 24);
        lv_obj_set_y(zoom_out, -8);
        lv_obj_set_width(zoom_out, TOUCH);
        lv_obj_set_height(zoom_out, TOUCH);
        lv_obj_set_clickable(zoom_out, true);
        lv_obj_set_scrollable(zoom_out, false);
        lv_obj_set_style_radius(zoom_out, 32, 0);
        lv_obj_set_style_bg_opa(zoom_out, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(color_control, home_zoom_out_style_bg_color_binding_cb, zoom_out, NULL);
        lv_obj_remove_theme(zoom_out, LV_PART_MAIN);
        lv_obj_add_style(zoom_out, &style_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_2 = lv_image_create(zoom_out);
        lv_image_set_src(lv_image_2, icon_minus);
        lv_obj_set_align(lv_image_2, LV_ALIGN_CENTER);
        lv_subject_add_observer_obj(subject_radar_range, home_lv_image_2_style_image_recolor_binding_cb, lv_image_2, NULL);
        lv_subject_add_observer_obj(color_grid, home_lv_image_2_style_image_recolor_binding_cb, lv_image_2, NULL);
        lv_subject_add_observer_obj(color_label, home_lv_image_2_style_image_recolor_binding_cb, lv_image_2, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_2, (255 * 100 / 100), 0);

        lv_obj_add_event_cb(zoom_out, zoom_out_clicked, LV_EVENT_CLICKED, NULL);

        lv_obj_t * zoom_in = lv_obj_create(radar_area);
        lv_obj_set_name(zoom_in, "zoom_in");
        lv_obj_set_align(zoom_in, LV_ALIGN_BOTTOM_RIGHT);
        lv_obj_set_x(zoom_in, -24);
        lv_obj_set_y(zoom_in, -8);
        lv_obj_set_width(zoom_in, TOUCH);
        lv_obj_set_height(zoom_in, TOUCH);
        lv_obj_set_clickable(zoom_in, true);
        lv_obj_set_scrollable(zoom_in, false);
        lv_obj_set_style_radius(zoom_in, 32, 0);
        lv_obj_set_style_bg_opa(zoom_in, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(color_control, home_zoom_in_style_bg_color_binding_cb, zoom_in, NULL);
        lv_obj_remove_theme(zoom_in, LV_PART_MAIN);
        lv_obj_add_style(zoom_in, &style_pressed, LV_STATE_PRESSED);
        lv_obj_t * lv_image_3 = lv_image_create(zoom_in);
        lv_image_set_src(lv_image_3, icon_plus);
        lv_obj_set_align(lv_image_3, LV_ALIGN_CENTER);
        lv_subject_add_observer_obj(subject_radar_range, home_lv_image_3_style_image_recolor_binding_cb, lv_image_3, NULL);
        lv_subject_add_observer_obj(color_grid, home_lv_image_3_style_image_recolor_binding_cb, lv_image_3, NULL);
        lv_subject_add_observer_obj(color_label, home_lv_image_3_style_image_recolor_binding_cb, lv_image_3, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_3, (255 * 100 / 100), 0);

        lv_obj_add_event_cb(zoom_in, zoom_in_clicked, LV_EVENT_CLICKED, NULL);

        lv_obj_t * traffic_header = traffic_header_create(lv_obj_0);
        lv_obj_set_name(traffic_header, "traffic_header");
        lv_obj_set_x(traffic_header, 400);
        lv_obj_set_y(traffic_header, TOUCH);

        lv_obj_t * traffic = traffic_table_create(lv_obj_0);
        lv_obj_set_name(traffic, "traffic");
        lv_obj_set_x(traffic, 400);
        lv_obj_set_y(traffic, TOUCH + 32);

        lv_obj_t * page_system = page_system_create(lv_obj_0);
        lv_obj_set_name(page_system, "page_system");

        lv_obj_t * page_wifi = page_wifi_create(lv_obj_0);
        lv_obj_set_name(page_wifi, "page_wifi");

        lv_obj_t * page_licenses = page_licenses_create(lv_obj_0);
        lv_obj_set_name(page_licenses, "page_licenses");

        lv_obj_t * page_license = page_license_create(lv_obj_0);
        lv_obj_set_name(page_license, "page_license");

        lv_obj_t * page_icao = page_icao_create(lv_obj_0);
        lv_obj_set_name(page_icao, "page_icao");

        lv_obj_t * page_callsign = page_callsign_create(lv_obj_0);
        lv_obj_set_name(page_callsign, "page_callsign");

        lv_obj_t * page_time_zone = page_time_zone_create(lv_obj_0);
        lv_obj_set_name(page_time_zone, "page_time_zone");

        lv_obj_t * page_zones = page_zones_create(lv_obj_0);
        lv_obj_set_name(page_zones, "page_zones");

        lv_obj_t * page_altitude = page_altitude_create(lv_obj_0);
        lv_obj_set_name(page_altitude, "page_altitude");

        the_root = lv_obj_0;
    }
    #endif

    if (the_root) screen_created = true;

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

static void home_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_bg), 0);
}

static void home_lv_obj_0_style_text_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_text_color(obj, lv_subject_get_color(color_label), 0);
}

static void home_radar_area_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_color(color_separator), 0);
}

static void home_bar_left_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_alert) == 0);
}

static void home_bar_left_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_int(subject_alert_phase) == 0 ? lv_subject_get_color(color_red) : lv_subject_get_color(color_red_dim), 0);
}

static void home_bar_right_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_alert) == 0);
}

static void home_bar_right_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_int(subject_alert_phase) == 0 ? lv_subject_get_color(color_red) : lv_subject_get_color(color_red_dim), 0);
}

static void home_no_fix_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_gps) != 0);
}

static void home_no_fix_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_yellow), 0);
}

static void home_track_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_control), 0);
}

static void home_track_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_color(color_tint), 0);
}

static void home_track_style_border_width_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_width(obj, lv_subject_get_int(subject_track_history) == 1 ? 2 : 0, 0);
}

static void home_lv_image_0_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_track_history) == 1);
}

static void home_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_label), 0);
}

static void home_lv_image_1_hidden_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_hidden(obj, lv_subject_get_int(subject_track_history) == 0);
}

static void home_lv_image_1_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_tint), 0);
}

static void home_set_subject_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    lv_subject_set_int(subject_track_history, !lv_subject_get_int(subject_track_history));
}

static void home_zoom_out_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_control), 0);
}

static void home_lv_image_2_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_int(subject_radar_range) == 32 ? lv_subject_get_color(color_grid) : lv_subject_get_color(color_label), 0);
}

static void home_zoom_in_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_control), 0);
}

static void home_lv_image_3_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_int(subject_radar_range) == 2 ? lv_subject_get_color(color_grid) : lv_subject_get_color(color_label), 0);
}

