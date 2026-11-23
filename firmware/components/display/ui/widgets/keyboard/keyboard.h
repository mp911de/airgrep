#ifndef LVGL_PRO_KEYBOARD_H
#define LVGL_PRO_KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "keyboard_gen.h"

enum { KEYBOARD_KEY_DELETE = -1, KEYBOARD_KEY_SAVE = -2 };
typedef void (*keyboard_cb_t)(lv_obj_t * keyboard, int key);

void keyboard_set_cb(lv_obj_t * keyboard, keyboard_cb_t cb);

void keyboard_set_save_active(lv_obj_t * keyboard, bool active);

#define KEYBOARD_USER_DATA keyboard_cb_t on_key; lv_obj_t * save; lv_obj_t * hint_line;

/* The Editor's keyboard_gen.c calls these. */
void keyboard_constructor_hook(lv_obj_t * obj);
void keyboard_destructor_hook(lv_obj_t * obj);
void keyboard_event_hook(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif
