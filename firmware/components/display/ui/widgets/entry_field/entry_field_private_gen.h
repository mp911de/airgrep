#ifndef LVGL_PRO_ENTRY_FIELD_PRIVATE_GEN_H
#define LVGL_PRO_ENTRY_FIELD_PRIVATE_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl_private.h"
#else
    #include "lvgl_private/lvgl_private.h"
#endif
#include "entry_field.h"

#ifndef ENTRY_FIELD_USER_DATA
#define ENTRY_FIELD_USER_DATA
#endif

typedef struct {
    lv_obj_t obj;
    ENTRY_FIELD_USER_DATA
} entry_field_t;

extern const lv_obj_class_t entry_field_class;

#if defined(LV_USE_XML) && LV_USE_XML
    void entry_field_register(void);
#endif

#ifdef __cplusplus
}
#endif

#endif