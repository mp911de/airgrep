#include "airgrep_ui.h"

#if defined(LV_USE_XML) && LV_USE_XML
#include "widgets/entry_field/entry_field_private_gen.h"
#include "widgets/keyboard/keyboard_private_gen.h"
#include "widgets/logo/logo_private_gen.h"
#include "widgets/radar/radar_private_gen.h"
#include "widgets/traffic_table/traffic_table_private_gen.h"
#endif

static void check_font(lv_font_t ** font, const char * name);

static uint32_t airgrep_ui_target = AIRGREP_UI_COMPILE_TARGET;

#ifndef LV_EDITOR_PREVIEW
    static const char * translation_languages[] = {"en", "de", NULL};
    static const char * translation_tags[] = {"no_gps_fix", "system", "section_time_zone", "automatic", "time_zone", "section_gps", "fix", "satellites", "antenna", "antenna_unknown", "antenna_active_ok", "antenna_short", "antenna_disconnected", "section_health", "all_systems", "ok", "section_display", "theme", "light", "dark", "track_history", "symbols", "planes", "simple", "conflict_indicator", "section_filter", "altitude", "altitude_zero", "section_ownship", "callsign", "icao_address", "not_set", "section_info", "device_id", "version", "licenses", "section_quick_access", "section_regions", "detected", "section_components", "wifi_battery", "section_wifi", "network", "password", "section_battery", "charge", "remaining", "no_battery", "charging", "until_full", "fully_charged", "scan_to_join", "wifi_off", "mute", "unmute", "own_ship", "beyond", "i2c", "adsbee", "barometer", "usb_c", "cpu", "oom", "gps_receiver", "gps_antenna", "charger", "battery", "memory", "unresponsive", "overvoltage", "overheated", "backup_battery", "battery_low", "too_cold", NULL};
    static const char * translation_texts[] = {
        "NO GPS FIX", "KEIN GPS-FIX", /* no_gps_fix */
        "System", "System", /* system */
        "TIME ZONE", "ZEITZONE", /* section_time_zone */
        "Automatic", "Automatisch", /* automatic */
        "Time zone", "Zeitzone", /* time_zone */
        "GPS", "GPS", /* section_gps */
        "Fix", "Fix", /* fix */
        "Satellites", "Satelliten", /* satellites */
        "Antenna", "Antenne", /* antenna */
        "Unknown", "Unbekannt", /* antenna_unknown */
        "Active, OK", "Aktiv, OK", /* antenna_active_ok */
        "Short", "Kurzschluss", /* antenna_short */
        "Disconnected", "Getrennt", /* antenna_disconnected */
        "HEALTH", "ZUSTAND", /* section_health */
        "System", "System", /* all_systems */
        "OK", "OK", /* ok */
        "DISPLAY", "ANZEIGE", /* section_display */
        "Theme", "Design", /* theme */
        "Light", "Hell", /* light */
        "Dark", "Dunkel", /* dark */
        "Track history", "Flugstrecke", /* track_history */
        "Symbols", "Symbole", /* symbols */
        "Planes", "Flugzeuge", /* planes */
        "Simple", "Einfach", /* simple */
        "Conflict Indicator", "Conflict Indicator", /* conflict_indicator */
        "FILTER", "FILTER", /* section_filter */
        "Altitude", "Vertikal", /* altitude */
        "0 ft shows traffic at every altitude.", "0 ft zeigt Verkehr in jeder Höhe.", /* altitude_zero */
        "OWNSHIP", "EIGENES FLUGZEUG", /* section_ownship */
        "Callsign", "Rufzeichen", /* callsign */
        "ICAO address", "ICAO-Adresse", /* icao_address */
        "Not set", "Nicht gesetzt", /* not_set */
        "INFO", "INFO", /* section_info */
        "Device ID", "Geräte-ID", /* device_id */
        "Version", "Version", /* version */
        "Licenses", "Lizenzen", /* licenses */
        "QUICK ACCESS", "SCHNELLZUGRIFF", /* section_quick_access */
        "REGIONS", "REGIONEN", /* section_regions */
        "Detected", "Erkannt", /* detected */
        "COMPONENTS", "KOMPONENTEN", /* section_components */
        "Wi-Fi and Battery", "WLAN und Akku", /* wifi_battery */
        "WI-FI", "WLAN", /* section_wifi */
        "Network", "Netzwerk", /* network */
        "Password", "Passwort", /* password */
        "BATTERY", "AKKU", /* section_battery */
        "Charge", "Ladung", /* charge */
        "Remaining", "Restzeit", /* remaining */
        "No battery", "Kein Akku", /* no_battery */
        "Charging", "Lädt", /* charging */
        "h until full", "h bis voll", /* until_full */
        "Fully charged", "Voll geladen", /* fully_charged */
        "Scan to join", "Scannen zum Verbinden", /* scan_to_join */
        "Wi-Fi off", "WLAN aus", /* wifi_off */
        "Mute", "Stumm", /* mute */
        "Unmute", "Laut", /* unmute */
        "Own ship", "Ownship", /* own_ship */
        "BEYOND", "ÜBER", /* beyond */
        "I2C", "I2C", /* i2c */
        "ADSBee", "ADSBee", /* adsbee */
        "Barometer", "Barometer", /* barometer */
        "USB-C", "USB-C", /* usb_c */
        "CPU", "CPU", /* cpu */
        "OOM", "OOM", /* oom */
        "GPS receiver", "GPS-Empfänger", /* gps_receiver */
        "GPS antenna", "GPS-Antenne", /* gps_antenna */
        "Charger", "Ladegerät", /* charger */
        "Battery", "Akku", /* battery */
        "Memory", "Speicher", /* memory */
        "Unresponsive", "Kein Signal", /* unresponsive */
        "Overvoltage", "Überspannung", /* overvoltage */
        "Overheated", "Überhitzt", /* overheated */
        "Backup battery", "Batterie", /* backup_battery */
        "Low", "Schwach", /* battery_low */
        "Too cold", "Zu kalt", /* too_cold */
    };
#endif

lv_obj_t * home = NULL;

lv_font_t * font_label;
extern lv_font_t font_label_data;
lv_font_t * font_small;
extern lv_font_t font_small_data;
lv_font_t * font_text;
extern lv_font_t font_text_data;
lv_font_t * font_title;
extern lv_font_t font_title_data;
lv_font_t * font_data;
extern lv_font_t font_data_data;
lv_font_t * font_display;
extern lv_font_t font_display_data;
lv_font_t * font_stored;
extern lv_font_t font_stored_data;

const void * symbol_light = NULL;
extern const void * symbol_light_data;
const void * symbol_airliner = NULL;
extern const void * symbol_airliner_data;
const void * symbol_glider = NULL;
extern const void * symbol_glider_data;
const void * symbol_helicopter = NULL;
extern const void * symbol_helicopter_data;
const void * symbol_balloon = NULL;
extern const void * symbol_balloon_data;
const void * symbol_unknown = NULL;
extern const void * symbol_unknown_data;
const void * symbol_simple = NULL;
extern const void * symbol_simple_data;
const void * symbol_paper_plane = NULL;
extern const void * symbol_paper_plane_data;
const void * icon_gps_3d = NULL;
extern const void * icon_gps_3d_data;
const void * icon_gps_2d = NULL;
extern const void * icon_gps_2d_data;
const void * icon_gps_search = NULL;
extern const void * icon_gps_search_data;
const void * icon_gps_antenna = NULL;
extern const void * icon_gps_antenna_data;
const void * icon_gps_fault = NULL;
extern const void * icon_gps_fault_data;
const void * icon_wifi = NULL;
extern const void * icon_wifi_data;
const void * icon_wifi_off = NULL;
extern const void * icon_wifi_off_data;
const void * icon_speaker = NULL;
extern const void * icon_speaker_data;
const void * icon_speaker_off = NULL;
extern const void * icon_speaker_off_data;
const void * icon_plus = NULL;
extern const void * icon_plus_data;
const void * icon_minus = NULL;
extern const void * icon_minus_data;
const void * icon_track = NULL;
extern const void * icon_track_data;
const void * icon_track_on = NULL;
extern const void * icon_track_on_data;
const void * icon_bell_slash = NULL;
extern const void * icon_bell_slash_data;
const void * icon_bell = NULL;
extern const void * icon_bell_data;
const void * icon_qrcode = NULL;
extern const void * icon_qrcode_data;
const void * icon_chevron_left = NULL;
extern const void * icon_chevron_left_data;
const void * icon_chevron_right = NULL;
extern const void * icon_chevron_right_data;
const void * icon_delete = NULL;
extern const void * icon_delete_data;
const void * icon_check = NULL;
extern const void * icon_check_data;
const void * icon_chosen = NULL;
extern const void * icon_chosen_data;
const void * icon_bolt = NULL;
extern const void * icon_bolt_data;
const void * icon_bolt_cut = NULL;
extern const void * icon_bolt_cut_data;
const void * icon_bolt_line = NULL;
extern const void * icon_bolt_line_data;
const void * icon_battery_slash = NULL;
extern const void * icon_battery_slash_data;
const void * icon_battery_slash_cut = NULL;
extern const void * icon_battery_slash_cut_data;
const void * logo_air = NULL;
extern const void * logo_air_data;
const void * logo_grep = NULL;
extern const void * logo_grep_data;
const void * logo_plane = NULL;
extern const void * logo_plane_data;
const void * icon_signal_0 = NULL;
extern const void * icon_signal_0_data;
const void * icon_signal_1 = NULL;
extern const void * icon_signal_1_data;
const void * icon_signal_2 = NULL;
extern const void * icon_signal_2_data;
const void * icon_signal_3 = NULL;
extern const void * icon_signal_3_data;

