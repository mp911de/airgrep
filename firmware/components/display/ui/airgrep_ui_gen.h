#ifndef LVGL_PRO_AIRGREP_UI_GEN_H
#define LVGL_PRO_AIRGREP_UI_GEN_H

#ifndef UI_SUBJECT_STRING_LENGTH
#define UI_SUBJECT_STRING_LENGTH 256
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif



/* Prototypes for target functions, needed by responsive const definitions */

void airgrep_ui_set_target(uint32_t target);
uint32_t airgrep_ui_get_target(void);
bool airgrep_ui_check_target(uint32_t target);

#define AIRGREP_UI_TARGET_UNDEFINED      (0 << 1)
#define AIRGREP_UI_TARGET_TOUCH_800X480  (1 << 1)
#define AIRGREP_UI_TARGET_ALL            0x0FFFFFFF

/* By default compile for all targets, allowing to switch to any targets at runtime */
#ifndef AIRGREP_UI_COMPILE_TARGET
#define AIRGREP_UI_COMPILE_TARGET AIRGREP_UI_TARGET_ALL
#endif

#define AIRGREP_UI_CHECK_COMPILE_TARGET(target) (AIRGREP_UI_COMPILE_TARGET & (target) ? 1 : 0)

/**
 * Smallest touch target, row and header height
 */
#define TOUCH (64)
#define HAIRLINE (1)
#define ALERT_STRIP_WIDTH (16)
#define TRAFFIC_WIDTH (400)
#define TRAFFIC_PADDING (12)
#define TRAFFIC_COLUMN_GAP (4)
#define TRAFFIC_TITLE_WIDTH (154)
#define TRAFFIC_ALT_WIDTH (80)
#define TRAFFIC_TREND_WIDTH (18)
#define TRAFFIC_BEARING_WIDTH (36)
#define TRAFFIC_DISTANCE_WIDTH (72)
#define SWIPE_ACTION_WIDTH (144)
#define RADIUS (12)
#define PRESSED (lv_color_hex(0x808080))
#define PRESSED_OPACITY_U8 (64)
#define LIGHT_BG (lv_color_hex(0xffffff))
#define DARK_BG (lv_color_hex(0x000000))
#define LIGHT_BG_GROUPED (lv_color_hex(0xefeff4))
#define DARK_BG_GROUPED (lv_color_hex(0x000000))
#define LIGHT_SURFACE (lv_color_hex(0xffffff))
#define DARK_SURFACE (lv_color_hex(0x1c1c1e))
#define LIGHT_SEPARATOR (lv_color_hex(0xc8c7cc))
#define DARK_SEPARATOR (lv_color_hex(0x38383a))
#define LIGHT_LABEL (lv_color_hex(0x000000))
#define DARK_LABEL (lv_color_hex(0xffffff))
#define LIGHT_LABEL_2 (lv_color_hex(0x6d6d72))
#define DARK_LABEL_2 (lv_color_hex(0x98989f))
#define LIGHT_TINT (lv_color_hex(0x007aff))
#define DARK_TINT (lv_color_hex(0x0a84ff))
#define LIGHT_GREEN (lv_color_hex(0x34c759))
#define DARK_GREEN (lv_color_hex(0x30d158))
#define LIGHT_RED (lv_color_hex(0xff3b30))
#define DARK_RED (lv_color_hex(0xff453a))
#define LIGHT_RED_DIM (lv_color_hex(0xf6b1ac))
#define DARK_RED_DIM (lv_color_hex(0x5a1410))
#define LIGHT_YELLOW (lv_color_hex(0xffcc00))
#define DARK_YELLOW (lv_color_hex(0xffd60a))
#define LIGHT_GRID (lv_color_hex(0xaeaeb2))
#define DARK_GRID (lv_color_hex(0x48484a))
#define LIGHT_CONTROL (lv_color_hex(0xe5e5ea))
#define DARK_CONTROL (lv_color_hex(0x2c2c2e))
#define LIGHT_LOGO_DOT (lv_color_hex(0xaeaeb2))
#define DARK_LOGO_DOT (lv_color_hex(0x636366))
#define LIGHT_ON_GREEN (lv_color_hex(0xffffff))
#define DARK_ON_GREEN (lv_color_hex(0x000000))
#define LIGHT_PILL (lv_color_hex(0xe5e5ea))
#define DARK_PILL (lv_color_hex(0x2c2c2e))
#define LIGHT_PILL_LABEL (lv_color_hex(0x3a3a3c))
#define DARK_PILL_LABEL (lv_color_hex(0xd1d1d6))
#define LIGHT_PILL_FLARM (lv_color_hex(0xd6eaff))
#define DARK_PILL_FLARM (lv_color_hex(0x0f2e4d))
#define LIGHT_PILL_FLARM_LABEL (lv_color_hex(0x0a4a8a))
#define DARK_PILL_FLARM_LABEL (lv_color_hex(0x8cc8ff))


#ifndef LV_XML_EVAL_STRING_BUF_SIZE
    #define LV_XML_EVAL_STRING_BUF_SIZE 256
#endif

/**
 * Animation path of a style transition with a `bezier(…)` easing. The control
 * points come from the transition descriptor's `user_data`, as a transition
 * descriptor cannot store animation parameters itself.
 */
static inline int32_t airgrep_ui_style_transition_bezier_path_helper(const lv_anim_t * a)
{
    const lv_anim_bezier3_para_t * para = (const lv_anim_bezier3_para_t *)lv_anim_get_user_data(a);
    if(para == NULL) return lv_anim_path_linear(a);

    lv_anim_t a_tmp = *a;
    lv_anim_set_bezier3_param(&a_tmp, para->x1, para->y1, para->x2, para->y2);
    return lv_anim_path_custom_bezier3(&a_tmp);
}

