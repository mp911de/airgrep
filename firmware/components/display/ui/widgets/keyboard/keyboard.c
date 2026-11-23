#include "keyboard_private_gen.h"
#include "airgrep_ui.h"

#define WIDTH 800
#define TOP 24
#define KEY_HEIGHT 72
#define GAP 8
#define ROWS 4

#define HEX_SIDE 16
#define HEX_KEY_WIDTH 120
#define HEX_BLOCK_GAP 16
#define HEX_BLOCK_WIDTH (3 * HEX_KEY_WIDTH + 2 * GAP)
#define HEX_LETTERS_X (HEX_SIDE + HEX_BLOCK_WIDTH + HEX_BLOCK_GAP)
#define HINT_INSET 8

#define KEY_WIDTH 70
#define SAVE_WIDTH 148

#define COLOR_ON_TINT lv_color_hex(0xffffff)


static void build_pad(lv_obj_t * keyboard, bool letters);
static void show_hint(lv_obj_t * keyboard);
static void build_rows(lv_obj_t * keyboard, const char * const * rows);
static lv_obj_t * create_key(lv_obj_t * keyboard, int32_t x, int32_t row, int32_t width, lv_subject_t * fill);
static void create_character(lv_obj_t * keyboard, int32_t x, int32_t row, int32_t width, char character);
static void create_delete(lv_obj_t * keyboard, int32_t x, int32_t row, int32_t width);
static void create_save(lv_obj_t * keyboard, int32_t x, int32_t row, int32_t width);
static void key_clicked(lv_event_t * e);

static const char * const english_rows[ROWS] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
static const char * const german_rows[ROWS] = {"1234567890", "QWERTZUIOP", "ASDFGHJKL", "YXCVBNM"};

void keyboard_constructor_hook(lv_obj_t * obj)
{
    LV_UNUSED(obj);
}

void keyboard_destructor_hook(lv_obj_t * obj)
{
    LV_UNUSED(obj);
}

void keyboard_event_hook(lv_event_t * e)
{
    LV_UNUSED(e);
}

void keyboard_set_layout(lv_obj_t * keyboard, const char * layout)
{
    lv_obj_clean(keyboard);
    ((keyboard_t *)keyboard)->hint_line = NULL;
    if(lv_streq(layout, "hex") || lv_streq(layout, "digits")) {
        build_pad(keyboard, lv_streq(layout, "hex"));
    }
    else {
        build_rows(keyboard, lv_streq(layout, "de") ? german_rows : english_rows);
    }
    show_hint(keyboard);
}

/* The tag must live as long as the keyboard. */
void keyboard_set_hint(lv_obj_t * keyboard, const char * hint)
{
    ((keyboard_t *)keyboard)->hint = hint;
    show_hint(keyboard);
}

void keyboard_set_cb(lv_obj_t * keyboard, keyboard_cb_t cb)
{
    ((keyboard_t *)keyboard)->on_key = cb;
}

void keyboard_set_save_active(lv_obj_t * keyboard, bool active)
{
    lv_obj_t * save = ((keyboard_t *)keyboard)->save;
    lv_obj_set_state(save, LV_STATE_DISABLED, !active);
    /* A recolour is not inherited, so the check follows by its own state. */
    lv_obj_set_state(lv_obj_get_child(save, 0), LV_STATE_DISABLED, !active);
}

static void build_pad(lv_obj_t * keyboard, bool letters)
{
    const char * digits = "123456789";
    for(int32_t i = 0; digits[i]; i++) {
        create_character(keyboard, HEX_SIDE + i % 3 * (HEX_KEY_WIDTH + GAP), i / 3, HEX_KEY_WIDTH, digits[i]);
    }
    create_character(keyboard, HEX_SIDE + HEX_KEY_WIDTH + GAP, 3, HEX_KEY_WIDTH, '0');
    create_delete(keyboard, HEX_SIDE + 2 * (HEX_KEY_WIDTH + GAP), 3, HEX_KEY_WIDTH);

    const char * hex = letters ? "ABCDEF" : "";
    for(int32_t i = 0; hex[i]; i++) {
        create_character(keyboard, HEX_LETTERS_X + i % 3 * (HEX_KEY_WIDTH + GAP), i / 3, HEX_KEY_WIDTH, hex[i]);
    }
    create_save(keyboard, HEX_LETTERS_X, 3, HEX_BLOCK_WIDTH);
}

static void show_hint(lv_obj_t * keyboard)
{
    keyboard_t * widget = (keyboard_t *)keyboard;
    if(widget->hint_line) {
        lv_obj_delete(widget->hint_line);
    }
    widget->hint_line = NULL;
    if(!widget->hint || !widget->hint[0]) {
        return;
    }

    lv_obj_t * line = lv_label_create(keyboard);
    lv_obj_set_pos(line, HEX_LETTERS_X + HINT_INSET, TOP + HINT_INSET);
    lv_obj_set_width(line, HEX_BLOCK_WIDTH - 2 * HINT_INSET);
    lv_label_set_translation_tag(line, widget->hint);
    lv_obj_set_style_text_font(line, font_text, 0);
    lv_obj_bind_style_prop(line, LV_STYLE_TEXT_COLOR, 0, color_label_2);
    widget->hint_line = line;
}

static void build_rows(lv_obj_t * keyboard, const char * const * rows)
{
    for(int32_t row = 0; row < ROWS; row++) {
        int32_t count = (int32_t)lv_strlen(rows[row]);
        int32_t width = count * (KEY_WIDTH + GAP) - GAP;
        if(row == ROWS - 1) {
            width += GAP + KEY_WIDTH + GAP + SAVE_WIDTH;
        }
        int32_t x = (WIDTH - width) / 2;
        for(int32_t i = 0; i < count; i++) {
            create_character(keyboard, x, row, KEY_WIDTH, rows[row][i]);
            x += KEY_WIDTH + GAP;
        }
        if(row == ROWS - 1) {
            create_delete(keyboard, x, row, KEY_WIDTH);
            create_save(keyboard, x + KEY_WIDTH + GAP, row, SAVE_WIDTH);
        }
    }
}

static lv_obj_t * create_key(lv_obj_t * keyboard, int32_t x, int32_t row, int32_t width, lv_subject_t * fill)
{
    lv_obj_t * key = lv_obj_create(keyboard);
    lv_obj_remove_style_all(key);
    lv_obj_set_pos(key, x, TOP + row * (KEY_HEIGHT + GAP));
    lv_obj_set_size(key, width, KEY_HEIGHT);
    lv_obj_set_scrollable(key, false);
    lv_obj_set_style_radius(key, RADIUS, 0);
    lv_obj_set_style_bg_opa(key, LV_OPA_COVER, 0);
    lv_obj_bind_style_prop(key, LV_STYLE_BG_COLOR, 0, fill);
    lv_obj_add_style(key, &style_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(key, key_clicked, LV_EVENT_CLICKED, NULL);
    return key;
}

static void create_character(lv_obj_t * keyboard, int32_t x, int32_t row, int32_t width, char character)
{
    lv_obj_t * key = create_key(keyboard, x, row, width, color_surface);
    lv_obj_set_user_data(key, (void *)(intptr_t)character);
    lv_obj_t * label = lv_label_create(key);
    char text[2] = {character, '\0'};
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, font_data, 0);
    lv_obj_bind_style_prop(label, LV_STYLE_TEXT_COLOR, 0, color_label);
}

static void create_delete(lv_obj_t * keyboard, int32_t x, int32_t row, int32_t width)
{
    lv_obj_t * key = create_key(keyboard, x, row, width, color_control);
    lv_obj_set_user_data(key, (void *)(intptr_t)KEYBOARD_KEY_DELETE);
    lv_obj_t * icon = lv_image_create(key);
    lv_image_set_src(icon, icon_delete);
    lv_obj_center(icon);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_bind_style_prop(icon, LV_STYLE_IMAGE_RECOLOR, 0, color_label);
}

static void create_save(lv_obj_t * keyboard, int32_t x, int32_t row, int32_t width)
{
    lv_obj_t * key = create_key(keyboard, x, row, width, color_tint);
    lv_obj_set_user_data(key, (void *)(intptr_t)KEYBOARD_KEY_SAVE);
    lv_obj_bind_style_prop(key, LV_STYLE_BG_COLOR, LV_STATE_DISABLED, color_control);
    lv_obj_t * icon = lv_image_create(key);
    lv_image_set_src(icon, icon_check);
    lv_obj_center(icon);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_set_style_image_recolor(icon, COLOR_ON_TINT, 0);
    lv_obj_bind_style_prop(icon, LV_STYLE_IMAGE_RECOLOR, LV_STATE_DISABLED, color_label_2);
    ((keyboard_t *)keyboard)->save = key;
    keyboard_set_save_active(keyboard, false);
}

static void key_clicked(lv_event_t * e)
{
    lv_obj_t * key = lv_event_get_current_target_obj(e);
    keyboard_t * keyboard = (keyboard_t *)lv_obj_get_parent(key);
    if(keyboard->on_key) {
        keyboard->on_key(&keyboard->obj, (int)(intptr_t)lv_obj_get_user_data(key));
    }
}
