#ifndef LVGL_PRO_KEYBOARD_PRIVATE_GEN_H
#define LVGL_PRO_KEYBOARD_PRIVATE_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl_private.h"
#else
    #include "lvgl_private/lvgl_private.h"
#endif
#include "keyboard.h"

#ifndef KEYBOARD_USER_DATA
#define KEYBOARD_USER_DATA
#endif

typedef struct {
    lv_obj_t obj;
    const char * layout;
    const char * hint;
    KEYBOARD_USER_DATA
} keyboard_t;

extern const lv_obj_class_t keyboard_class;

#if defined(LV_USE_XML) && LV_USE_XML
    void keyboard_register(void);
#endif

#ifdef __cplusplus
}
#endif

#endif