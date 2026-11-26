#ifndef LVGL_PRO_TRAFFIC_TABLE_H
#define LVGL_PRO_TRAFFIC_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "traffic_table_gen.h"
#include "widgets/traffic_types.h"

typedef enum { TRAFFIC_ADSB, TRAFFIC_ADSR, TRAFFIC_TISB, TRAFFIC_FLARM, TRAFFIC_UNKNOWN_SOURCE } traffic_source_t;

#define TRAFFIC_TABLE_MAX_ROWS 32
#define TRAFFIC_TABLE_BAND_SIZE 24
#define TRAFFIC_NO_CONFLICT UINT16_MAX

typedef struct {
    uint32_t target_id;
    const char * title;
    traffic_source_t source;
    traffic_level_t level;
    bool muted;
    bool own_ship_offered;
    bool relative_position_available;
    int16_t relative_altitude_ft;
    bool relative_altitude_unknown;
    traffic_vertical_trend_t vertical_trend;
    int16_t relative_bearing_deg;
    float distance_nm;
    uint16_t conflict_in_s;
} traffic_row_t;

typedef enum {
    TRAFFIC_ACTION_MUTE,
    TRAFFIC_ACTION_UNMUTE,
    TRAFFIC_ACTION_OWN_SHIP,
} traffic_action_t;

typedef void (*traffic_action_cb_t)(traffic_action_t action, uint32_t target_id);

/**
 * Replace the rows, in the order given, one row at a time. Texts are copied. Up to TRAFFIC_TABLE_MAX_ROWS.
 * While a row or the table is pressed, the table moves or a row is open, the rows keep their targets and
 * their order and only take the new values.
 * @param inside    the first rows, those of targets inside the outer ring of the radar. The band
 *                  lies between them and the rest. All rows inside: no band.
 */
void traffic_table_begin_rows(lv_obj_t * table, uint32_t count, uint32_t inside);
void traffic_table_set_row(lv_obj_t * table, uint32_t index, const traffic_row_t * data);

/**
 * What the band says, as "BEYOND 2 NM". The text is copied, up to TRAFFIC_TABLE_BAND_SIZE - 1 bytes.
 */
void traffic_table_set_band(lv_obj_t * table, const char * text);

void traffic_table_set_action_cb(lv_obj_t * table, traffic_action_cb_t cb);

void traffic_table_scroll_to_top(lv_obj_t * table);

#define TRAFFIC_TABLE_USER_DATA \
    bool locked; \
    traffic_action_cb_t on_action; \
    lv_timer_t * close_timer; \
    uint32_t first_outside_row; \
    char band[TRAFFIC_TABLE_BAND_SIZE];

/* The Editor's traffic_table_gen.c calls these. */
void traffic_table_constructor_hook(lv_obj_t * obj);
void traffic_table_destructor_hook(lv_obj_t * obj);
void traffic_table_event_hook(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif
