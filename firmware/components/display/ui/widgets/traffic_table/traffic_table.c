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
#define SCROLLBAR_WIDTH 4
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

static void draw_band(lv_event_t * e);
static int32_t nearest_row_stop_y(lv_obj_t * table, int32_t y);
static lv_obj_t * create_row(lv_obj_t * table, traffic_row_state_t * state);
static lv_obj_t * create_action(lv_obj_t * row);
static void update_row(lv_obj_t * row, const traffic_row_t * data);
static void draw_trend_and_bearing(lv_event_t * e);
static int32_t closed_row_scroll_x(lv_obj_t * row);
static bool rows_locked(lv_obj_t * table);
static void keep_inside(lv_obj_t * obj);
static void refresh_locked_rows(lv_obj_t * table, const traffic_row_t * rows, uint32_t count);
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
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(obj, LV_DIR_VER);
    lv_obj_set_scroll_elastic(obj, false);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_style_width(obj, SCROLLBAR_WIDTH, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(obj, SCROLLBAR_WIDTH / 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(obj, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_SCROLLBAR);

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
    traffic_table_t * widget = (traffic_table_t *)table;
    count = LV_MIN(count, TRAFFIC_TABLE_MAX_ROWS);
    lv_obj_set_style_bg_color(table, lv_subject_get_color(color_grid), LV_PART_SCROLLBAR);
    if(rows_locked(table)) {
        refresh_locked_rows(table, rows, count);
        return;
    }

    uint32_t previous_count = lv_obj_get_child_count(table);
    uint32_t previous_divider = widget->first_outside_row;
    while(lv_obj_get_child_count(table) > count) {
        lv_obj_delete(lv_obj_get_child(table, -1));
    }
    while(lv_obj_get_child_count(table) < count) {
        create_row(table, &widget->rows[lv_obj_get_child_count(table)]);
    }

    widget->first_outside_row = LV_MIN(inside, count);
    bool had_divider = previous_divider < previous_count && previous_divider < count;
    if(had_divider && previous_divider != widget->first_outside_row) {
        lv_obj_set_style_margin_top(lv_obj_get_child(table, (int32_t)previous_divider), 0, 0);
    }
    if(widget->first_outside_row < count && (!had_divider || previous_divider != widget->first_outside_row)) {
        lv_obj_set_style_margin_top(lv_obj_get_child(table, (int32_t)widget->first_outside_row), BAND, 0);
    }
    for(uint32_t i = 0; i < count; i++) {
        lv_obj_t * row = lv_obj_get_child(table, (int32_t)i);
        widget->rows[i].target_id = rows[i].target_id;
        update_row(row, &rows[i]);
    }
}

void traffic_table_set_band(lv_obj_t * table, const char * text)
{
    traffic_table_t * widget = (traffic_table_t *)table;
    if(lv_strcmp(widget->band, text) == 0) {
        return;
    }
    lv_strlcpy(widget->band, text, sizeof(widget->band));
    lv_obj_invalidate(table);
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

static void draw_band(lv_event_t * e)
{
    lv_obj_t * table = lv_event_get_current_target_obj(e);
    const traffic_table_t * widget = (const traffic_table_t *)table;
    if(widget->first_outside_row >= lv_obj_get_child_count(table)) {
        return;
    }

    lv_area_t area;
    lv_obj_get_coords(lv_obj_get_child(table, (int32_t)widget->first_outside_row), &area);
    area.y2 = area.y1 - 1;
    area.y1 -= BAND;
    area.x2 = area.x1 + TRAFFIC_WIDTH - 1;

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
static lv_obj_t * create_row(lv_obj_t * table, traffic_row_state_t * state)
{
    lv_obj_t * row = lv_obj_create(table);
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
    lv_obj_add_event_cb(mute, action_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)ROW_MUTE);
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

static void update_row(lv_obj_t * row, const traffic_row_t * data)
{
    traffic_row_state_t * state = lv_obj_get_user_data(row);
    state->muted = data->muted;
    state->vertical_trend = data->vertical_trend;
    state->relative_position_available = data->relative_position_available;
    state->relative_bearing_deg = data->relative_bearing_deg;

    bool filled = !data->muted && data->level >= TRAFFIC_LEVEL_ADVISORY;
    bool alert = data->level == TRAFFIC_LEVEL_ALERT;
    lv_color_t text = !filled ? lv_subject_get_color(data->muted ? color_label_2 : color_label)
                      : alert ? TRAFFIC_COLOR_ON_RED : TRAFFIC_COLOR_ON_YELLOW;

    lv_obj_set_style_border_color(row, lv_subject_get_color(color_separator), 0);

    lv_obj_t * content = lv_obj_get_child(row, ROW_CONTENT);
    lv_obj_set_style_bg_opa(content, filled ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(content, lv_subject_get_color(alert ? color_red : color_yellow), 0);
    lv_obj_set_style_text_color(content, text, 0);

    lv_label_set_text(lv_obj_get_child(content, CONTENT_TITLE), data->title);

    lv_obj_t * pill = lv_obj_get_child(content, CONTENT_PILL);
    static const char * const source_labels[] = {
        [TRAFFIC_ADSB] = "ADS-B", [TRAFFIC_ADSR] = "ADS-R", [TRAFFIC_TISB] = "TIS-B",
        [TRAFFIC_FLARM] = "FLARM", [TRAFFIC_UNKNOWN_SOURCE] = "?"
    };
    lv_label_set_text(pill, source_labels[data->source]);
    lv_obj_set_style_bg_color(pill, filled ? text : lv_subject_get_color(data->source == TRAFFIC_FLARM ? color_pill_flarm : color_pill), 0);
    lv_obj_set_style_text_color(pill, filled || data->muted ? text
                                : lv_subject_get_color(data->source == TRAFFIC_FLARM ? color_pill_flarm_label : color_pill_label), 0);
    lv_obj_set_style_bg_opa(pill, !filled ? LV_OPA_COVER : alert ? LV_OPA_20 : LV_OPA_10, 0);

    lv_obj_t * bell = lv_obj_get_child(content, CONTENT_BELL);
    lv_obj_set_hidden(bell, !data->muted);
    lv_obj_set_style_image_recolor(bell, lv_subject_get_color(color_tint), 0);
    lv_obj_align_to(bell, pill, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    char value[12];
    lv_obj_t * conflict_seconds = lv_obj_get_child(content, CONTENT_CONFLICT_TIME);
    lv_obj_set_hidden(conflict_seconds, !filled || data->conflict_in_s == TRAFFIC_NO_CONFLICT);
    lv_snprintf(value, sizeof(value), "%u s", (unsigned)data->conflict_in_s);
    lv_label_set_text(conflict_seconds, value);
    lv_obj_align_to(conflict_seconds, pill, LV_ALIGN_OUT_RIGHT_MID, CONFLICT_TIME_GAP_PX, 0);

    traffic_format_altitude(value, sizeof(value), data->relative_altitude_ft, data->relative_altitude_unknown);
    lv_label_set_text(lv_obj_get_child(content, CONTENT_ALT), value);

    if(data->relative_position_available) {
        int32_t tenths = (int32_t)(data->distance_nm * 10 + 0.5f);
        lv_snprintf(value, sizeof(value), "%" LV_PRId32 ".%" LV_PRId32, tenths / 10, tenths % 10);
    }
    else {
        lv_snprintf(value, sizeof(value), "–");
    }
    lv_label_set_text(lv_obj_get_child(content, CONTENT_NM), value);

    /* Never red: red means act now, and neither action is an alert. */
    lv_obj_t * own = lv_obj_get_child(row, ROW_OWN);
    int32_t rest = closed_row_scroll_x(row);
    lv_obj_set_hidden(own, !data->own_ship_offered);
    lv_obj_set_style_bg_color(own, lv_subject_get_color(color_green), 0);
    lv_obj_set_style_text_color(own, lv_subject_get_color(color_on_green), 0);
    lv_obj_set_style_image_recolor(lv_obj_get_child(own, ACTION_ICON), lv_subject_get_color(color_on_green), 0);
    lv_label_set_text(lv_obj_get_child(own, ACTION_LABEL), lv_tr("own_ship"));

    lv_obj_t * mute = lv_obj_get_child(row, ROW_MUTE);
    lv_obj_set_style_bg_color(mute, lv_subject_get_color(color_tint), 0);
    lv_obj_set_style_text_color(mute, TRAFFIC_COLOR_ON_RED, 0);
    lv_obj_set_style_image_recolor(lv_obj_get_child(mute, ACTION_ICON), TRAFFIC_COLOR_ON_RED, 0);
    lv_image_set_src(lv_obj_get_child(mute, ACTION_ICON), data->muted ? icon_bell : icon_bell_slash);
    lv_label_set_text(lv_obj_get_child(mute, ACTION_LABEL), lv_tr(data->muted ? "unmute" : "mute"));

    /* Showing or hiding "Own ship" moves the content conflict_seconds the strip. */
    if(closed_row_scroll_x(row) != rest) {
        lv_obj_update_layout(row);
        lv_obj_scroll_to_x(row, closed_row_scroll_x(row), LV_ANIM_OFF);
    }
    fade_actions(row);
    lv_obj_invalidate(row);
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
    traffic_draw_trend(layer, trend, state->vertical_trend, shape.color, false);

    if(state->relative_position_available) {
        /* A dart of two triangles that share the tip and the notch: tip, right, notch, left. */
        static const lv_point_t dart[] = {{0, -12}, {8, 10}, {0, 5}, {-8, 10}};
        int32_t sin = lv_trigo_sin(state->relative_bearing_deg);
        int32_t cos = lv_trigo_cos(state->relative_bearing_deg);
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

static void refresh_locked_rows(lv_obj_t * table, const traffic_row_t * rows, uint32_t count)
{
    for(uint32_t i = 0; i < lv_obj_get_child_count(table); i++) {
        lv_obj_t * row = lv_obj_get_child(table, (int32_t)i);
        uint32_t target_id = ((const traffic_row_state_t *)lv_obj_get_user_data(row))->target_id;
        for(uint32_t j = 0; j < count; j++) {
            if(rows[j].target_id != target_id) {
                continue;
            }
            traffic_row_t held = rows[j];
            held.own_ship_offered = closed_row_scroll_x(row) != 0;
            update_row(row, &held);
            break;
        }
    }
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
    report(row, own ? TRAFFIC_ACTION_OWN_SHIP : state->muted ? TRAFFIC_ACTION_UNMUTE : TRAFFIC_ACTION_MUTE);
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