lv_style_t style_pressed;
lv_style_t style_pressed_clear;

lv_subject_t * color_bg;
lv_subject_t * color_bg_grouped;
lv_subject_t * color_surface;
lv_subject_t * color_separator;
lv_subject_t * color_label;
lv_subject_t * color_label_2;
lv_subject_t * color_tint;
lv_subject_t * color_green;
lv_subject_t * color_red;
lv_subject_t * color_red_dim;
lv_subject_t * color_yellow;
lv_subject_t * color_grid;
lv_subject_t * color_control;
lv_subject_t * color_on_green;
lv_subject_t * color_logo_dot;
lv_subject_t * color_pill;
lv_subject_t * color_pill_label;
lv_subject_t * color_pill_flarm;
lv_subject_t * color_pill_flarm_label;
lv_subject_t * subject_time;
lv_subject_t * subject_health;
lv_subject_t * subject_battery;
lv_subject_t * subject_battery_state;
lv_subject_t * subject_antenna;
lv_subject_t * subject_gps;
lv_subject_t * subject_gps_symbol;
lv_subject_t * subject_wifi;
lv_subject_t * subject_muted;
lv_subject_t * subject_page;
lv_subject_t * subject_page_opening;
lv_subject_t * subject_page_closing;
lv_subject_t * subject_time_zone;
lv_subject_t * subject_wifi_network;
lv_subject_t * subject_wifi_password;
lv_subject_t * subject_battery_remaining;
lv_subject_t * subject_satellites;
lv_subject_t * subject_signal;
lv_subject_t * subject_theme_dark;
lv_subject_t * subject_track_history;
lv_subject_t * subject_simple_symbols;
lv_subject_t * subject_conflict_indicator;
lv_subject_t * subject_time_zone_automatic;
lv_subject_t * subject_zone;
lv_subject_t * subject_radar_range;
lv_subject_t * subject_icao;
lv_subject_t * subject_callsign;
lv_subject_t * subject_altitude_filter;
lv_subject_t * subject_device_id;
lv_subject_t * subject_version;
lv_subject_t * subject_alert_phase;
lv_subject_t * subject_alert;

