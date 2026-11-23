#ifndef LVGL_PRO_CAUSE_ROW_GEN_H
#define LVGL_PRO_CAUSE_ROW_GEN_H

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

lv_obj_t * cause_row_create(lv_obj_t * parent, const char * title, const char * value, bool separator);

#ifdef __cplusplus
}
#endif

#endif