#ifndef LVGL_PRO_ENTRY_FIELD_H
#define LVGL_PRO_ENTRY_FIELD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "entry_field_gen.h"

/**
 * Show a text in a number of places. The text is copied.
 * A stored text is what was there before the user typed: it is lighter and the next key lands on the first place.
 * A typed text is followed by the place the next key lands on.
 */
void entry_field_set(lv_obj_t * field, uint32_t places, const char * text, bool stored);

/* The Editor's entry_field_gen.c calls these. */
void entry_field_constructor_hook(lv_obj_t * obj);
void entry_field_destructor_hook(lv_obj_t * obj);
void entry_field_event_hook(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif
