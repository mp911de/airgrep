#include "radar_gen.h"

#include <stdlib.h>

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl_private.h"
#else
    #include "lvgl_private/lvgl_private.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML

#include "lv_xml_private/lv_xml_private.h"

void * radar_xml_create(lv_xml_parser_state_t * state, const char ** attrs)
{
    LV_UNUSED(attrs);
    void * item = radar_create(lv_xml_state_get_parent(state));

    if(item == NULL) {
        LV_LOG_ERROR("Failed to create radar");
        return NULL;
    }

    return item;
}

void radar_xml_apply(lv_xml_parser_state_t * state, const char ** attrs)
{
    void * item = lv_xml_state_get_item(state);

    lv_xml_obj_apply(state, attrs);

    for(int i = 0; attrs[i]; i += 2) {
        const char * name = attrs[i];
        const char * value = attrs[i + 1];

        if(lv_streq("range_nm", name)) radar_set_range_nm(item, atoi(value));
    }
}

void radar_register(void)
{
    lv_xml_register_widget("radar", radar_xml_create, radar_xml_apply);
}

#endif