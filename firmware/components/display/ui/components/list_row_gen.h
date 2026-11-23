#ifndef LVGL_PRO_LIST_ROW_GEN_H
#define LVGL_PRO_LIST_ROW_GEN_H

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

lv_obj_t * list_row_create(lv_obj_t * parent, const char * label, bool separator);
lv_obj_t * list_row_get_value(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif

#endif