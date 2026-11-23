#include "keyboard_private_gen.h"
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif
#include "../../airgrep_ui.h"

void keyboard_constructor_hook(lv_obj_t * obj);
void keyboard_destructor_hook(lv_obj_t * obj);
void keyboard_event_hook(lv_event_t * e);

static void keyboard_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void keyboard_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void keyboard_event(const lv_obj_class_t * class_p, lv_event_t * e);

const lv_obj_class_t keyboard_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = keyboard_constructor,
    .destructor_cb = keyboard_destructor,
    .event_cb = keyboard_event,
    .instance_size = sizeof(keyboard_t),
    .editable = 1,
    .name = "keyboard"
};

lv_obj_t * keyboard_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(&keyboard_class, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

static void keyboard_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    keyboard_t * widget = (keyboard_t *)obj;

    static bool style_inited = false;

    if (!style_inited) {

        style_inited = true;
    }

    lv_obj_t * the_root = NULL;
    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_set_width(obj, 800);
        lv_obj_set_height(obj, 352);
        lv_obj_set_scrollable(obj, false);

        lv_obj_remove_theme(obj, LV_PART_MAIN);

        the_root = obj;
    }
    #endif
    keyboard_constructor_hook(obj);

    LV_TRACE_OBJ_CREATE("finished");
}

static void keyboard_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    keyboard_destructor_hook(obj);
}

static void keyboard_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_result_t res;

    res = lv_obj_event_base(&keyboard_class, e);
    if(res != LV_RESULT_OK) return;

    keyboard_event_hook(e);
}

