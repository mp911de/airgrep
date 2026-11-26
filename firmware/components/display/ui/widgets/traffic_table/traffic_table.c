#include "traffic_table_private_gen.h"
#include "airgrep_ui.h"
#include "widgets/traffic_trend.h"

#define ALT_X (TRAFFIC_PADDING + TRAFFIC_TITLE_WIDTH + TRAFFIC_COLUMN_GAP)
#define TREND_X (ALT_X + TRAFFIC_ALT_WIDTH + TRAFFIC_COLUMN_GAP)
#define TREND_CENTER_X (TREND_X + TRAFFIC_TREND_WIDTH / 2)
#define BEARING_X (TREND_X + TRAFFIC_TREND_WIDTH + TRAFFIC_COLUMN_GAP)
#define BEARING_CENTER_X (BEARING_X + TRAFFIC_BEARING_WIDTH / 2)
#define NM_X (BEARING_X + TRAFFIC_BEARING_WIDTH + TRAFFIC_COLUMN_GAP)

#define TITLE_Y 3
#define PILL_Y 35
#define CONFLICT_TIME_GAP_PX 8
#define TEXT_OPTICAL_Y_PX 1
#define BAND (TOUCH / 2)
#define BAND_LETTER_SPACE 1

#define ACTION_ICON_SIZE 26
#define INITIAL_ACTION_OPACITY LV_OPA_10
/* The traffic symbol has air around its shape, the bell has none: drawn this large the plane matches the bell. */
#define OWN_ICON_SIZE 36
#define ROW_CLOSE_DELAY_MS 10000
#define BELL_TOUCH_EXTENSION_PX 20

enum { ROW_OWN, ROW_CONTENT, ROW_MUTE };
enum { CONTENT_TITLE, CONTENT_PILL, CONTENT_BELL, CONTENT_CONFLICT_TIME, CONTENT_ALT, CONTENT_NM };
enum { ACTION_ICON, ACTION_LABEL };
enum { ROW_TITLE, ROW_SOURCE, ROW_ATTENTION, ROW_ALTITUDE, ROW_DISTANCE, ROW_CONFLICT, ROW_TREND, ROW_BEARING, ROW_SUBJECT_COUNT };
enum { ATTENTION_MUTED = -1, UNKNOWN_VALUE = INT32_MIN };

typedef struct {
    uint32_t target_id;
    lv_subject_t * subjects[ROW_SUBJECT_COUNT];
    char * title_buffers;
    size_t title_capacity;
} traffic_row_state_t;

static void draw_band(lv_event_t * e);
static void invalidate_band(lv_obj_t * table);
static int32_t nearest_row_stop_y(lv_obj_t * table, int32_t y);
static lv_obj_t * create_row(lv_obj_t * table);
static lv_obj_t * create_action(lv_obj_t * row);
static void update_row(lv_obj_t * row, const traffic_row_t * data);
static void draw_trend_and_bearing(lv_event_t * e);
static int32_t closed_row_scroll_x(lv_obj_t * row);
static bool rows_locked(lv_obj_t * table);
static void keep_inside(lv_obj_t * obj);
static void bind_row(lv_obj_t * row);
static void close_rows(lv_obj_t * table, lv_obj_t * except, lv_anim_enable_t anim);
static void row_scrolled(lv_event_t * e);
static void fade_actions(lv_obj_t * row);
static void action_clicked(lv_event_t * e);
static void bell_clicked(lv_event_t * e);
static void pressed_anywhere(lv_event_t * e);
static void close_idle(lv_timer_t * timer);
static void report(lv_obj_t * row, traffic_action_t action);

void traffic_table_constructor_hook(lv_obj_t * obj)
{
    ((traffic_table_t *)obj)->close_timer = lv_timer_create(close_idle, 1000, obj);
    for(lv_indev_t * indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev)) {
        lv_indev_add_event_cb(indev, pressed_anywhere, LV_EVENT_PRESSED, obj);
    }
}

void traffic_table_destructor_hook(lv_obj_t * obj)
{
    lv_timer_delete(((traffic_table_t *)obj)->close_timer);
    for(lv_indev_t * indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev)) {
        lv_indev_remove_event_cb_with_user_data(indev, pressed_anywhere, obj);
    }
}

void traffic_table_event_hook(lv_event_t * e)
{
    lv_obj_t * table = lv_event_get_current_target_obj(e);
    if(lv_event_get_code(e) == LV_EVENT_DRAW_MAIN) {
        draw_band(e);
    }
    if(lv_event_get_code(e) == LV_EVENT_SCROLL) {
        keep_inside(table);
    }
    if(lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
        close_rows(table, NULL, LV_ANIM_ON);
    }
    /* The table never rests on half a row. LVGL's own snapping lets a drag pull the last row to the top. */
    if(lv_event_get_code(e) == LV_EVENT_SCROLL_END) {
        int32_t y = lv_obj_get_scroll_y(table);
        int32_t stop = nearest_row_stop_y(table, y);
        if(y != stop) {
            lv_obj_scroll_to_y(table, stop, LV_ANIM_ON);
        }
    }
}

void traffic_table_set_rows(lv_obj_t * table, const traffic_row_t * rows, uint32_t count, uint32_t inside)
{
    traffic_table_begin_rows(table, count, inside);
    for(uint32_t i = 0; i < LV_MIN(count, TRAFFIC_TABLE_MAX_ROWS); i++) {
        traffic_table_set_row(table, i, &rows[i]);
    }
}

void traffic_table_begin_rows(lv_obj_t * table, uint32_t count, uint32_t inside)
{
    traffic_table_t * widget = (traffic_table_t *)table;
    count = LV_MIN(count, TRAFFIC_TABLE_MAX_ROWS);
    widget->locked = rows_locked(table);
    if(widget->locked) {
        return;
    }

    uint32_t previous_count = lv_obj_get_child_count(table);
    uint32_t previous_divider = widget->first_outside_row;
    if(previous_divider < previous_count && (inside >= count || previous_divider != inside)) {
        invalidate_band(table);
    }
    while(lv_obj_get_child_count(table) > count) {
        lv_obj_delete(lv_obj_get_child(table, -1));
    }
    while(lv_obj_get_child_count(table) < count) {
        if(!create_row(table)) {
            count = lv_obj_get_child_count(table);
            break;
        }
    }

    widget->first_outside_row = LV_MIN(inside, count);
    bool had_divider = previous_divider < previous_count && previous_divider < count;
    if(had_divider && previous_divider != widget->first_outside_row) {
        lv_obj_set_style_margin_top(lv_obj_get_child(table, (int32_t)previous_divider), 0, 0);
    }
    if(widget->first_outside_row < count && (!had_divider || previous_divider != widget->first_outside_row)) {
        lv_obj_set_style_margin_top(lv_obj_get_child(table, (int32_t)widget->first_outside_row), BAND, 0);
        lv_obj_update_layout(table);
        invalidate_band(table);
    }
}

void traffic_table_set_row(lv_obj_t * table, uint32_t index, const traffic_row_t * data)
{
    if(((traffic_table_t *)table)->locked) {
        for(uint32_t i = 0; i < lv_obj_get_child_count(table); i++) {
            lv_obj_t * row = lv_obj_get_child(table, (int32_t)i);
            const traffic_row_state_t * state = lv_obj_get_user_data(row);
            if(data->target_id == state->target_id) {
                traffic_row_t held = *data;
                held.own_ship_offered = closed_row_scroll_x(row) != 0;
                update_row(row, &held);
                return;
            }
        }
    }
    else if(index < lv_obj_get_child_count(table)) {
        update_row(lv_obj_get_child(table, (int32_t)index), data);
    }
}

void traffic_table_set_band(lv_obj_t * table, const char * text)
{
    traffic_table_t * widget = (traffic_table_t *)table;
    if(lv_strcmp(widget->band, text) == 0) {
        return;
    }
    lv_strlcpy(widget->band, text, sizeof(widget->band));
    invalidate_band(table);
}

void traffic_table_set_action_cb(lv_obj_t * table, traffic_action_cb_t cb)
{
    ((traffic_table_t *)table)->on_action = cb;
}

void traffic_table_scroll_to_top(lv_obj_t * table)
{
    close_rows(table, NULL, LV_ANIM_OFF);
    lv_obj_scroll_to_y(table, 0, LV_ANIM_OFF);
}

