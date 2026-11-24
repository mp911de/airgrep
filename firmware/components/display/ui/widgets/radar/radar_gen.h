#ifndef LVGL_PRO_RADAR_GEN_H
#define LVGL_PRO_RADAR_GEN_H

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

lv_obj_t * radar_create(lv_obj_t * parent);
/**
 * Distance of the outer ring in NM
 * @param radar   pointer to a radar
 * @param range_nm  Distance of the outer ring in NM
 */
void radar_set_range_nm(lv_obj_t * radar, int32_t range_nm);
void radar_set_tracks(lv_obj_t * radar, bool tracks);
void radar_set_simple(lv_obj_t * radar, bool simple);
void radar_set_indicator(lv_obj_t * radar, bool indicator);

#ifdef __cplusplus
}
#endif

#endif