void airgrep_ui_init_gen(const char * asset_path)
{
    /* When running from the editor the theme set from the XML should overwrite this */
#if !defined(LV_EDITOR_PREVIEW)
#if LV_USE_THEME_SIMPLE
    lv_display_t * disp = lv_display_get_default();
    lv_theme_t * th = lv_theme_simple_init(disp);
    lv_display_set_theme(disp, th);
#else
    LV_LOG_WARN("Simple theme is selected in project.xml but LV_USE_THEME_SIMPLE is disabled");
#endif
#endif


    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        if (!font_label) {
            font_label = &font_label_data;

        }
        if (!font_small) {
            font_small = &font_small_data;

        }
        if (!font_text) {
            font_text = &font_text_data;

        }
        if (!font_title) {
            font_title = &font_title_data;

        }
        if (!font_data) {
            font_data = &font_data_data;

        }
        if (!font_display) {
            font_display = &font_display_data;

        }
        if (!font_stored) {
            font_stored = &font_stored_data;

        }
    }
    #endif

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        if (!symbol_light) {
            symbol_light = &symbol_light_data;
        }
        if (!symbol_airliner) {
            symbol_airliner = &symbol_airliner_data;
        }
        if (!symbol_glider) {
            symbol_glider = &symbol_glider_data;
        }
        if (!symbol_helicopter) {
            symbol_helicopter = &symbol_helicopter_data;
        }
        if (!symbol_balloon) {
            symbol_balloon = &symbol_balloon_data;
        }
        if (!symbol_unknown) {
            symbol_unknown = &symbol_unknown_data;
        }
        if (!symbol_simple) {
            symbol_simple = &symbol_simple_data;
        }
        if (!symbol_paper_plane) {
            symbol_paper_plane = &symbol_paper_plane_data;
        }
        if (!icon_gps_3d) {
            icon_gps_3d = &icon_gps_3d_data;
        }
        if (!icon_gps_2d) {
            icon_gps_2d = &icon_gps_2d_data;
        }
        if (!icon_gps_search) {
            icon_gps_search = &icon_gps_search_data;
        }
        if (!icon_gps_antenna) {
            icon_gps_antenna = &icon_gps_antenna_data;
        }
        if (!icon_gps_fault) {
            icon_gps_fault = &icon_gps_fault_data;
        }
        if (!icon_wifi) {
            icon_wifi = &icon_wifi_data;
        }
        if (!icon_wifi_off) {
            icon_wifi_off = &icon_wifi_off_data;
        }
        if (!icon_speaker) {
            icon_speaker = &icon_speaker_data;
        }
        if (!icon_speaker_off) {
            icon_speaker_off = &icon_speaker_off_data;
        }
        if (!icon_plus) {
            icon_plus = &icon_plus_data;
        }
        if (!icon_minus) {
            icon_minus = &icon_minus_data;
        }
        if (!icon_track) {
            icon_track = &icon_track_data;
        }
        if (!icon_track_on) {
            icon_track_on = &icon_track_on_data;
        }
        if (!icon_bell_slash) {
            icon_bell_slash = &icon_bell_slash_data;
        }
        if (!icon_bell) {
            icon_bell = &icon_bell_data;
        }
        if (!icon_qrcode) {
            icon_qrcode = &icon_qrcode_data;
        }
        if (!icon_chevron_left) {
            icon_chevron_left = &icon_chevron_left_data;
        }
        if (!icon_chevron_right) {
            icon_chevron_right = &icon_chevron_right_data;
        }
        if (!icon_delete) {
            icon_delete = &icon_delete_data;
        }
        if (!icon_check) {
            icon_check = &icon_check_data;
        }
        if (!icon_chosen) {
            icon_chosen = &icon_chosen_data;
        }
        if (!icon_bolt) {
            icon_bolt = &icon_bolt_data;
        }
        if (!icon_bolt_cut) {
            icon_bolt_cut = &icon_bolt_cut_data;
        }
        if (!icon_bolt_line) {
            icon_bolt_line = &icon_bolt_line_data;
        }
        if (!icon_battery_slash) {
            icon_battery_slash = &icon_battery_slash_data;
        }
        if (!icon_battery_slash_cut) {
            icon_battery_slash_cut = &icon_battery_slash_cut_data;
        }
        if (!logo_air) {
            logo_air = &logo_air_data;
        }
        if (!logo_grep) {
            logo_grep = &logo_grep_data;
        }
        if (!logo_plane) {
            logo_plane = &logo_plane_data;
        }
        if (!icon_signal_0) {
            icon_signal_0 = &icon_signal_0_data;
        }
        if (!icon_signal_1) {
            icon_signal_1 = &icon_signal_1_data;
        }
        if (!icon_signal_2) {
            icon_signal_2 = &icon_signal_2_data;
        }
        if (!icon_signal_3) {
            icon_signal_3 = &icon_signal_3_data;
        }
    }
    #endif

    static bool style_inited = false;

    if (!style_inited) {
        lv_style_init(&style_pressed);
        lv_style_init(&style_pressed_clear);

        lv_style_set_recolor(&style_pressed, PRESSED);
        lv_style_set_recolor_opa(&style_pressed, PRESSED_OPACITY_U8);
        lv_style_set_recolor(&style_pressed_clear, PRESSED);
        lv_style_set_recolor_opa(&style_pressed_clear, PRESSED_OPACITY_U8);
        lv_style_set_bg_color(&style_pressed_clear, PRESSED);
        lv_style_set_bg_opa(&style_pressed_clear, PRESSED_OPACITY_U8);

        style_inited = true;
    }

    color_bg = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_bg, lv_color_hex(0xffffff));
    color_bg_grouped = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_bg_grouped, lv_color_hex(0xefeff4));
    color_surface = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_surface, lv_color_hex(0xffffff));
    color_separator = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_separator, lv_color_hex(0xc8c7cc));
    color_label = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_label, lv_color_hex(0x000000));
    color_label_2 = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_label_2, lv_color_hex(0x6d6d72));
    color_tint = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_tint, lv_color_hex(0x007aff));
    color_green = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_green, lv_color_hex(0x34c759));
    color_red = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_red, lv_color_hex(0xff3b30));
    color_red_dim = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_red_dim, lv_color_hex(0xf6b1ac));
    color_yellow = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_yellow, lv_color_hex(0xffcc00));
    color_grid = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_grid, lv_color_hex(0xaeaeb2));
    color_control = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_control, lv_color_hex(0xe5e5ea));
    color_on_green = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_on_green, lv_color_hex(0xffffff));
    color_logo_dot = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_logo_dot, lv_color_hex(0xaeaeb2));
    color_pill = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_pill, lv_color_hex(0xe5e5ea));
    color_pill_label = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_pill_label, lv_color_hex(0x3a3a3c));
    color_pill_flarm = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_pill_flarm, lv_color_hex(0xd6eaff));
    color_pill_flarm_label = lv_subject_create(LV_SUBJECT_TYPE_COLOR);
    lv_subject_set_color(color_pill_flarm_label, lv_color_hex(0x0a4a8a));
    static char subject_time_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_time_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_time = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_time, subject_time_buf, subject_time_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_time, "");
    subject_health = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_health, 0);
    lv_subject_set_max_value_int(subject_health, 2);
    lv_subject_set_int(subject_health, 0);
    subject_battery = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_battery, 0);
    lv_subject_set_max_value_int(subject_battery, 100);
    lv_subject_set_int(subject_battery, 0);
    subject_battery_state = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_battery_state, 0);
    lv_subject_set_max_value_int(subject_battery_state, 3);
    lv_subject_set_int(subject_battery_state, 0);
    subject_antenna = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_antenna, 0);
    lv_subject_set_max_value_int(subject_antenna, 4);
    lv_subject_set_int(subject_antenna, 0);
    subject_gps = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_gps, 0);
    lv_subject_set_max_value_int(subject_gps, 2);
    lv_subject_set_int(subject_gps, 0);
    subject_gps_symbol = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_gps_symbol, 0);
    lv_subject_set_max_value_int(subject_gps_symbol, 5);
    lv_subject_set_int(subject_gps_symbol, 0);
    subject_wifi = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_wifi, 0);
    lv_subject_set_max_value_int(subject_wifi, 1);
    lv_subject_set_int(subject_wifi, 0);
    subject_muted = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_muted, 0);
    lv_subject_set_max_value_int(subject_muted, 1);
    lv_subject_set_int(subject_muted, 0);
    subject_page = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_page, 0);
    lv_subject_set_max_value_int(subject_page, 9);
    lv_subject_set_int(subject_page, 0);
    subject_page_opening = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_page_opening, 0);
    lv_subject_set_max_value_int(subject_page_opening, 9);
    lv_subject_set_int(subject_page_opening, 0);
    subject_page_closing = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_page_closing, 0);
    lv_subject_set_max_value_int(subject_page_closing, 9);
    lv_subject_set_int(subject_page_closing, 0);
    static char subject_time_zone_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_time_zone_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_time_zone = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_time_zone, subject_time_zone_buf, subject_time_zone_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_time_zone, "");
    static char subject_wifi_network_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_wifi_network_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_wifi_network = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_wifi_network, subject_wifi_network_buf, subject_wifi_network_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_wifi_network, "");
    static char subject_wifi_password_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_wifi_password_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_wifi_password = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_wifi_password, subject_wifi_password_buf, subject_wifi_password_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_wifi_password, "");
    static char subject_battery_remaining_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_battery_remaining_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_battery_remaining = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_battery_remaining, subject_battery_remaining_buf, subject_battery_remaining_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_battery_remaining, "");
    subject_satellites = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_satellites, 0);
    lv_subject_set_max_value_int(subject_satellites, 99);
    lv_subject_set_int(subject_satellites, 0);
    subject_signal = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_signal, -1);
    lv_subject_set_max_value_int(subject_signal, 3);
    lv_subject_set_int(subject_signal, 0);
    subject_theme_dark = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_theme_dark, 0);
    lv_subject_set_max_value_int(subject_theme_dark, 1);
    lv_subject_set_int(subject_theme_dark, 0);
    subject_track_history = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_track_history, 0);
    lv_subject_set_max_value_int(subject_track_history, 1);
    lv_subject_set_int(subject_track_history, 0);
    subject_simple_symbols = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_simple_symbols, 0);
    lv_subject_set_max_value_int(subject_simple_symbols, 1);
    lv_subject_set_int(subject_simple_symbols, 0);
    subject_conflict_indicator = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_conflict_indicator, 0);
    lv_subject_set_max_value_int(subject_conflict_indicator, 1);
    lv_subject_set_int(subject_conflict_indicator, 0);
    subject_time_zone_automatic = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_time_zone_automatic, 0);
    lv_subject_set_max_value_int(subject_time_zone_automatic, 1);
    lv_subject_set_int(subject_time_zone_automatic, 1);
    static char subject_zone_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_zone_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_zone = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_zone, subject_zone_buf, subject_zone_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_zone, "");
    subject_radar_range = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_radar_range, 2);
    lv_subject_set_max_value_int(subject_radar_range, 32);
    lv_subject_set_int(subject_radar_range, 4);
    static char subject_icao_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_icao_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_icao = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_icao, subject_icao_buf, subject_icao_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_icao, "");
    static char subject_callsign_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_callsign_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_callsign = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_callsign, subject_callsign_buf, subject_callsign_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_callsign, "");
    static char subject_altitude_filter_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_altitude_filter_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_altitude_filter = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_altitude_filter, subject_altitude_filter_buf, subject_altitude_filter_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_altitude_filter, "");
    static char subject_device_id_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_device_id_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_device_id = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_device_id, subject_device_id_buf, subject_device_id_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_device_id, "");
    static char subject_version_buf[UI_SUBJECT_STRING_LENGTH];
    static char subject_version_prev_buf[UI_SUBJECT_STRING_LENGTH];
    subject_version = lv_subject_create(LV_SUBJECT_TYPE_STRING);
    lv_subject_set_string_buffer_static(subject_version, subject_version_buf, subject_version_prev_buf, UI_SUBJECT_STRING_LENGTH);
    lv_subject_set_string(subject_version, "");
    subject_alert_phase = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_alert_phase, 0);
    lv_subject_set_max_value_int(subject_alert_phase, 1);
    lv_subject_set_int(subject_alert_phase, 0);
    subject_alert = lv_subject_create(LV_SUBJECT_TYPE_INT);
    lv_subject_set_min_value_int(subject_alert, 0);
    lv_subject_set_max_value_int(subject_alert, 1);
    lv_subject_set_int(subject_alert, 0);

    #ifndef LV_EDITOR_PREVIEW
        lv_translation_add_static(translation_languages, translation_tags, translation_texts);
        lv_translation_set_language(translation_languages[0]);
    #endif

#if defined(LV_USE_XML) && LV_USE_XML
    entry_field_register();
    keyboard_register();
    logo_register();
    radar_register();
    traffic_table_register();

    /* Check all fonts / default if needed. This prevents fonts that are used in one target but
       defined in another from causing assertion failures during rendering of the Preview. */
    check_font(&font_label, "font_label");
    check_font(&font_small, "font_small");
    check_font(&font_text, "font_text");
    check_font(&font_title, "font_title");
    check_font(&font_data, "font_data");
    check_font(&font_display, "font_display");
    check_font(&font_stored, "font_stored");

    lv_xml_register_font(NULL, "font_label", font_label);
    lv_xml_register_font(NULL, "font_small", font_small);
    lv_xml_register_font(NULL, "font_text", font_text);
    lv_xml_register_font(NULL, "font_title", font_title);
    lv_xml_register_font(NULL, "font_data", font_data);
    lv_xml_register_font(NULL, "font_display", font_display);
    lv_xml_register_font(NULL, "font_stored", font_stored);

    lv_xml_register_subject(NULL, "color_bg", color_bg);
    lv_xml_register_subject(NULL, "color_bg_grouped", color_bg_grouped);
    lv_xml_register_subject(NULL, "color_surface", color_surface);
    lv_xml_register_subject(NULL, "color_separator", color_separator);
    lv_xml_register_subject(NULL, "color_label", color_label);
    lv_xml_register_subject(NULL, "color_label_2", color_label_2);
    lv_xml_register_subject(NULL, "color_tint", color_tint);
    lv_xml_register_subject(NULL, "color_green", color_green);
    lv_xml_register_subject(NULL, "color_red", color_red);
    lv_xml_register_subject(NULL, "color_red_dim", color_red_dim);
    lv_xml_register_subject(NULL, "color_yellow", color_yellow);
    lv_xml_register_subject(NULL, "color_grid", color_grid);
    lv_xml_register_subject(NULL, "color_control", color_control);
    lv_xml_register_subject(NULL, "color_on_green", color_on_green);
    lv_xml_register_subject(NULL, "color_logo_dot", color_logo_dot);
    lv_xml_register_subject(NULL, "color_pill", color_pill);
    lv_xml_register_subject(NULL, "color_pill_label", color_pill_label);
    lv_xml_register_subject(NULL, "color_pill_flarm", color_pill_flarm);
    lv_xml_register_subject(NULL, "color_pill_flarm_label", color_pill_flarm_label);
    lv_xml_register_subject(NULL, "subject_time", subject_time);
    lv_xml_register_subject(NULL, "subject_health", subject_health);
    lv_xml_register_subject(NULL, "subject_battery", subject_battery);
    lv_xml_register_subject(NULL, "subject_battery_state", subject_battery_state);
    lv_xml_register_subject(NULL, "subject_antenna", subject_antenna);
    lv_xml_register_subject(NULL, "subject_gps", subject_gps);
    lv_xml_register_subject(NULL, "subject_gps_symbol", subject_gps_symbol);
    lv_xml_register_subject(NULL, "subject_wifi", subject_wifi);
    lv_xml_register_subject(NULL, "subject_muted", subject_muted);
    lv_xml_register_subject(NULL, "subject_page", subject_page);
    lv_xml_register_subject(NULL, "subject_page_opening", subject_page_opening);
    lv_xml_register_subject(NULL, "subject_page_closing", subject_page_closing);
    lv_xml_register_subject(NULL, "subject_time_zone", subject_time_zone);
    lv_xml_register_subject(NULL, "subject_wifi_network", subject_wifi_network);
    lv_xml_register_subject(NULL, "subject_wifi_password", subject_wifi_password);
    lv_xml_register_subject(NULL, "subject_battery_remaining", subject_battery_remaining);
    lv_xml_register_subject(NULL, "subject_satellites", subject_satellites);
    lv_xml_register_subject(NULL, "subject_signal", subject_signal);
    lv_xml_register_subject(NULL, "subject_theme_dark", subject_theme_dark);
    lv_xml_register_subject(NULL, "subject_track_history", subject_track_history);
    lv_xml_register_subject(NULL, "subject_simple_symbols", subject_simple_symbols);
    lv_xml_register_subject(NULL, "subject_conflict_indicator", subject_conflict_indicator);
    lv_xml_register_subject(NULL, "subject_time_zone_automatic", subject_time_zone_automatic);
    lv_xml_register_subject(NULL, "subject_zone", subject_zone);
    lv_xml_register_subject(NULL, "subject_radar_range", subject_radar_range);
    lv_xml_register_subject(NULL, "subject_icao", subject_icao);
    lv_xml_register_subject(NULL, "subject_callsign", subject_callsign);
    lv_xml_register_subject(NULL, "subject_altitude_filter", subject_altitude_filter);
    lv_xml_register_subject(NULL, "subject_device_id", subject_device_id);
    lv_xml_register_subject(NULL, "subject_version", subject_version);
    lv_xml_register_subject(NULL, "subject_alert_phase", subject_alert_phase);
    lv_xml_register_subject(NULL, "subject_alert", subject_alert);

    lv_xml_register_event_cb(NULL, "header_left_clicked", header_left_clicked);
    lv_xml_register_event_cb(NULL, "header_right_clicked", header_right_clicked);
    lv_xml_register_event_cb(NULL, "zoom_out_clicked", zoom_out_clicked);
    lv_xml_register_event_cb(NULL, "zoom_in_clicked", zoom_in_clicked);

