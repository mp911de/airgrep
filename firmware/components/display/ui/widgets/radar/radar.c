#include "radar_private_gen.h"
#include <math.h>
#include <stddef.h>
#include <string.h>
#include "navigation/units.h"
#include "radar_grid.h"
#include "radar_labels.h"
#include "radar_outline.h"
#include "radar_track.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#ifdef CONFIG_IDF_TARGET_ESP32S3
#include "radar_wedge.h"
#define RADAR_WEDGE_TAG (&radar_wedge_tag)
#else
#define RADAR_WEDGE_TAG NULL
#endif
#include "widgets/traffic_trend.h"
#include "airgrep_ui.h"

enum { OWN_TRACK_DEG = 0, RING_LABEL_DEG = 180 };

#define RING_WIDTH 2
#define OUTER_RING_MARGIN_PX 24
#define TICK_LENGTH 12
#define INNER_DASH_PX 8
#define DASH_LABEL_GAP_PX 5
#define SECTION_WIDTH 6
#define SECTION_TICK_LENGTH 14
#define SECTION_TICK_WIDTH 2
#define SECTION_LABEL_GAP_PX 1
#define NO_FIX_OPACITY LV_OPA_30
#define LABEL_OPTICAL_Y_PX (-2)
#define LABEL_FLIP_X_PX 100
#define TREND_SLOT_WIDTH 18
#define TREND_GAP 6
#define TREND_OPTICAL_Y_PX (-1)
#define DOT_SIZE (2 * RADAR_DOT_RADIUS_PX)
#define WEDGE_INNER_RADIUS_PX 32
#define LIMIT_WIDTH 2
/* The edge of the way out is dashed, so it reads as an opening. */
#define RECOVERY_DASH 8
#define RECOVERY_GAP 6
#define TAPE_MARK_HEIGHT_PX 4
#define TAPE_MARK_MARGIN_PX 2

#include "radar_recovery_sectors.h"

typedef struct {
    lv_point_t at;
    lv_area_t label;
    radar_point_t dots[RADAR_DOT_COUNT];
    uint32_t dot_count;
} placed_t;

static void draw(lv_event_t * e);
static void draw_grid(radar_t * radar, lv_layer_t * layer, lv_point_t center, int32_t outer, int32_t label_width,
                      lv_color_t color, lv_opa_t opa);
static int32_t lvgl_arc_angle(int32_t bearing_deg)
{
    return radar_bearing_deg(bearing_deg - 90);
}

static void draw_ring(lv_layer_t * layer, lv_point_t center, int32_t radius, int32_t width, int32_t from_deg,
                      int32_t to_deg, lv_color_t color, lv_opa_t opa, void * tag);
static void draw_dashed_ring(lv_layer_t * layer, lv_point_t center, int32_t radius, int32_t label_width,
                             lv_color_t color, lv_opa_t opa);
static void draw_spoke(lv_layer_t * layer, lv_point_t center, float deg, int32_t from, int32_t to, int32_t width,
                       lv_color_t color, lv_opa_t opa);
static void draw_section(lv_layer_t * layer, lv_point_t center, int32_t outer, const radar_section_t * section,
                         int32_t label_width, lv_color_t color);
static void draw_symbol(lv_layer_t * layer, lv_point_t at, const void * symbol, lv_color_t color, lv_opa_t opa,
                        int32_t scale, int32_t rotation_deg);
static void draw_track(lv_layer_t * layer, const radar_t * radar, const placed_t * placed,
                       const radar_target_t * target, int32_t outer);
static void draw_bands(lv_layer_t * layer, const radar_track_bands_t * bands, lv_point_t center, int32_t outer,
                       lv_color_t bg);
static void draw_way_out(radar_t * radar, lv_layer_t * layer, const radar_track_bands_t * bands,
                         lv_point_t center, int32_t outer);
static void draw_tape(lv_layer_t * layer, const radar_tape_band_t * bands, uint32_t count, int32_t x, lv_point_t center,
                      int32_t outer, lv_color_t bg);
static void draw_box(lv_layer_t * layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t color);
static lv_color_t band_color(radar_band_level_t level);
static void draw_text(lv_layer_t * layer, const char * text, const lv_font_t * font, lv_color_t color,
                      const lv_area_t * area);
static void place(const radar_t * radar, uint32_t count, const radar_field_t * field, placed_t * placed);
static uint32_t dots_of(const radar_t * radar, const radar_target_t * target, int32_t outer, radar_point_t * dots);
static radar_box_t box_of(const lv_area_t * area);
static void place_bottom_ring_label(const char * text, lv_point_t center, int32_t radius, lv_area_t * area);
static lv_color_t level_color(const radar_target_t * target);
static int32_t heading(const radar_target_t * target);
static const void * symbol_of(const radar_t * radar, const radar_target_t * target);
static void theme_changed(lv_observer_t * observer, lv_subject_t * subject);

static const uint8_t light_wedge_pct[] = {
    [RADAR_BAND_PREVENTIVE] = 10, [RADAR_BAND_CORRECTIVE] = 16, [RADAR_BAND_WARNING] = 10
};
static const uint8_t dark_wedge_pct[] = {
    [RADAR_BAND_PREVENTIVE] = 18, [RADAR_BAND_CORRECTIVE] = 24, [RADAR_BAND_WARNING] = 24
};

void radar_constructor_hook(lv_obj_t * obj)
{
    lv_subject_add_observer_obj(color_bg, theme_changed, obj, NULL);
    lv_subject_add_observer_obj(subject_gps, theme_changed, obj, NULL);
    lv_subject_add_observer_obj(subject_alert, theme_changed, obj, NULL);
}

void radar_destructor_hook(lv_obj_t * obj)
{
    radar_t * radar = (radar_t *)obj;
    radar_grid_release(&radar->sparse_grid);
    recovery_sectors_release(&radar->recovery_sectors);
}

void radar_event_hook(lv_event_t * e)
{
    if(lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        draw(e);
    }
}

void radar_set_range_nm(lv_obj_t * radar, int32_t range_nm)
{
    radar_t * widget = (radar_t *)radar;
    widget->range_nm = range_nm;
    lv_snprintf(widget->inner_label, sizeof(widget->inner_label), "%" LV_PRId32 " NM", range_nm / 2);
    lv_snprintf(widget->outer_label, sizeof(widget->outer_label), "%" LV_PRId32 " NM", range_nm);
    lv_obj_invalidate(radar);
}

void radar_set_tracks(lv_obj_t * radar, bool tracks)
{
    ((radar_t *)radar)->tracks = tracks;
    lv_obj_invalidate(radar);
}

void radar_set_simple(lv_obj_t * radar, bool simple)
{
    ((radar_t *)radar)->simple = simple;
    lv_obj_invalidate(radar);
}

void radar_set_indicator(lv_obj_t * radar, bool indicator)
{
    ((radar_t *)radar)->indicator = indicator;
    lv_obj_invalidate(radar);
}

void radar_set_track_bands(lv_obj_t * radar, const radar_track_band_t * bands, uint32_t count)
{
    radar_track_bands_t * shown = &((radar_t *)radar)->track_bands;
    if(!count && !shown->wedge_count && !shown->limit_count) {
        return;
    }
    radar_track_bands_t next;
    radar_bands_track(bands, LV_MIN(count, RADAR_MAX_BANDS), &next);
    bool same = next.wedge_count == shown->wedge_count && next.limit_count == shown->limit_count;
    for(uint32_t i = 0; same && i < next.wedge_count; i++) {
        const radar_wedge_t * a = &next.wedges[i], * b = &shown->wedges[i];
        same = a->start_deg == b->start_deg && a->sweep_deg == b->sweep_deg && a->level == b->level;
    }
    for(uint32_t i = 0; same && i < next.limit_count; i++) {
        same = next.limits[i].deg == shown->limits[i].deg && next.limits[i].level == shown->limits[i].level;
    }
    if(same) {
        return;
    }
    shown->wedge_count = next.wedge_count;
    shown->limit_count = next.limit_count;
    memcpy(shown->wedges, next.wedges, next.wedge_count * sizeof(*next.wedges));
    memcpy(shown->limits, next.limits, next.limit_count * sizeof(*next.limits));
    lv_obj_invalidate(radar);
}

void radar_set_tape(lv_obj_t * radar, radar_tape_t tape, const radar_tape_band_t * bands, uint32_t count)
{
    radar_t * widget = (radar_t *)radar;
    count = LV_MIN(count, RADAR_MAX_BANDS);
    if(count == widget->tape_counts[tape] &&
       (!count || !memcmp(bands, widget->tapes[tape], count * sizeof(*bands)))) {
        return;
    }
    widget->tape_counts[tape] = count;
    lv_memcpy(widget->tapes[tape], bands, widget->tape_counts[tape] * sizeof(radar_tape_band_t));
    lv_obj_invalidate(radar);
}

void radar_set_targets(lv_obj_t * radar, const radar_target_t * targets, uint32_t count)
{
    radar_t * widget = (radar_t *)radar;
    count = LV_MIN(count, RADAR_MAX_TARGETS);
    bool changed = count != widget->target_count;
    for(uint32_t i = 0; i < count; i++) {
        const radar_target_t * target = &targets[i];
        uint8_t samples = LV_MIN(target->tail_count, RADAR_TAIL_HISTORY_S);
        size_t used = offsetof(radar_target_t, tail) + samples * sizeof(*target->tail);
        if(i < widget->target_count && !memcmp(&widget->targets[i], target, used)) {
            continue;
        }
        lv_memcpy(&widget->targets[i], target, used);
        widget->targets[i].tail_count = samples;
        traffic_format_altitude(widget->labels[i], sizeof(widget->labels[i]), target->relative_altitude_ft, target->relative_altitude_unknown);
        changed = true;
    }
    widget->target_count = count;
    if(changed) {
        lv_obj_invalidate(radar);
    }
}

