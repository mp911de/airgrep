#ifndef LVGL_PRO_LOGO_H
#define LVGL_PRO_LOGO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "logo_gen.h"

#define LOGO_USER_DATA uint32_t first_dot;

/* The Editor's logo_gen.c calls these. */
void logo_constructor_hook(lv_obj_t * obj);
void logo_destructor_hook(lv_obj_t * obj);
void logo_event_hook(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif
