#include "entry_field_private_gen.h"
#include "airgrep_ui.h"

#define PLACE_WIDTH 32
#define UNDERLINE 3

#define STATE_NEXT LV_STATE_USER_1
#define STATE_STORED LV_STATE_USER_2

static void create_place(lv_obj_t * field);

void entry_field_constructor_hook(lv_obj_t * obj)
{
    LV_UNUSED(obj);
}

void entry_field_destructor_hook(lv_obj_t * obj)
{
    LV_UNUSED(obj);
}

void entry_field_event_hook(lv_event_t * e)
{
    LV_UNUSED(e);
}

void entry_field_set(lv_obj_t * field, uint32_t places, const char * text, bool stored)
{
    while(lv_obj_get_child_count(field) > places) {
        lv_obj_delete(lv_obj_get_child(field, -1));
    }
    while(lv_obj_get_child_count(field) < places) {
        create_place(field);
    }

    uint32_t length = (uint32_t)lv_strlen(text);
    uint32_t next = stored ? 0 : length;
    for(uint32_t i = 0; i < places; i++) {
        lv_obj_t * place = lv_obj_get_child(field, (int32_t)i);
        lv_obj_t * character = lv_obj_get_child(place, 0);
        char shown[2] = {i < length ? text[i] : '\0', '\0'};
        lv_label_set_text(character, shown);
        lv_obj_set_state(place, STATE_NEXT, i == next);
        lv_obj_set_state(character, STATE_STORED, stored);
    }
}

static void create_place(lv_obj_t * field)
{
    lv_obj_t * place = lv_obj_create(field);
    lv_obj_remove_style_all(place);
    lv_obj_set_size(place, PLACE_WIDTH, lv_pct(100));
    lv_obj_set_clickable(place, false);
    lv_obj_set_scrollable(place, false);
    lv_obj_set_style_border_side(place, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(place, UNDERLINE, 0);
    lv_obj_bind_style_prop(place, LV_STYLE_BORDER_COLOR, 0, color_grid);
    lv_obj_bind_style_prop(place, LV_STYLE_BORDER_COLOR, STATE_NEXT, color_tint);

    lv_obj_t * character = lv_label_create(place);
    lv_obj_center(character);
    lv_obj_set_style_text_font(character, font_data, 0);
    lv_obj_set_style_text_font(character, font_stored, STATE_STORED);
    lv_obj_bind_style_prop(character, LV_STYLE_TEXT_COLOR, 0, color_label);
    lv_obj_bind_style_prop(character, LV_STYLE_TEXT_COLOR, STATE_STORED, color_grid);
}
