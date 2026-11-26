#ifndef LVGL_PRO_RADAR_H
#define LVGL_PRO_RADAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "radar_gen.h"
#include "radar_bands.h"
#include "radar_sections.h"
#include "widgets/traffic_types.h"

#define RADAR_MAX_TARGETS 32
#define RADAR_TAIL_HISTORY_S 60
#define RADAR_LABEL_CAPACITY 8

/* A place relative to the own ship: to the right of its track and ahead of it. */
typedef struct {
    float right_nm;
    float ahead_nm;
} radar_place_t;

/* Bearing and track are relative to the own track, the radar is track up. */
typedef struct {
    const void * symbol; /* Image of the kind, nose up */
    traffic_level_t level;
    bool muted;
    bool track_valid;
    int16_t bearing_deg;
    int16_t track_deg;
    int16_t relative_altitude_ft;
    bool relative_altitude_unknown;
    traffic_vertical_trend_t vertical_trend;
    float distance_nm;
    bool predicted;
    uint16_t speed_kt;
    float turn_dps; /* To the right is positive */
    uint8_t tail_count;
    radar_place_t tail[RADAR_TAIL_HISTORY_S]; /* Where it was, the newest second first */
} radar_target_t;

/**
 * Replace the targets inside the outer ring. The radar keeps a copy, up to RADAR_MAX_TARGETS.
 */
void radar_set_targets(lv_obj_t * radar, const radar_target_t * targets, uint32_t count);

#define RADAR_SECTION_LEVELS 3

/**
 * Replace the targets beyond the outer ring. Each lights the part of the ring at its bearing. Targets in
 * conflict have sections of their own, in the colour of their level and on top of the others.
 * @param bearings_deg  relative to the own track, clockwise. Up to RADAR_MAX_TARGETS.
 * @param levels        one for each bearing
 */
void radar_set_beyond(lv_obj_t * radar, const int16_t * bearings_deg, const traffic_level_t * levels, uint32_t count);

typedef enum {
    RADAR_TAPE_SPEED,
    RADAR_TAPE_ALTITUDE,
} radar_tape_t;

/**
 * Replace the Bands of the own track. The radar keeps what it draws of them.
 * @param bands     degrees clockwise from the own track. Up to RADAR_MAX_BANDS.
 */
void radar_set_track_bands(lv_obj_t * radar, const radar_track_band_t * bands, uint32_t count);

/* Prepare the recovery rings or one spoke angle. True while more work remains. */
bool radar_warm_recovery_next(lv_obj_t * radar);

/**
 * Replace the Bands of a tape. The radar keeps a copy. The middle of a tape is what the own ship flies now.
 * @param bands     shares of what the tape spans to either side of its middle, -1 at its lower end and 1 at its
 *                  upper end. Up to RADAR_MAX_BANDS.
 */
void radar_set_tape(lv_obj_t * radar, radar_tape_t tape, const radar_tape_band_t * bands, uint32_t count);

/* The texts live here, so a drawing that is carried out later still finds them. */
#define RADAR_USER_DATA \
    radar_target_t targets[RADAR_MAX_TARGETS]; \
    char labels[RADAR_MAX_TARGETS][RADAR_LABEL_CAPACITY]; \
    uint32_t target_count; \
    radar_section_t sections[RADAR_SECTION_LEVELS][RADAR_MAX_SECTIONS]; \
    uint32_t section_counts[RADAR_SECTION_LEVELS]; \
    char inner_label[RADAR_LABEL_CAPACITY]; \
    char outer_label[RADAR_LABEL_CAPACITY]; \
    radar_track_bands_t track_bands; \
    radar_tape_band_t tapes[2][RADAR_MAX_BANDS]; \
    uint32_t tape_counts[2]; \
    struct radar_grid_cache * sparse_grid; \
    struct recovery_sectors * recovery_sectors;

/* The Editor's radar_gen.c calls these. */
void radar_constructor_hook(lv_obj_t * obj);
void radar_destructor_hook(lv_obj_t * obj);
void radar_event_hook(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif
