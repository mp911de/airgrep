#include "entry_field_private_gen.h"
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif
#include "../../airgrep_ui.h"

void entry_field_constructor_hook(lv_obj_t * obj);
void entry_field_destructor_hook(lv_obj_t * obj);
void entry_field_event_hook(lv_event_t * e);

static void entry_field_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void entry_field_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void entry_field_event(const lv_obj_class_t * class_p, lv_event_t * e);

const lv_obj_class_t entry_field_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = entry_field_constructor,
    .destructor_cb = entry_field_destructor,
    .event_cb = entry_field_event,
    .instance_size = sizeof(entry_field_t),
    .editable = 1,
    .name = "entry_field"
};

lv_obj_t * entry_field_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(&entry_field_class, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

static void entry_field_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    entry_field_t * widget = (entry_field_t *)obj;

    static bool style_inited = false;

    if (!style_inited) {

        style_inited = true;
    }

    lv_obj_t * the_root = NULL;
    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_set_width(obj, LV_SIZE_CONTENT);
        lv_obj_set_height(obj, 48);
        lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_clickable(obj, false);
        lv_obj_set_scrollable(obj, false);
        lv_obj_set_style_pad_column(obj, 8, 0);

        lv_obj_remove_theme(obj, LV_PART_MAIN);

        the_root = obj;
    }
    #endif
    entry_field_constructor_hook(obj);

    LV_TRACE_OBJ_CREATE("finished");
}

static void entry_field_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    entry_field_destructor_hook(obj);
}

static void entry_field_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_result_t res;

    res = lv_obj_event_base(&entry_field_class, e);
    if(res != LV_RESULT_OK) return;

    entry_field_event_hook(e);
}