extern lv_obj_t * home;

extern lv_style_t style_pressed;
extern lv_style_t style_pressed_clear;

extern lv_font_t * font_label;
extern lv_font_t * font_small;
extern lv_font_t * font_text;
extern lv_font_t * font_title;
extern lv_font_t * font_data;
extern lv_font_t * font_display;
extern lv_font_t * font_stored;


extern const void * symbol_light;
extern const void * symbol_airliner;
extern const void * symbol_glider;
extern const void * symbol_helicopter;
extern const void * symbol_balloon;
extern const void * symbol_unknown;
extern const void * symbol_simple;
extern const void * symbol_paper_plane;
extern const void * icon_gps_3d;
extern const void * icon_gps_2d;
extern const void * icon_gps_search;
extern const void * icon_gps_antenna;
extern const void * icon_gps_fault;
extern const void * icon_wifi;
extern const void * icon_wifi_off;
extern const void * icon_speaker;
extern const void * icon_speaker_off;
extern const void * icon_plus;
extern const void * icon_minus;
extern const void * icon_track;
extern const void * icon_track_on;
extern const void * icon_bell_slash;
extern const void * icon_bell;
extern const void * icon_qrcode;
extern const void * icon_chevron_left;
extern const void * icon_chevron_right;
extern const void * icon_delete;
extern const void * icon_check;
extern const void * icon_chosen;
extern const void * icon_bolt;
extern const void * icon_bolt_cut;
extern const void * icon_bolt_line;
extern const void * icon_battery_slash;
extern const void * icon_battery_slash_cut;
extern const void * logo_air;
extern const void * logo_grep;
extern const void * logo_plane;
extern const void * icon_signal_0;
extern const void * icon_signal_1;
extern const void * icon_signal_2;
extern const void * icon_signal_3;

extern lv_subject_t * color_bg;
extern lv_subject_t * color_bg_grouped;
extern lv_subject_t * color_surface;
extern lv_subject_t * color_separator;
extern lv_subject_t * color_label;
extern lv_subject_t * color_label_2;
extern lv_subject_t * color_tint;
extern lv_subject_t * color_green;
extern lv_subject_t * color_red;
extern lv_subject_t * color_red_dim;
extern lv_subject_t * color_yellow;
extern lv_subject_t * color_grid;
extern lv_subject_t * color_control;
extern lv_subject_t * color_on_green;
extern lv_subject_t * color_logo_dot;
extern lv_subject_t * color_pill;
extern lv_subject_t * color_pill_label;
extern lv_subject_t * color_pill_flarm;
extern lv_subject_t * color_pill_flarm_label;
extern lv_subject_t * subject_time;
extern lv_subject_t * subject_health;
extern lv_subject_t * subject_battery;
extern lv_subject_t * subject_battery_state;
extern lv_subject_t * subject_antenna;
extern lv_subject_t * subject_gps;
extern lv_subject_t * subject_gps_symbol;
extern lv_subject_t * subject_wifi;
extern lv_subject_t * subject_muted;
extern lv_subject_t * subject_page;
extern lv_subject_t * subject_page_opening;
extern lv_subject_t * subject_page_closing;
extern lv_subject_t * subject_time_zone;
extern lv_subject_t * subject_wifi_network;
extern lv_subject_t * subject_wifi_password;
extern lv_subject_t * subject_wifi_join;
extern lv_subject_t * subject_battery_remaining;
extern lv_subject_t * subject_satellites;
extern lv_subject_t * subject_signal;
extern lv_subject_t * subject_theme_dark;
extern lv_subject_t * subject_track_history;
extern lv_subject_t * subject_simple_symbols;
extern lv_subject_t * subject_conflict_indicator;
extern lv_subject_t * subject_time_zone_automatic;
extern lv_subject_t * subject_zone;
extern lv_subject_t * subject_radar_range;
extern lv_subject_t * subject_icao;
extern lv_subject_t * subject_callsign;
extern lv_subject_t * subject_altitude_filter;
extern lv_subject_t * subject_device_id;
extern lv_subject_t * subject_version;
extern lv_subject_t * subject_alert_phase;
extern lv_subject_t * subject_alert;

void header_left_clicked(lv_event_t * e);
void header_right_clicked(lv_event_t * e);
void zoom_out_clicked(lv_event_t * e);
void zoom_in_clicked(lv_event_t * e);

void airgrep_ui_init_gen(const char * asset_path);

#include "components/cause_row_gen.h"
#include "components/chevron_gen.h"
#include "components/choice_row_gen.h"
#include "components/license_row_gen.h"
#include "components/list_group_gen.h"
#include "components/list_row_gen.h"
#include "components/nav_bar_gen.h"
#include "components/page_altitude_gen.h"
#include "components/page_callsign_gen.h"
#include "components/page_gen.h"
#include "components/page_icao_gen.h"
#include "components/page_license_gen.h"
#include "components/page_licenses_gen.h"
#include "components/page_system_gen.h"
#include "components/page_time_zone_gen.h"
#include "components/page_wifi_gen.h"
#include "components/page_zones_gen.h"
#include "components/section_header_gen.h"
#include "components/segmented_symbols_gen.h"
#include "components/segmented_theme_gen.h"
#include "components/status_header_gen.h"
#include "components/traffic_header_gen.h"
#include "screens/charger_gen.h"
#include "screens/home_gen.h"
#include "screens/startup_gen.h"
#include "widgets/entry_field/entry_field_gen.h"
#include "widgets/keyboard/keyboard_gen.h"
#include "widgets/logo/logo_gen.h"
#include "widgets/radar/radar_gen.h"
#include "widgets/traffic_table/traffic_table_gen.h"

#ifdef __cplusplus
}
#endif

#endif
