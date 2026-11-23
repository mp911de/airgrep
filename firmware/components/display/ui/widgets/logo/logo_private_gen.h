#ifndef LVGL_PRO_LOGO_PRIVATE_GEN_H
#define LVGL_PRO_LOGO_PRIVATE_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl_private.h"
#else
    #include "lvgl_private/lvgl_private.h"
#endif
#include "logo.h"

#ifndef LOGO_USER_DATA
#define LOGO_USER_DATA
#endif

typedef struct {
    lv_obj_t obj;
    LOGO_USER_DATA
} logo_t;

extern const lv_obj_class_t logo_class;

#if defined(LV_USE_XML) && LV_USE_XML
    void logo_register(void);
#endif

#ifdef __cplusplus
}
#endif

#endif