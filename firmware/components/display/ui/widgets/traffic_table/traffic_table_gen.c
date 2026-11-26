#include "traffic_table_private_gen.h"
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif

#if defined(LV_USE_XML) && LV_USE_XML
    #include "lv_xml/lv_xml.h"
#endif
#include "../../airgrep_ui.h"

void traffic_table_constructor_hook(lv_obj_t * obj);
void traffic_table_destructor_hook(lv_obj_t * obj);
void traffic_table_event_hook(lv_event_t * e);

static void traffic_table_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void traffic_table_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void traffic_table_event(const lv_obj_class_t * class_p, lv_event_t * e);

const lv_obj_class_t traffic_table_class = {
    .base_class = &lv_obj_class,
    .constructor_cb = traffic_table_constructor,
    .destructor_cb = traffic_table_destructor,
    .event_cb = traffic_table_event,
    .instance_size = sizeof(traffic_table_t),
    .editable = 1,
    .name = "traffic_table"
};

lv_obj_t * traffic_table_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(&traffic_table_class, parent);
    lv_obj_class_init_obj(obj);

    return obj;
}

static void traffic_table_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");

    traffic_table_t * widget = (traffic_table_t *)obj;

    static bool style_inited = false;

    if (!style_inited) {

        style_inited = true;
    }

    lv_obj_t * the_root = NULL;
    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_set_width(obj, 400);
        lv_obj_set_height(obj, 384);
        lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(obj, LV_DIR_VER);
        lv_obj_set_scroll_elastic(obj, false);
        lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_ACTIVE);
        lv_obj_set_style_width(obj, 4, LV_PART_SCROLLBAR);
        lv_obj_set_style_radius(obj, 2, LV_PART_SCROLLBAR);
        lv_obj_set_style_pad_right(obj, 2, LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_SCROLLBAR);
        lv_obj_bind_style_prop(obj, LV_STYLE_BG_COLOR, LV_PART_SCROLLBAR, color_grid);

        lv_obj_remove_theme(obj, LV_PART_MAIN);

        the_root = obj;
    }
    #endif
    traffic_table_constructor_hook(obj);

    LV_TRACE_OBJ_CREATE("finished");
}

static void traffic_table_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);

    traffic_table_destructor_hook(obj);
}

static void traffic_table_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);

    lv_result_t res;

    res = lv_obj_event_base(&traffic_table_class, e);
    if(res != LV_RESULT_OK) return;

    traffic_table_event_hook(e);
}