void radar_set_beyond(lv_obj_t * radar, const int16_t * bearings_deg, const traffic_level_t * levels, uint32_t count)
{
    radar_t * widget = (radar_t *)radar;
    count = LV_MIN(count, RADAR_MAX_TARGETS);
    bool changed = false;
    for(uint32_t group = 0; group < RADAR_SECTION_LEVELS; group++) {
        int16_t turned[RADAR_MAX_TARGETS];
        uint32_t members = 0;
        for(uint32_t i = 0; i < count; i++) {
            uint32_t of = levels[i] == TRAFFIC_LEVEL_ALERT ? 2 : levels[i] == TRAFFIC_LEVEL_ADVISORY ? 1 : 0;
            if(of == group) {
                turned[members++] = (int16_t)radar_bearing_deg(bearings_deg[i]);
            }
        }
        if(changed) {
            widget->section_counts[group] = radar_sections(turned, members, widget->sections[group]);
        }
        else {
            radar_section_t next[RADAR_MAX_SECTIONS];
            uint32_t used = radar_sections(turned, members, next);
            if(used != widget->section_counts[group] || memcmp(next, widget->sections[group], used * sizeof(*next))) {
                memcpy(widget->sections[group], next, used * sizeof(*next));
                widget->section_counts[group] = used;
                changed = true;
            }
        }
    }
    if(changed) {
        lv_obj_invalidate(radar);
    }
}

static void draw(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_current_target_obj(e);
    radar_t * radar = (radar_t *)obj;
    lv_layer_t * layer = lv_event_get_layer(e);

    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    lv_point_t center = {area.x1 + lv_area_get_width(&area) / 2, area.y1 + lv_area_get_height(&area) / 2};
    int32_t outer = lv_area_get_width(&area) / 2 - OUTER_RING_MARGIN_PX;
    int32_t inner = outer / 2;

    bool lost = lv_subject_get_int(subject_gps) == 0 || radar->range_nm <= 0;
    lv_opa_t opa = lost ? NO_FIX_OPACITY : LV_OPA_COVER;
    lv_color_t grid = lv_subject_get_color(color_grid);
    lv_color_t bg = lv_subject_get_color(color_bg);

    lv_area_t inner_label;
    lv_area_t outer_label;
    place_bottom_ring_label(radar->inner_label, center, inner, &inner_label);
    place_bottom_ring_label(radar->outer_label, center, outer, &outer_label);

    uint32_t count = lost ? 0 : radar->target_count;
    placed_t placed[RADAR_MAX_TARGETS];
    radar_field_t field = {.center = {center.x, center.y},
                           .inner = inner,
                           .outer = outer,
                           .prefer_left_after_x_px = LABEL_FLIP_X_PX,
                           .bounds = box_of(&area),
                           .ring_labels = {box_of(&inner_label), box_of(&outer_label)}};
    place(radar, count, &field, placed);

    bool indicator = radar->indicator && !lost;
    if(indicator) {
        draw_bands(layer, &radar->track_bands, center, outer, bg);
    }
    /* In an alert the alert bars stand where the tapes are. The hairline beside the radar keeps its column. */
    if(indicator && lv_subject_get_int(subject_alert) == 0) {
        draw_tape(layer, radar->tapes[RADAR_TAPE_SPEED], radar->tape_counts[RADAR_TAPE_SPEED], area.x1, center,
                  outer, bg);
        draw_tape(layer, radar->tapes[RADAR_TAPE_ALTITUDE], radar->tape_counts[RADAR_TAPE_ALTITUDE],
                  area.x2 + 1 - HAIRLINE - ALERT_STRIP_WIDTH, center, outer, bg);
    }

    for(uint32_t i = 0; radar->tracks && i < count; i++) {
        if(!radar->targets[i].muted) {
            draw_track(layer, radar, &placed[i], &radar->targets[i], outer);
        }
    }

    draw_grid(radar, layer, center, outer, lv_area_get_width(&inner_label), grid, opa);
    draw_spoke(layer, center, OWN_TRACK_DEG, outer, outer + TICK_LENGTH, RING_WIDTH, grid, opa);
    if(indicator) {
        draw_way_out(radar, layer, &radar->track_bands, center, outer);
    }
    lv_subject_t * const colors[RADAR_SECTION_LEVELS] = {color_label, color_yellow, color_red};
    for(uint32_t group = 0; !lost && group < RADAR_SECTION_LEVELS; group++) {
        for(uint32_t i = 0; i < radar->section_counts[group]; i++) {
            draw_section(layer, center, outer, &radar->sections[group][i], lv_area_get_width(&outer_label),
                         lv_subject_get_color(colors[group]));
        }
    }

    radar_outline_text(layer, radar->inner_label, font_small, bg, &inner_label);
    radar_outline_text(layer, radar->outer_label, font_small, bg, &outer_label);
    lv_color_t ring_text = lv_subject_get_color(color_label_2);
    draw_text(layer, radar->inner_label, font_small, ring_text, &inner_label);
    draw_text(layer, radar->outer_label, font_small, ring_text, &outer_label);
    for(uint32_t i = 0; i < count; i++) {
        const radar_target_t * target = &radar->targets[i];
        const void * symbol = symbol_of(radar, target);
        lv_color_t color = level_color(target);
        const lv_area_t * label = &placed[i].label;
        lv_point_t trend = {label->x2 + 1 + TREND_GAP + TRAFFIC_TREND_SIZE / 2,
                            label->y1 + lv_area_get_height(label) / 2 - LABEL_OPTICAL_Y_PX + TREND_OPTICAL_Y_PX};
        radar_outline_symbol(layer, symbol, placed[i].at, bg, RADAR_SYMBOL_SCALE, heading(target));
        radar_outline_text(layer, radar->labels[i], font_label, bg, label);
        traffic_draw_trend(layer, trend, target->vertical_trend, bg, true);
        draw_symbol(layer, placed[i].at, symbol, color, LV_OPA_COVER, RADAR_SYMBOL_SCALE, heading(target));
        draw_text(layer, radar->labels[i], font_label, color, &placed[i].label);
        traffic_draw_trend(layer, trend, target->vertical_trend, color, false);
    }

    const void * own = radar->simple ? symbol_paper_plane : symbol_light;
    radar_outline_symbol(layer, own, center, bg, RADAR_OWN_SYMBOL_SCALE, 0);
    draw_symbol(layer, center, own, lv_subject_get_color(color_green), opa, RADAR_OWN_SYMBOL_SCALE, 0);
}

static void grid_shapes(lv_layer_t * layer, lv_point_t center, int32_t outer, int32_t label_width, lv_color_t color,
                        lv_opa_t opa)
{
    draw_dashed_ring(layer, center, outer / 2, label_width, color, opa);
    draw_ring(layer, center, outer, RING_WIDTH, 0, 360, color, opa, NULL);
}

static lv_draw_buf_t *capture_grid(lv_layer_t *layer, const lv_area_t *area, lv_point_t center, int32_t outer,
                                   int32_t label_width, lv_opa_t opacity)
{
    lv_draw_buf_t *mask = lv_draw_buf_create((uint32_t)lv_area_get_width(area), (uint32_t)lv_area_get_height(area),
                                             LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
    if(!mask) {
        return NULL;
    }
    lv_draw_buf_clear(mask, NULL);
    lv_layer_t capture;
    lv_draw_layer_init(&capture, layer, LV_COLOR_FORMAT_A8, area);
    capture.draw_buf = mask;
    grid_shapes(&capture, center, outer, label_width, lv_color_white(), opacity);
    while(capture.draw_task_head) {
        if(!lv_draw_dispatch_layer(capture.display, &capture)) {
            lv_draw_wait_for_finish();
        }
    }
    lv_draw_layer_deinit(&capture);
    lv_draw_buf_flush_cache(mask, NULL);
    return mask;
}

static void draw_grid(radar_t * radar, lv_layer_t * layer, lv_point_t center, int32_t outer, int32_t label_width,
                      lv_color_t color, lv_opa_t opa)
{
    int32_t extent = outer + RING_WIDTH;
    lv_area_t area = {center.x - extent, center.y - extent, center.x + extent, center.y + extent};
    if(outer <= RING_WIDTH ||
       !radar_grid_draw(&radar->sparse_grid, layer, &area, center, outer, label_width, color, opa, capture_grid)) {
        grid_shapes(layer, center, outer, label_width, color, opa);
    }
}

static void draw_ring(lv_layer_t * layer, lv_point_t center, int32_t radius, int32_t width, int32_t from_deg,
                      int32_t to_deg, lv_color_t color, lv_opa_t opa, void * tag)
{
    if(to_deg <= from_deg) {
        return;
    }
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.base.user_data = tag;
    arc.color = color;
    arc.opa = opa;
    arc.width = width;
    arc.center = center;
    arc.radius = radius + width / 2;
    bool whole = to_deg - from_deg >= 360;
    arc.start_angle = whole ? 0 : lvgl_arc_angle(from_deg);
    arc.end_angle = whole ? 360 : lvgl_arc_angle(to_deg);
    lv_draw_arc(layer, &arc);
}

/* The ring starts and ends on a dash at its label, so the label cuts none. */
static void draw_dashed_ring(lv_layer_t * layer, lv_point_t center, int32_t radius, int32_t label_width,
                             lv_color_t color, lv_opa_t opa)
{
    float r = (float)radius;
    float open = (float)label_width / 2.0f + DASH_LABEL_GAP_PX;
    float length = 2 * (float)M_PI * r - 2 * open;
    int32_t parts = 2 * (int32_t)lroundf((length / INNER_DASH_PX + 1) / 2) - 1;
    float part = length / (float)parts / r;
    float start = RING_LABEL_DEG * radians_per_degree + open / r;

    lv_draw_line_dsc_t dash;
    lv_draw_line_dsc_init(&dash);
    dash.color = color;
    dash.opa = opa;
    dash.width = RING_WIDTH;
    for(int32_t i = 0; i < parts; i += 2) {
        float from = start + (float)i * part;
        float to = from + part;
        dash.p1.x = center.x + (int32_t)lroundf(sinf(from) * r);
        dash.p1.y = center.y - (int32_t)lroundf(cosf(from) * r);
        dash.p2.x = center.x + (int32_t)lroundf(sinf(to) * r);
        dash.p2.y = center.y - (int32_t)lroundf(cosf(to) * r);
        lv_draw_line(layer, &dash);
    }
}

static void draw_spoke(lv_layer_t * layer, lv_point_t center, float deg, int32_t from, int32_t to, int32_t width,
                       lv_color_t color, lv_opa_t opa)
{
    float x = sinf(deg * radians_per_degree);
    float y = -cosf(deg * radians_per_degree);
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = color;
    line.opa = opa;
    line.width = width;
    line.p1.x = center.x + (int32_t)lroundf(x * (float)from);
    line.p1.y = center.y + (int32_t)lroundf(y * (float)from);
    line.p2.x = center.x + (int32_t)lroundf(x * (float)to);
    line.p2.y = center.y + (int32_t)lroundf(y * (float)to);
    lv_draw_line(layer, &line);
}

static void draw_section(lv_layer_t * layer, lv_point_t center, int32_t outer, const radar_section_t * section,
                         int32_t label_width, lv_color_t color)
{
    int32_t from = section->start_deg;
    int32_t to = from + section->sweep_deg;
    int32_t hidden = (int32_t)ceilf(((float)label_width / 2.0f + SECTION_LABEL_GAP_PX) / (float)outer / radians_per_degree);

    /* A section that begins left of the track meets the label on its second turn. */
    static const int32_t labels[] = {RING_LABEL_DEG, RING_LABEL_DEG + 360};
    int32_t start = from;
    for(uint32_t i = 0; i < 2; i++) {
        if(labels[i] + hidden <= start || labels[i] - hidden >= to) {
            continue;
        }
        draw_ring(layer, center, outer, SECTION_WIDTH, start, labels[i] - hidden, color, LV_OPA_COVER, NULL);
        start = labels[i] + hidden;
    }
    draw_ring(layer, center, outer, SECTION_WIDTH, start, to, color, LV_OPA_COVER, NULL);

    if(section->sweep_deg >= 360) {
        return;
    }
    int32_t ends[] = {from, to};
    for(uint32_t i = 0; i < 2; i++) {
        if(LV_ABS(ends[i] % 360 - RING_LABEL_DEG) < hidden) {
            continue;
        }
        draw_spoke(layer, center, (float)ends[i], outer - SECTION_TICK_LENGTH / 2, outer + SECTION_TICK_LENGTH / 2,
                   SECTION_TICK_WIDTH, color, LV_OPA_COVER);
    }
}

static void draw_symbol(lv_layer_t * layer, lv_point_t at, const void * symbol, lv_color_t color, lv_opa_t opa,
                        int32_t scale, int32_t rotation_deg)
{
    lv_draw_image_dsc_t image;
    lv_draw_image_dsc_init(&image);
    image.src = symbol;
    image.recolor = color;
    image.recolor_opa = LV_OPA_COVER;
    image.opa = opa;
    image.scale_x = scale;
    image.scale_y = scale;
    image.rotation = rotation_deg * 10;
    const lv_image_dsc_t * source = symbol;
    image.pivot.x = source->header.w / 2;
    image.pivot.y = source->header.h / 2;

    lv_area_t area;
    lv_area_set(&area, at.x - image.pivot.x, at.y - image.pivot.y,
                at.x - image.pivot.x + source->header.w - 1, at.y - image.pivot.y + source->header.h - 1);
    radar_draw_symbol_image(layer, &image, &area);
}

/* The symbol is rounded to its pixel once, here: it must not sit a pixel off its place on a ring. */
static void place(const radar_t * radar, uint32_t count, const radar_field_t * field, placed_t * placed)
{
    radar_spot_t symbols[RADAR_MAX_TARGETS];
    radar_spot_t sizes[RADAR_MAX_TARGETS];
    radar_box_t labels[RADAR_MAX_TARGETS];
    /* A dot between the digits of a label reads as part of the number, so the labels keep off the dots. */
    radar_spot_t dots[RADAR_MAX_TARGETS * RADAR_DOT_COUNT];
    radar_field_t with_dots = *field;
    with_dots.dots = dots;
    with_dots.dot_count = 0;
    for(uint32_t i = 0; i < count; i++) {
        const radar_target_t * target = &radar->targets[i];
        float distance = target->distance_nm * (float)field->outer / (float)radar->range_nm;
        float bearing = (float)target->bearing_deg * radians_per_degree;
        symbols[i].x = field->center.x + (int32_t)lroundf(sinf(bearing) * distance);
        symbols[i].y = field->center.y - (int32_t)lroundf(cosf(bearing) * distance);

        lv_point_t size;
        lv_text_get_size(&size, radar->labels[i], font_label, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        sizes[i].x = size.x + (target->vertical_trend ? TREND_GAP + TREND_SLOT_WIDTH : 0);
        sizes[i].y = size.y;

        placed[i].dot_count = dots_of(radar, target, field->outer, placed[i].dots);
        for(uint32_t dot = 0; dot < placed[i].dot_count; dot++) {
            dots[with_dots.dot_count].x = symbols[i].x + (int32_t)lroundf(placed[i].dots[dot].x);
            dots[with_dots.dot_count].y = symbols[i].y + (int32_t)lroundf(placed[i].dots[dot].y);
            with_dots.dot_count++;
        }
    }
    radar_labels(&with_dots, symbols, sizes, count, labels);

    for(uint32_t i = 0; i < count; i++) {
        int32_t trend_width = radar->targets[i].vertical_trend ? TREND_GAP + TREND_SLOT_WIDTH : 0;
        placed[i].at.x = symbols[i].x;
        placed[i].at.y = symbols[i].y;
        placed[i].label.x1 = labels[i].x1;
        placed[i].label.x2 = labels[i].x2 - trend_width;
        placed[i].label.y1 = labels[i].y1 + LABEL_OPTICAL_Y_PX;
        placed[i].label.y2 = labels[i].y2 + LABEL_OPTICAL_Y_PX;
    }
}

static radar_box_t box_of(const lv_area_t * area)
{
    radar_box_t box = {area->x1, area->y1, area->x2, area->y2};
    return box;
}

static void place_bottom_ring_label(const char * text, lv_point_t center, int32_t radius, lv_area_t * area)
{
    lv_point_t size;
    lv_text_get_size(&size, text, font_small, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    area->x1 = center.x - size.x / 2;
    area->x2 = area->x1 + size.x - 1;
    area->y1 = center.y + radius - size.y / 2;
    area->y2 = area->y1 + size.y - 1;
}

/* The text must live until it is drawn, which may be after this returns. */
static void draw_text(lv_layer_t * layer, const char * text, const lv_font_t * font, lv_color_t color,
                      const lv_area_t * area)
{
    lv_draw_label_dsc_t label;
    lv_draw_label_dsc_init(&label);
    label.text = text;
    label.font = font;
    label.color = color;
    lv_draw_label(layer, &label, area);
}

typedef struct {
    lv_layer_t * layer;
    lv_point_t at;
    lv_color_t color;
} tail_pen_t;

static void draw_tail_run(void * user, int32_t x, int32_t y, int32_t length, uint8_t opa)
{
    const tail_pen_t * pen = user;
    lv_draw_rect_dsc_t run;
    lv_draw_rect_dsc_init(&run);
    run.bg_color = pen->color;
    run.bg_opa = opa;
    lv_area_t area = {pen->at.x + x, pen->at.y + y, pen->at.x + x + length - 1, pen->at.y + y};
    lv_draw_rect(pen->layer, &run, &area);
}

static void draw_stroke_mask(lv_layer_t * layer, lv_point_t at, const radar_tail_part_t * parts, uint32_t count,
                             lv_color_t color)
{
    if(!count) {
        return;
    }
    float left = 0, right = 0, top = 0, bottom = 0;
    for(uint32_t i = 0; i < count; i++) {
        left = fminf(left, fminf(parts[i].from.x, parts[i].to.x));
        right = fmaxf(right, fmaxf(parts[i].from.x, parts[i].to.x));
        top = fminf(top, fminf(parts[i].from.y, parts[i].to.y));
        bottom = fmaxf(bottom, fmaxf(parts[i].from.y, parts[i].to.y));
    }
    const float reach = RADAR_STROKE_WIDTH_PX / 2.0f + 0.5f;
    lv_area_t bounds = {(int32_t)floorf(left - reach), (int32_t)floorf(top - reach),
                        (int32_t)ceilf(right + reach), (int32_t)ceilf(bottom + reach)};
    lv_area_t area = bounds;
    lv_area_move(&area, at.x, at.y);
    lv_layer_t * mask = lv_draw_layer_create(layer, LV_COLOR_FORMAT_A8, &area);
    bool filled = mask && lv_draw_layer_alloc_buf(mask);
    if(filled) {
        radar_track_ribbon(mask->draw_buf->data, mask->draw_buf->header.stride,
                           bounds.x1, bounds.y1, bounds.x2, bounds.y2, parts, count);
        lv_draw_buf_flush_cache(mask->draw_buf, NULL);
    }
    if(mask) {
        /* LVGL releases this layer when its parent consumes the draw task. */
        lv_draw_image_dsc_t image;
        lv_draw_image_dsc_init(&image);
        image.src = mask;
        image.recolor = color;
        image.recolor_opa = LV_OPA_COVER;
        lv_draw_layer(layer, &image, &area);
    }
    if(!filled) {
        tail_pen_t pen = {layer, at, color};
        radar_track_tail_runs(parts, count, RADAR_STROKE_WIDTH_PX, draw_tail_run, &pen);
    }
}

/* The coverage of LVGL's six-pixel rounded rectangle. Tinting at draw time serves every level and theme. */
static const LV_ATTRIBUTE_MEM_ALIGN uint8_t dot_alpha[6][LV_DRAW_BUF_STRIDE(6, LV_COLOR_FORMAT_A8)] = {
    {0, 128, 240, 240, 128, 0},
    {128, 255, 255, 255, 255, 128},
    {240, 255, 255, 255, 255, 240},
    {240, 255, 255, 255, 255, 240},
    {128, 255, 255, 255, 255, 128},
    {0, 128, 240, 240, 128, 0}
};
_Static_assert(DOT_SIZE == 6, "prediction dot mask has diameter six");
static const lv_image_dsc_t dot_stamp = {
    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_A8, .w = DOT_SIZE, .h = DOT_SIZE,
               .stride = sizeof(dot_alpha[0])},
    .data_size = sizeof(dot_alpha), .data = &dot_alpha[0][0],
};

static void draw_track(lv_layer_t * layer, const radar_t * radar, const placed_t * placed,
                       const radar_target_t * target, int32_t outer)
{
    float pixels_per_nm = (float)outer / (float)radar->range_nm;
    float bearing = (float)target->bearing_deg * radians_per_degree;
    float right_nm = sinf(bearing) * target->distance_nm;
    float ahead_nm = cosf(bearing) * target->distance_nm;

    radar_point_t path[RADAR_TAIL_HISTORY_S + 1] = {{0, 0}};
    for(uint32_t i = 0; i < target->tail_count; i++) {
        path[i + 1].x = (target->tail[i].right_nm - right_nm) * pixels_per_nm;
        path[i + 1].y = -(target->tail[i].ahead_nm - ahead_nm) * pixels_per_nm;
    }
    radar_tail_part_t parts[RADAR_TAIL_HISTORY_S + RADAR_TAIL_STEPS];
    uint32_t part_count = radar_track_tail(path, target->tail_count + 1, parts);

    draw_stroke_mask(layer, placed->at, parts, part_count, TRAFFIC_COLOR_OTHER);

    if(!placed->dot_count) {
        return;
    }
    lv_draw_image_dsc_t dot;
    lv_draw_image_dsc_init(&dot);
    dot.src = &dot_stamp;
    dot.recolor = level_color(target);
    dot.recolor_opa = LV_OPA_COVER;
    for(uint32_t i = 0; i < placed->dot_count; i++) {
        lv_area_t area;
        area.x1 = placed->at.x + (int32_t)lroundf(placed->dots[i].x - DOT_SIZE / 2.0f);
        area.y1 = placed->at.y + (int32_t)lroundf(placed->dots[i].y - DOT_SIZE / 2.0f);
        area.x2 = area.x1 + DOT_SIZE - 1;
        area.y2 = area.y1 + DOT_SIZE - 1;
        lv_draw_image(layer, &dot, &area);
    }
}

/* A dash of the way out, from the middle of the radar. Painted pixel by pixel as a tail is: a line of the renderer ends
 * on whole pixels and would stand beside the ring it runs along. The ends are round, so the dash is drawn that much
 * shorter. */
static void draw_dash(lv_layer_t * layer, lv_point_t center, radar_point_t from, radar_point_t to)
{
    float length = hypotf(to.x - from.x, to.y - from.y);
    if(length <= RADAR_STROKE_WIDTH_PX) {
        return;
    }
    float cut = RADAR_STROKE_WIDTH_PX / 2.0f / length;
    radar_point_t at = {roundf(from.x), roundf(from.y)};
    radar_tail_part_t part = {{from.x + (to.x - from.x) * cut - at.x, from.y + (to.y - from.y) * cut - at.y},
                              {to.x - (to.x - from.x) * cut - at.x, to.y - (to.y - from.y) * cut - at.y}, 0};
    tail_pen_t pen = {layer, {center.x + (int32_t)at.x, center.y + (int32_t)at.y}, lv_subject_get_color(color_green)};
    radar_track_tail_runs(&part, 1, RADAR_STROKE_WIDTH_PX, draw_tail_run, &pen);
}

static void draw_dashed_circle(lv_layer_t * layer, lv_point_t center, int32_t radius)
{
    float r = (float)radius;
    float length = 2 * (float)M_PI * r;
    int32_t dashes = (int32_t)ceilf(length / (RECOVERY_DASH + RECOVERY_GAP));
    for(int32_t dash = 0; dash < dashes; dash++) {
        float along = (float)dash * (RECOVERY_DASH + RECOVERY_GAP);
        float from = along / r;
        float to = LV_MIN(along + RECOVERY_DASH, length) / r;
        radar_point_t begin = {sinf(from) * r, -cosf(from) * r};
        radar_point_t end = {sinf(to) * r, -cosf(to) * r};
        draw_dash(layer, center, begin, end);
    }
}

/* A wedge is a faint fill, so what lies on it stays readable. A limit is opaque, the colour of its level mixed
 * into the background: the fill beside it does not tint it. The way out is a gap, its edge is drawn later. */
static void draw_bands(lv_layer_t * layer, const radar_track_bands_t * bands, lv_point_t center, int32_t outer,
                       lv_color_t bg)
{
    const uint8_t * fill = lv_subject_get_int(subject_theme_dark) ? dark_wedge_pct : light_wedge_pct;
    for(uint32_t i = 0; i < bands->wedge_count; i++) {
        const radar_wedge_t * wedge = &bands->wedges[i];
        if(wedge->level == RADAR_BAND_RECOVERY) {
            continue;
        }
        draw_ring(layer, center, (WEDGE_INNER_RADIUS_PX + outer) / 2, outer - WEDGE_INNER_RADIUS_PX, wedge->start_deg,
                  wedge->start_deg + wedge->sweep_deg, band_color(wedge->level),
                  (lv_opa_t)(LV_OPA_COVER * fill[wedge->level] / 100), RADAR_WEDGE_TAG);
    }
    for(uint32_t i = 0; i < bands->limit_count; i++) {
        draw_spoke(layer, center, bands->limits[i].deg, WEDGE_INNER_RADIUS_PX, outer, LIMIT_WIDTH,
                   lv_color_mix(band_color(bands->limits[i].level), bg, LV_OPA_50), LV_OPA_COVER);
    }
}

/* The dashed edge of the way out lies on the rings, or the outer ring would hide the part that runs along it. */
static void draw_way_out(radar_t * radar, lv_layer_t * layer, const radar_track_bands_t * bands,
                         lv_point_t center, int32_t outer)
{
    if(recovery_sectors_draw(&radar->recovery_sectors, layer, bands, center, outer, lv_subject_get_color(color_green))) {
        return;
    }
    for(uint32_t i = 0; i < bands->wedge_count; i++) {
        const radar_wedge_t * wedge = &bands->wedges[i];
        if(wedge->level != RADAR_BAND_RECOVERY) {
            continue;
        }
        if(wedge->sweep_deg >= 360) {
            draw_dashed_circle(layer, center, outer);
            draw_dashed_circle(layer, center, WEDGE_INNER_RADIUS_PX);
            continue;
        }
        float length = radar_bands_edge_length(wedge, WEDGE_INNER_RADIUS_PX, outer);
        int32_t dashes = (int32_t)ceilf(length / (RECOVERY_DASH + RECOVERY_GAP));
        for(int32_t dash = 0; dash < dashes; dash++) {
            float along = (float)dash * (RECOVERY_DASH + RECOVERY_GAP);
            draw_dash(layer, center, radar_bands_edge(wedge, WEDGE_INNER_RADIUS_PX, outer, along),
                      radar_bands_edge(wedge, WEDGE_INNER_RADIUS_PX, outer, LV_MIN(along + RECOVERY_DASH, length)));
        }
    }
}

/* A tape has no scale. Its middle is what the own ship flies now, and the mark there never moves. */
static void draw_tape(lv_layer_t * layer, const radar_tape_band_t * bands, uint32_t count, int32_t x, lv_point_t center,
                      int32_t outer, lv_color_t bg)
{
    int32_t top = center.y - outer;
    draw_box(layer, x, top, x + ALERT_STRIP_WIDTH - 1, center.y + outer - 1, lv_subject_get_color(color_control));

    radar_stretch_t stretches[RADAR_MAX_BANDS];
    uint32_t stretch_count = radar_bands_tape(bands, count, 2 * outer, stretches);
    for(uint32_t i = 0; i < stretch_count; i++) {
        draw_box(layer, x, top + stretches[i].y, x + ALERT_STRIP_WIDTH - 1, top + stretches[i].y + stretches[i].height - 1,
                 band_color(stretches[i].level));
    }

    int32_t mark = center.y - TAPE_MARK_HEIGHT_PX / 2;
    draw_box(layer, x - TAPE_MARK_MARGIN_PX, mark - TAPE_MARK_MARGIN_PX, x + ALERT_STRIP_WIDTH - 1 + TAPE_MARK_MARGIN_PX,
             mark + TAPE_MARK_HEIGHT_PX - 1 + TAPE_MARK_MARGIN_PX, bg);
    draw_box(layer, x, mark, x + ALERT_STRIP_WIDTH - 1, mark + TAPE_MARK_HEIGHT_PX - 1, lv_subject_get_color(color_label));
}

static void draw_box(lv_layer_t * layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t color)
{
    lv_draw_rect_dsc_t box;
    lv_draw_rect_dsc_init(&box);
    box.bg_color = color;
    lv_area_t area = {x1, y1, x2, y2};
    lv_draw_rect(layer, &box, &area);
}

/* The preventive level is the accent: grey is the colour of other traffic and of the empty tape. */
static lv_color_t band_color(radar_band_level_t level)
{
    switch(level) {
        case RADAR_BAND_WARNING:
            return lv_subject_get_color(color_red);
        case RADAR_BAND_CORRECTIVE:
            return lv_subject_get_color(color_yellow);
        default:
            return lv_subject_get_color(color_tint);
    }
}

static uint32_t dots_of(const radar_t * radar, const radar_target_t * target, int32_t outer, radar_point_t * dots)
{
    if(!radar->tracks || target->muted || !target->predicted || target->speed_kt == 0) {
        return 0;
    }
    return radar_track_dots((float)target->speed_kt / 3600.0f * (float)outer / (float)radar->range_nm, target->track_deg,
                            target->turn_dps, dots);
}

static int32_t heading(const radar_target_t * target)
{
    return target->track_valid ? (int32_t)target->track_deg : 0;
}

static const void * symbol_of(const radar_t * radar, const radar_target_t * target)
{
    if(!radar->simple) {
        return target->symbol;
    }
    return target->track_valid ? symbol_simple : symbol_unknown;
}

static lv_color_t level_color(const radar_target_t * target)
{
    if(target->muted) {
        return TRAFFIC_COLOR_OTHER;
    }
    switch(target->level) {
        case TRAFFIC_LEVEL_ALERT:
            return lv_subject_get_color(color_red);
        case TRAFFIC_LEVEL_ADVISORY:
            return lv_subject_get_color(color_yellow);
        case TRAFFIC_LEVEL_PROXIMATE:
            return lv_subject_get_color(color_label);
        default:
            return TRAFFIC_COLOR_OTHER;
    }
}

static void theme_changed(lv_observer_t * observer, lv_subject_t * subject)
{
    if(subject == subject_gps &&
       (lv_subject_get_previous_int(subject) != 0) == (lv_subject_get_int(subject) != 0)) {
        return;
    }
    lv_obj_invalidate(lv_observer_get_target_obj(observer));
}
