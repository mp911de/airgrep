#ifndef LVGL_PRO_PAGE_GEN_H
#define LVGL_PRO_PAGE_GEN_H

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

typedef enum {
    PAGE_TIMELINE_TIMELINE_PUSH = 0,
    _PAGE_TIMELINE_CNT = 1
}page_timeline_t;

lv_obj_t * page_create(lv_obj_t * parent, int32_t page);

/**
 * Get a timeline of a page
 * @param obj          pointer to a page component
 * @param timeline_id  ID of the timeline
 * @return             pointer to the timeline or NULL if not found
 */
lv_anim_timeline_t * page_get_timeline(lv_obj_t * obj, page_timeline_t timeline_id);

#ifdef __cplusplus
}
#endif

#endif