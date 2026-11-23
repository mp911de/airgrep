#ifndef LVGL_PRO_TRAFFIC_TABLE_GEN_H
#define LVGL_PRO_TRAFFIC_TABLE_GEN_H

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

lv_obj_t * traffic_table_create(lv_obj_t * parent);

#ifdef __cplusplus
}
#endif

#endif