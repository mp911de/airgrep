#ifndef LVGL_PRO_TRAFFIC_TABLE_PRIVATE_GEN_H
#define LVGL_PRO_TRAFFIC_TABLE_PRIVATE_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl_private.h"
#else
    #include "lvgl_private/lvgl_private.h"
#endif
#include "traffic_table.h"

#ifndef TRAFFIC_TABLE_USER_DATA
#define TRAFFIC_TABLE_USER_DATA
#endif

typedef struct {
    lv_obj_t obj;
    TRAFFIC_TABLE_USER_DATA
} traffic_table_t;

extern const lv_obj_class_t traffic_table_class;

#if defined(LV_USE_XML) && LV_USE_XML
    void traffic_table_register(void);
#endif

#ifdef __cplusplus
}
#endif

#endif