#endif

    /* Register all the global assets so that they won't be created again when globals.xml is parsed.
     * While running in the editor skip this step to update the preview when the XML changes */
#if defined(LV_USE_XML) && LV_USE_XML && !defined(LV_EDITOR_PREVIEW)
    lv_xml_register_image(NULL, "symbol_light", symbol_light);
    lv_xml_register_image(NULL, "symbol_airliner", symbol_airliner);
    lv_xml_register_image(NULL, "symbol_glider", symbol_glider);
    lv_xml_register_image(NULL, "symbol_helicopter", symbol_helicopter);
    lv_xml_register_image(NULL, "symbol_balloon", symbol_balloon);
    lv_xml_register_image(NULL, "symbol_unknown", symbol_unknown);
    lv_xml_register_image(NULL, "symbol_simple", symbol_simple);
    lv_xml_register_image(NULL, "symbol_paper_plane", symbol_paper_plane);
    lv_xml_register_image(NULL, "icon_gps_3d", icon_gps_3d);
    lv_xml_register_image(NULL, "icon_gps_2d", icon_gps_2d);
    lv_xml_register_image(NULL, "icon_gps_search", icon_gps_search);
    lv_xml_register_image(NULL, "icon_gps_antenna", icon_gps_antenna);
    lv_xml_register_image(NULL, "icon_gps_fault", icon_gps_fault);
    lv_xml_register_image(NULL, "icon_wifi", icon_wifi);
    lv_xml_register_image(NULL, "icon_wifi_off", icon_wifi_off);
    lv_xml_register_image(NULL, "icon_speaker", icon_speaker);
    lv_xml_register_image(NULL, "icon_speaker_off", icon_speaker_off);
    lv_xml_register_image(NULL, "icon_plus", icon_plus);
    lv_xml_register_image(NULL, "icon_minus", icon_minus);
    lv_xml_register_image(NULL, "icon_track", icon_track);
    lv_xml_register_image(NULL, "icon_track_on", icon_track_on);
    lv_xml_register_image(NULL, "icon_bell_slash", icon_bell_slash);
    lv_xml_register_image(NULL, "icon_bell", icon_bell);
    lv_xml_register_image(NULL, "icon_qrcode", icon_qrcode);
    lv_xml_register_image(NULL, "icon_chevron_left", icon_chevron_left);
    lv_xml_register_image(NULL, "icon_chevron_right", icon_chevron_right);
    lv_xml_register_image(NULL, "icon_delete", icon_delete);
    lv_xml_register_image(NULL, "icon_check", icon_check);
    lv_xml_register_image(NULL, "icon_chosen", icon_chosen);
    lv_xml_register_image(NULL, "icon_bolt", icon_bolt);
    lv_xml_register_image(NULL, "icon_bolt_cut", icon_bolt_cut);
    lv_xml_register_image(NULL, "icon_bolt_line", icon_bolt_line);
    lv_xml_register_image(NULL, "icon_battery_slash", icon_battery_slash);
    lv_xml_register_image(NULL, "icon_battery_slash_cut", icon_battery_slash_cut);
    lv_xml_register_image(NULL, "logo_air", logo_air);
    lv_xml_register_image(NULL, "logo_grep", logo_grep);
    lv_xml_register_image(NULL, "logo_plane", logo_plane);
    lv_xml_register_image(NULL, "icon_signal_0", icon_signal_0);
    lv_xml_register_image(NULL, "icon_signal_1", icon_signal_1);
    lv_xml_register_image(NULL, "icon_signal_2", icon_signal_2);
    lv_xml_register_image(NULL, "icon_signal_3", icon_signal_3);
