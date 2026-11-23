#ifndef LVGL_PRO_KEYBOARD_GEN_H
#define LVGL_PRO_KEYBOARD_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif

#include "../../airgrep_ui_gen.h"

lv_obj_t * keyboard_create(lv_obj_t * parent);
/**
 * hex for 0 to 9 and A to F, digits for 0 to 9, or a language as en or de for 0 to 9 and A to Z in the order of that language
 * @param keyboard   pointer to a keyboard
 * @param layout  hex for 0 to 9 and A to F, digits for 0 to 9, or a language as en or de for 0 to 9 and A to Z in the order of that language
 */
void keyboard_set_layout(lv_obj_t * keyboard, const char * layout);

/**
 * Translation tag of one line that says what a value does, shown where digits has no letters
 * @param keyboard   pointer to a keyboard
 * @param hint  Translation tag of one line that says what a value does, shown where digits has no letters
 */
void keyboard_set_hint(lv_obj_t * keyboard, const char * hint);

#ifdef __cplusplus
}
#endif

#endif