static bool band_area(lv_obj_t * table, lv_area_t * area)
{
    const traffic_table_t * widget = (const traffic_table_t *)table;
    if(widget->first_outside_row >= lv_obj_get_child_count(table)) {
        return false;
    }
    lv_obj_get_coords(lv_obj_get_child(table, (int32_t)widget->first_outside_row), area);
    area->y2 = area->y1 - 1;
    area->y1 -= BAND;
    area->x2 = area->x1 + TRAFFIC_WIDTH - 1;
    return true;
}

static void invalidate_band(lv_obj_t * table)
{
    lv_area_t area;
    if(band_area(table, &area)) {
        lv_obj_invalidate_area(table, &area);
    }
}

static void draw_band(lv_event_t * e)
{
    lv_obj_t * table = lv_event_get_current_target_obj(e);
    const traffic_table_t * widget = (const traffic_table_t *)table;
    lv_area_t area;
    if(!band_area(table, &area)) {
        return;
    }
    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color = lv_subject_get_color(color_control);
    fill.border_color = lv_subject_get_color(color_separator);
    fill.border_width = HAIRLINE;
    fill.border_side = LV_BORDER_SIDE_BOTTOM;
    lv_draw_rect(lv_event_get_layer(e), &fill, &area);

    lv_draw_label_dsc_t text;
    lv_draw_label_dsc_init(&text);
    text.text = widget->band;
    text.font = font_small;
    text.color = lv_subject_get_color(color_label_2);
    text.letter_space = BAND_LETTER_SPACE;
    area.x1 += TRAFFIC_PADDING;
    area.y1 += (BAND - HAIRLINE - lv_font_get_line_height(font_small)) / 2;
    lv_draw_label(lv_event_get_layer(e), &text, &area);
}

static int32_t nearest_row_stop_y(lv_obj_t * table, int32_t y)
{
    const traffic_table_t * widget = (const traffic_table_t *)table;
    bool below_band = widget->first_outside_row < lv_obj_get_child_count(table) &&
                      y > (int32_t)widget->first_outside_row * TOUCH + BAND / 2;
    int32_t shift = below_band ? BAND : 0;
    int32_t stop = (y - shift + TOUCH / 2) / TOUCH * TOUCH + shift;
    return LV_MIN(stop, y + lv_obj_get_scroll_bottom(table));
}

/* A row is a strip of three parts, the content in the middle. It rests on the content and scrolls
 * sideways to uncover an action. LVGL gives a touch to the row or to the table by the axis that moves first. */
static void delete_row_subjects(lv_event_t * e)
{
    traffic_row_state_t * state = lv_event_get_user_data(e);
    for(unsigned i = 0; i < ROW_SUBJECT_COUNT; i++) {
        lv_subject_delete(state->subjects[i]);
    }
    lv_free(state->title_buffers);
    lv_free(state);
}

static lv_obj_t * create_row(lv_obj_t * table)
{
    traffic_row_state_t * state = lv_calloc(1, sizeof(*state));
    if(!state) {
        return NULL;
    }
    lv_obj_t * row = lv_obj_create(table);
    if(!row) {
        lv_free(state);
        return NULL;
    }
    lv_obj_add_event_cb(row, delete_row_subjects, LV_EVENT_DELETE, state);
    for(unsigned i = 0; i < ROW_SUBJECT_COUNT; i++) {
        state->subjects[i] = lv_subject_create(i == ROW_TITLE ? LV_SUBJECT_TYPE_STRING : LV_SUBJECT_TYPE_INT);
        if(!state->subjects[i]) {
            lv_obj_delete(row);
            return NULL;
        }
    }
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, TRAFFIC_WIDTH, TOUCH);
    lv_obj_set_user_data(row, state);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_scroll_dir(row, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_elastic(row, false);
    lv_obj_set_scroll_momentum(row, false);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, HAIRLINE, 0);
    lv_obj_set_style_border_post(row, true, 0);
    lv_obj_add_event_cb(row, row_scrolled, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(row, row_scrolled, LV_EVENT_SCROLL_BEGIN, NULL);
    lv_obj_add_event_cb(row, row_scrolled, LV_EVENT_SCROLL_END, NULL);

    lv_obj_t * own = create_action(row);
    lv_obj_set_hidden(own, true);
    lv_image_set_src(lv_obj_get_child(own, ACTION_ICON), symbol_light);
    lv_obj_set_size(lv_obj_get_child(own, ACTION_ICON), OWN_ICON_SIZE, OWN_ICON_SIZE);
    lv_obj_set_style_margin_hor(lv_obj_get_child(own, ACTION_ICON), (ACTION_ICON_SIZE - OWN_ICON_SIZE) / 2, 0);
    lv_obj_add_event_cb(own, action_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)ROW_OWN);
    lv_obj_bind_style_prop(own, LV_STYLE_BG_COLOR, 0, color_green);
    lv_obj_bind_style_prop(own, LV_STYLE_TEXT_COLOR, 0, color_on_green);
    lv_obj_bind_style_prop(lv_obj_get_child(own, ACTION_ICON), LV_STYLE_IMAGE_RECOLOR, 0, color_on_green);
    lv_label_set_translation_tag(lv_obj_get_child(own, ACTION_LABEL), "own_ship");

    lv_obj_t * content = lv_obj_create(row);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, TRAFFIC_WIDTH, TOUCH - HAIRLINE);
    lv_obj_set_scrollable(content, false);
    lv_obj_add_event_cb(content, draw_trend_and_bearing, LV_EVENT_DRAW_MAIN_END, state);

    lv_obj_t * title = lv_label_create(content);
    lv_obj_set_pos(title, TRAFFIC_PADDING, TITLE_Y);
    lv_obj_set_width(title, TRAFFIC_TITLE_WIDTH);
    lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_font(title, font_title, 0);
    lv_label_set_text_static(title, "");

    lv_obj_t * pill = lv_label_create(content);
    lv_obj_set_pos(pill, TRAFFIC_PADDING, PILL_Y);
    lv_obj_set_style_text_font(pill, font_small, 0);
    lv_obj_set_style_pad_hor(pill, 8, 0);
    lv_obj_set_style_pad_ver(pill, 1, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);

    lv_obj_t * bell = lv_image_create(content);
    lv_image_set_src(bell, icon_bell_slash);
    lv_obj_set_style_image_recolor_opa(bell, LV_OPA_COVER, 0);
    lv_obj_set_clickable(bell, true);
    lv_obj_add_style(bell, &style_pressed, LV_STATE_PRESSED);
    lv_obj_set_ext_click_area(bell, BELL_TOUCH_EXTENSION_PX);
    lv_obj_bind_style_prop(bell, LV_STYLE_IMAGE_RECOLOR, 0, color_tint);
    lv_obj_add_event_cb(bell, bell_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t * conflict_seconds = lv_label_create(content);
    lv_obj_set_style_text_font(conflict_seconds, font_small, 0);

    lv_obj_t * alt = lv_label_create(content);
    lv_obj_set_width(alt, TRAFFIC_ALT_WIDTH);
    lv_obj_align(alt, LV_ALIGN_LEFT_MID, ALT_X, TEXT_OPTICAL_Y_PX);
    lv_obj_set_style_text_font(alt, font_data, 0);
    lv_obj_set_style_text_align(alt, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t * nm = lv_label_create(content);
    lv_obj_set_width(nm, TRAFFIC_DISTANCE_WIDTH);
    lv_obj_align(nm, LV_ALIGN_LEFT_MID, NM_X + 1, TEXT_OPTICAL_Y_PX);
    lv_obj_set_style_text_font(nm, font_data, 0);
    lv_obj_set_style_text_align(nm, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t * mute = create_action(row);
    lv_obj_bind_style_prop(mute, LV_STYLE_BG_COLOR, 0, color_tint);
    lv_obj_set_style_text_color(mute, TRAFFIC_COLOR_ON_RED, 0);
    lv_obj_set_style_image_recolor(lv_obj_get_child(mute, ACTION_ICON), TRAFFIC_COLOR_ON_RED, 0);
    lv_obj_add_event_cb(mute, action_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)ROW_MUTE);
    lv_obj_bind_style_prop(row, LV_STYLE_BORDER_COLOR, 0, color_separator);
    bind_row(row);
    fade_actions(row);
    return row;
}

static lv_obj_t * create_action(lv_obj_t * row)
{
    lv_obj_t * action = lv_obj_create(row);
    lv_obj_remove_style_all(action);
    lv_obj_set_size(action, SWIPE_ACTION_WIDTH, TOUCH);
    lv_obj_set_scrollable(action, false);
    lv_obj_set_clickable(action, true);
    lv_obj_add_style(action, &style_pressed, LV_STATE_PRESSED);
    lv_obj_set_flex_flow(action, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(action, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(action, 8, 0);
    lv_obj_set_style_bg_opa(action, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(action, font_label, 0);

    lv_obj_t * icon = lv_image_create(action);
    lv_obj_set_size(icon, ACTION_ICON_SIZE, ACTION_ICON_SIZE);
    lv_image_set_inner_align(icon, LV_IMAGE_ALIGN_CONTAIN);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_label_create(action);
    return action;
}

static void altitude_changed(lv_observer_t * observer, lv_subject_t * subject)
{
    const int32_t feet = lv_subject_get_int(subject);
    char text[12];
    traffic_format_altitude(text, sizeof(text), feet, feet == UNKNOWN_VALUE);
    lv_label_set_text(lv_observer_get_target_obj(observer), text);
}

static void distance_changed(lv_observer_t * observer, lv_subject_t * subject)
{
    lv_obj_t * label = lv_observer_get_target_obj(observer);
    const int32_t tenths = lv_subject_get_int(subject);
    if(tenths == UNKNOWN_VALUE) {
        lv_label_set_text_static(label, "–");
    }
    else {
        lv_label_set_text_fmt(label, "%" LV_PRId32 ".%" LV_PRId32, tenths / 10, tenths % 10);
    }
}

static void conflict_changed(lv_observer_t * observer, lv_subject_t * subject)
{
    lv_obj_t * label = lv_observer_get_target_obj(observer);
    const int32_t seconds = lv_subject_get_int(subject);
    lv_obj_set_hidden(label, seconds == TRAFFIC_NO_CONFLICT);
    if(seconds != TRAFFIC_NO_CONFLICT) {
        lv_label_set_text_fmt(label, "%" LV_PRId32 " s", seconds);
    }
}

static void arrows_changed(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * content = lv_observer_get_target_obj(observer);
    lv_area_t area;
    lv_obj_get_coords(content, &area);
    area.x2 = area.x1 + BEARING_X + TRAFFIC_BEARING_WIDTH - 1;
    area.x1 += TREND_X;
    lv_obj_invalidate_area(content, &area);
}

static void style_changed(lv_observer_t * observer, lv_subject_t * subject)
{
    lv_obj_t * row = lv_observer_get_target_obj(observer);
    const traffic_row_state_t * state = lv_obj_get_user_data(row);
    const int32_t attention = lv_subject_get_int(state->subjects[ROW_ATTENTION]);
    const int32_t source = lv_subject_get_int(state->subjects[ROW_SOURCE]);
    const bool muted = attention == ATTENTION_MUTED;
    const bool filled = attention >= TRAFFIC_LEVEL_ADVISORY;
    const bool alert = attention == TRAFFIC_LEVEL_ALERT;
    lv_color_t text = !filled ? lv_subject_get_color(muted ? color_label_2 : color_label)
                      : alert ? TRAFFIC_COLOR_ON_RED : TRAFFIC_COLOR_ON_YELLOW;
    lv_obj_t * content = lv_obj_get_child(row, ROW_CONTENT);
    lv_obj_set_style_bg_opa(content, filled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(content, lv_subject_get_color(alert ? color_red : color_yellow), 0);
    lv_obj_set_style_text_color(content, text, 0);

    lv_obj_t * pill = lv_obj_get_child(content, CONTENT_PILL);
    lv_obj_set_style_bg_color(pill, filled ? text : lv_subject_get_color(source == TRAFFIC_FLARM ? color_pill_flarm : color_pill), 0);
    lv_obj_set_style_text_color(pill, filled || muted ? text
                                : lv_subject_get_color(source == TRAFFIC_FLARM ? color_pill_flarm_label : color_pill_label), 0);
    lv_obj_set_style_bg_opa(pill, !filled ? LV_OPA_COVER : alert ? LV_OPA_20 : LV_OPA_10, 0);

    lv_obj_t * bell = lv_obj_get_child(content, CONTENT_BELL);
    if(subject == state->subjects[ROW_SOURCE]) {
        static const char * const source_labels[] = {
            [TRAFFIC_ADSB] = "ADS-B", [TRAFFIC_ADSR] = "ADS-R", [TRAFFIC_TISB] = "TIS-B",
            [TRAFFIC_FLARM] = "FLARM", [TRAFFIC_UNKNOWN_SOURCE] = "?"
        };
        lv_label_set_text_static(pill, source_labels[source]);
        lv_obj_align_to(bell, pill, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
        lv_obj_align_to(lv_obj_get_child(content, CONTENT_CONFLICT_TIME), pill, LV_ALIGN_OUT_RIGHT_MID, CONFLICT_TIME_GAP_PX, 0);
    }
    if(subject == state->subjects[ROW_ATTENTION]) {
        lv_obj_set_hidden(bell, !muted);
        lv_obj_t * mute = lv_obj_get_child(row, ROW_MUTE);
        lv_image_set_src(lv_obj_get_child(mute, ACTION_ICON), muted ? icon_bell : icon_bell_slash);
        lv_label_set_translation_tag(lv_obj_get_child(mute, ACTION_LABEL), muted ? "unmute" : "mute");
    }
}

static void bind_row(lv_obj_t * row)
{
    const traffic_row_state_t * state = lv_obj_get_user_data(row);
    lv_obj_t * content = lv_obj_get_child(row, ROW_CONTENT);
    lv_label_bind_text(lv_obj_get_child(content, CONTENT_TITLE), state->subjects[ROW_TITLE], NULL);
    lv_subject_add_observer_obj(state->subjects[ROW_ALTITUDE], altitude_changed, lv_obj_get_child(content, CONTENT_ALT), NULL);
    lv_subject_add_observer_obj(state->subjects[ROW_DISTANCE], distance_changed, lv_obj_get_child(content, CONTENT_NM), NULL);
    lv_subject_add_observer_obj(state->subjects[ROW_CONFLICT], conflict_changed, lv_obj_get_child(content, CONTENT_CONFLICT_TIME), NULL);
    lv_subject_add_observer_obj(state->subjects[ROW_SOURCE], style_changed, row, NULL);
    lv_subject_add_observer_obj(state->subjects[ROW_ATTENTION], style_changed, row, NULL);
    lv_subject_add_observer_obj(subject_theme_dark, style_changed, row, NULL);
    lv_subject_add_observer_obj(state->subjects[ROW_TREND], arrows_changed, content, NULL);
    lv_subject_add_observer_obj(state->subjects[ROW_BEARING], arrows_changed, content, NULL);
}

static void update_row(lv_obj_t * row, const traffic_row_t * data)
{
    traffic_row_state_t * state = lv_obj_get_user_data(row);
    const char * title = data->title ? data->title : "";
    if(!state->title_buffers || lv_strcmp(state->title_buffers, title) != 0) {
        const size_t length = lv_strlen(title) + 1;
        if(length > state->title_capacity) {
            const size_t capacity = LV_MAX(length, 9u);
            char * buffers = lv_realloc(state->title_buffers, 2 * capacity);
            if(!buffers) {
                return;
            }
            state->title_buffers = buffers;
            state->title_capacity = capacity;
            lv_subject_set_string_buffer_static(state->subjects[ROW_TITLE], buffers, buffers + capacity, capacity);
        }
        lv_subject_set_string(state->subjects[ROW_TITLE], title);
    }
    state->target_id = data->target_id;
    lv_subject_set_int(state->subjects[ROW_SOURCE], data->source);
    lv_subject_set_int(state->subjects[ROW_ATTENTION], data->muted ? ATTENTION_MUTED : (int32_t)data->level);
    lv_subject_set_int(state->subjects[ROW_CONFLICT], data->muted || data->level < TRAFFIC_LEVEL_ADVISORY ? TRAFFIC_NO_CONFLICT : data->conflict_in_s);
    /* Preserve the sign of an altitude displayed as minus zero hundreds. */
    int32_t altitude = data->relative_altitude_ft / 100 * 100;
    if(data->relative_altitude_ft < 0 && altitude == 0) {
        altitude = -1;
    }
    lv_subject_set_int(state->subjects[ROW_ALTITUDE], data->relative_altitude_unknown ? UNKNOWN_VALUE : altitude);
    lv_subject_set_int(state->subjects[ROW_DISTANCE], data->relative_position_available ? (int32_t)(data->distance_nm * 10 + 0.5f) : UNKNOWN_VALUE);
    lv_subject_set_int(state->subjects[ROW_TREND], data->vertical_trend);
    lv_subject_set_int(state->subjects[ROW_BEARING], data->relative_position_available ? data->relative_bearing_deg : UNKNOWN_VALUE);

    lv_obj_t * own = lv_obj_get_child(row, ROW_OWN);
    if(lv_obj_is_hidden(own) == data->own_ship_offered) {
        lv_obj_set_hidden(own, !data->own_ship_offered);
        lv_obj_update_layout(row);
        lv_obj_scroll_to_x(row, closed_row_scroll_x(row), LV_ANIM_OFF);
        fade_actions(row);
    }
}

static void draw_trend_and_bearing(lv_event_t * e)
{
    lv_obj_t * content = lv_event_get_current_target_obj(e);
    const traffic_row_state_t * state = lv_event_get_user_data(e);
    lv_layer_t * layer = lv_event_get_layer(e);

    lv_area_t area;
    lv_obj_get_coords(content, &area);
    int32_t middle = area.y1 + lv_area_get_height(&area) / 2;

    lv_draw_triangle_dsc_t shape;
    lv_draw_triangle_dsc_init(&shape);
    shape.color = lv_obj_get_style_text_color(content, LV_PART_MAIN);

    lv_point_t trend = {area.x1 + TREND_CENTER_X, middle + 1};
    traffic_draw_trend(layer, trend, (traffic_vertical_trend_t)lv_subject_get_int(state->subjects[ROW_TREND]), shape.color, false);

    const int32_t bearing = lv_subject_get_int(state->subjects[ROW_BEARING]);
    if(bearing != UNKNOWN_VALUE) {
        /* A dart of two triangles that share the tip and the notch: tip, right, notch, left. */
        static const lv_point_t dart[] = {{0, -12}, {8, 10}, {0, 5}, {-8, 10}};
        int32_t sin = lv_trigo_sin((int16_t)bearing);
        int32_t cos = lv_trigo_cos((int16_t)bearing);
        lv_point_precise_t p[4];
        for(uint32_t i = 0; i < 4; i++) {
            p[i].x = area.x1 + BEARING_CENTER_X + ((dart[i].x * cos - dart[i].y * sin) >> LV_TRIGO_SHIFT);
            p[i].y = middle + ((dart[i].x * sin + dart[i].y * cos) >> LV_TRIGO_SHIFT);
        }
        shape.p[0] = p[0];
        shape.p[1] = p[1];
        shape.p[2] = p[2];
        lv_draw_triangle(layer, &shape);
        shape.p[1] = p[3];
        lv_draw_triangle(layer, &shape);
    }
}

static int32_t closed_row_scroll_x(lv_obj_t * row)
{
    return lv_obj_is_hidden(lv_obj_get_child(row, ROW_OWN)) ? 0 : SWIPE_ACTION_WIDTH;
}

/* LVGL 9.6.0 lets one fast move of the finger carry the content past its end, see elastic_diff() in
 * lv_indev_scroll.c. This takes it back before it is drawn. */
static void keep_inside(lv_obj_t * obj)
{
    int32_t x = LV_MIN(lv_obj_get_scroll_left(obj), 0) - LV_MIN(lv_obj_get_scroll_right(obj), 0);
    int32_t y = LV_MIN(lv_obj_get_scroll_top(obj), 0) - LV_MIN(lv_obj_get_scroll_bottom(obj), 0);
    if(x || y) {
        lv_obj_scroll_by_raw(obj, x, y);
    }
}

static lv_obj_tree_walk_res_t find_pressed_child(lv_obj_t * obj, void * pressed)
{
    if(!lv_obj_has_state(obj, LV_STATE_PRESSED)) {
        return LV_OBJ_TREE_WALK_NEXT;
    }
    *(bool *)pressed = true;
    return LV_OBJ_TREE_WALK_END;
}

static bool rows_locked(lv_obj_t * table)
{
    for(lv_indev_t * indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev)) {
        if(lv_indev_get_state(indev) != LV_INDEV_STATE_PRESSED) {
            continue;
        }
        bool pressed = false;
        lv_obj_tree_walk(table, find_pressed_child, &pressed);
        if(pressed) {
            return true;
        }
        break;
    }
    if(lv_obj_is_scrolling(table)) {
        return true;
    }
    for(uint32_t i = 0; i < lv_obj_get_child_count(table); i++) {
        lv_obj_t * row = lv_obj_get_child(table, (int32_t)i);
        if(lv_obj_is_scrolling(row) || lv_obj_get_scroll_x(row) != closed_row_scroll_x(row)) {
            return true;
        }
    }
    return false;
}

static void close_rows(lv_obj_t * table, lv_obj_t * except, lv_anim_enable_t anim)
{
    for(uint32_t i = 0; i < lv_obj_get_child_count(table); i++) {
        lv_obj_t * row = lv_obj_get_child(table, (int32_t)i);
        if(row != except && lv_obj_get_scroll_x(row) != closed_row_scroll_x(row)) {
            lv_obj_scroll_to_x(row, closed_row_scroll_x(row), anim);
        }
    }
}

static void fade_action(lv_obj_t * action, int32_t uncovered)
{
    int32_t way = LV_CLAMP(0, uncovered, SWIPE_ACTION_WIDTH) * 255 / SWIPE_ACTION_WIDTH;
    lv_obj_set_style_opa(action, INITIAL_ACTION_OPACITY + (LV_OPA_COVER - INITIAL_ACTION_OPACITY) * way * way / (255 * 255), 0);
}

static void fade_actions(lv_obj_t * row)
{
    int32_t moved = lv_obj_get_scroll_x(row) - closed_row_scroll_x(row);
    fade_action(lv_obj_get_child(row, ROW_MUTE), moved);
    fade_action(lv_obj_get_child(row, ROW_OWN), -moved);
}

static void row_scrolled(lv_event_t * e)
{
    lv_obj_t * row = lv_event_get_current_target_obj(e);
    if(lv_event_get_code(e) == LV_EVENT_SCROLL) {
        keep_inside(row);
        fade_actions(row);
        return;
    }
    if(lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
        close_rows(lv_obj_get_parent(row), row, LV_ANIM_ON);
        return;
    }
    int32_t rest = closed_row_scroll_x(row);
    int32_t x = lv_obj_get_scroll_x(row);
    int32_t stop = x > rest + SWIPE_ACTION_WIDTH / 2 ? rest + SWIPE_ACTION_WIDTH : x < rest - SWIPE_ACTION_WIDTH / 2 ? 0 : rest;
    if(x != stop) {
        lv_obj_scroll_to_x(row, stop, LV_ANIM_ON);
    }
}

static void action_clicked(lv_event_t * e)
{
    lv_obj_t * row = lv_obj_get_parent(lv_event_get_current_target_obj(e));
    const traffic_row_state_t * state = lv_obj_get_user_data(row);
    bool own = (intptr_t)lv_event_get_user_data(e) == ROW_OWN;
    const bool muted = lv_subject_get_int(state->subjects[ROW_ATTENTION]) == ATTENTION_MUTED;
    report(row, own ? TRAFFIC_ACTION_OWN_SHIP : muted ? TRAFFIC_ACTION_UNMUTE : TRAFFIC_ACTION_MUTE);
}

static void bell_clicked(lv_event_t * e)
{
    report(lv_obj_get_parent(lv_obj_get_parent(lv_event_get_current_target_obj(e))), TRAFFIC_ACTION_UNMUTE);
}

/* The row closes first: the answer to the action is a new set of rows, and an open row would hold it off. */
static void report(lv_obj_t * row, traffic_action_t action)
{
    traffic_table_t * table = (traffic_table_t *)lv_obj_get_parent(row);
    uint32_t target_id = ((const traffic_row_state_t *)lv_obj_get_user_data(row))->target_id;
    lv_obj_scroll_to_x(row, closed_row_scroll_x(row), LV_ANIM_OFF);
    if(table->on_action) {
        table->on_action(action, target_id);
    }
}

static void pressed_anywhere(lv_event_t * e)
{
    lv_obj_t * table = lv_event_get_user_data(e);
    lv_obj_t * row = lv_event_get_param(e);
    while(row && lv_obj_get_parent(row) != table) {
        row = lv_obj_get_parent(row);
    }
    close_rows(table, row, LV_ANIM_ON);
}

static void close_idle(lv_timer_t * timer)
{
    if(lv_display_get_inactive_time(NULL) >= ROW_CLOSE_DELAY_MS) {
        close_rows(lv_timer_get_user_data(timer), NULL, LV_ANIM_ON);
    }
}