#endif

#if !defined(LV_USE_XML) || LV_USE_XML == 0
    /* If XML is enabled it's assumed that the permanent screens are created
     * manually from XML using lv_xml_create() */
    /* Create all the permanent screens first so that they can reference each other */
    home = lv_obj_create(NULL);

    home_create();
#endif
}

void airgrep_ui_set_target(uint32_t target)
{
    airgrep_ui_target = target;
}

uint32_t airgrep_ui_get_target(void)
{
    return airgrep_ui_target;
}

bool airgrep_ui_check_target(uint32_t target)
{
    return (airgrep_ui_target & target) ? true : false;
}

#if defined(LV_EDITOR_PREVIEW)
void __attribute__((weak)) header_left_clicked(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("header_left_clicked was called\n");
}
void __attribute__((weak)) header_right_clicked(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("header_right_clicked was called\n");
}
void __attribute__((weak)) zoom_out_clicked(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("zoom_out_clicked was called\n");
}
void __attribute__((weak)) zoom_in_clicked(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG("zoom_in_clicked was called\n");
}
#endif

static void check_font(lv_font_t ** font, const char * name)
{
    if (!(*font)) {
        *font = (lv_font_t *)LV_FONT_DEFAULT;
        LV_LOG_WARN("font `%s` was not set. Using `LV_FONT_DEFAULT` instead", name);
    }
}