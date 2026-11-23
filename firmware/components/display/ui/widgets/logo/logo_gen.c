#include "logo_private_gen.h"
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif
#include "../../airgrep_ui.h"

void logo_constructor_hook(lv_obj_t * obj);
void logo_destructor_hook(lv_obj_t * obj);
void logo_event_hook(lv_event_t * e);

static void logo_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void logo_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void logo_event(const lv_obj_class_t * class_p, lv_event_t * e);

const lv_obj_class_t logo_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = logo_constructor,
    .destructor_cb = logo_destructor,
    .event_cb = logo_event,
    .instance_size = sizeof(logo_t),
    .editable = 1,
    .name = "logo"
};

lv_obj_t * logo_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(&logo_class, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

static void logo_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    logo_t * widget = (logo_t *)obj;

    static bool style_inited = false;

    if (!style_inited) {

        style_inited = true;
    }

    lv_obj_t * the_root = NULL;
    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_set_width(obj, 480);
        lv_obj_set_height(obj, 126);
        lv_obj_set_clickable(obj, false);
        lv_obj_set_scrollable(obj, false);

        lv_obj_remove_theme(obj, LV_PART_MAIN);

        the_root = obj;
    }
    #endif
    logo_constructor_hook(obj);

    LV_TRACE_OBJ_CREATE("finished");
}

static void logo_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    logo_destructor_hook(obj);
}

static void logo_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_result_t res;

    res = lv_obj_event_base(&logo_class, e);
    if(res != LV_RESULT_OK) return;

    logo_event_hook(e);
}

