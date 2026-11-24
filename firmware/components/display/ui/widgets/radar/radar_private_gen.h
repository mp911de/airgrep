#ifndef LVGL_PRO_RADAR_PRIVATE_GEN_H
#define LVGL_PRO_RADAR_PRIVATE_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl_private.h"
#else
    #include "lvgl_private/lvgl_private.h"
#endif
#include "radar.h"

#ifndef RADAR_USER_DATA
#define RADAR_USER_DATA
#endif

typedef struct {
    lv_obj_t obj;
    int32_t range_nm;
    bool tracks;
    bool simple;
    bool indicator;
    RADAR_USER_DATA
} radar_t;

extern const lv_obj_class_t radar_class;

#if defined(LV_USE_XML) && LV_USE_XML
    void radar_register(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
