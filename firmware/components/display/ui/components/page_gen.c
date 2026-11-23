#include "page_gen.h"
#include "../airgrep_ui.h"

typedef struct {
    /* A plain <prop> a binding froze. The runtime inlines it, because it
     * re-parses the view per instance; the generator compiles once, so the value
     * is kept here and read back in the callback. A plain field, not a subject:
     * nothing ever notifies on it. */
    int32_t page_snapshot;

    /* Index is the <COMP>_TIMELINE_* enum. Kept here rather than the root's
     * user_data, which stays free for the user. */
    lv_anim_timeline_t ** timelines;

    /* One bit per `{…}` trigger, so only a false-to-true edge fires */
    uint8_t play_timeline_event_lv_obj_0_prev : 1;
    uint8_t play_timeline_event_lv_obj_0_2_prev : 1;
} page_t;

static lv_anim_timeline_t * timeline_timeline_push_create(lv_obj_t * obj);

static uint32_t page_get_instance_event;
static void page_get_instance_cb(lv_event_t * e);
static page_t * page_get_instance(lv_obj_t * obj);
static void page_destroy_instance(page_t * inst);
static void page_delete_cb(lv_event_t * e);
/* Private event code for a condition trigger. One per XML element, never per
 * instance, so it cannot run away. */
static lv_event_code_t page_play_timeline_event_trigger_code;
/* Private event code for a condition trigger. One per XML element, never per
 * instance, so it cannot run away. */
static lv_event_code_t page_play_timeline_event_trigger_code_2;
static void page_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_lv_obj_0_play_timeline_event_cond_cb(lv_observer_t * observer, lv_subject_t * subject);
static void page_lv_obj_0_play_timeline_event_cond_cb_2(lv_observer_t * observer, lv_subject_t * subject);

lv_obj_t * page_create(lv_obj_t * parent, int32_t page)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    if(page_get_instance_event == 0) page_get_instance_event = lv_event_register_id();
    page_t * inst = lv_malloc(sizeof(page_t));
    lv_memzero(inst, sizeof(page_t));
    inst->page_snapshot = page;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "page_#");
        lv_obj_add_event_cb(lv_obj_0, page_get_instance_cb, page_get_instance_event, inst);
        lv_obj_set_x(lv_obj_0, 0);
        lv_obj_set_y(lv_obj_0, TOUCH);
        lv_obj_set_width(lv_obj_0, 800);
        lv_obj_set_height(lv_obj_0, 416);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_obj_set_style_translate_x(lv_obj_0, 800, 0);
        lv_subject_add_observer_obj(color_bg_grouped, page_lv_obj_0_style_bg_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);

        lv_obj_remove_theme(lv_obj_0, LV_PART_MAIN);
        if(page_play_timeline_event_trigger_code == 0) page_play_timeline_event_trigger_code = lv_event_register_id();
        if(page_play_timeline_event_trigger_code_2 == 0) page_play_timeline_event_trigger_code_2 = lv_event_register_id();

        lv_anim_timeline_t ** at_array = lv_malloc(sizeof(lv_anim_timeline_t *) * _PAGE_TIMELINE_CNT);
        at_array[PAGE_TIMELINE_TIMELINE_PUSH] = timeline_timeline_push_create(lv_obj_0);
        inst->timelines = at_array;

        lv_obj_add_play_timeline_event(lv_obj_0, page_play_timeline_event_trigger_code, page_get_timeline(lv_obj_0, PAGE_TIMELINE_TIMELINE_PUSH), 0, false);
        lv_subject_add_observer_obj(subject_page_opening, page_lv_obj_0_play_timeline_event_cond_cb, lv_obj_0, inst);
        lv_obj_add_play_timeline_event(lv_obj_0, page_play_timeline_event_trigger_code_2, page_get_timeline(lv_obj_0, PAGE_TIMELINE_TIMELINE_PUSH), 0, true);
        lv_subject_add_observer_obj(subject_page_closing, page_lv_obj_0_play_timeline_event_cond_cb_2, lv_obj_0, inst);

        the_root = lv_obj_0;
    }
    #endif

    if(the_root) lv_obj_add_event_cb(the_root, page_delete_cb, LV_EVENT_DELETE, inst);
    else page_destroy_instance(inst); /* no target matched, so nothing owns it */

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

lv_anim_timeline_t * page_get_timeline(lv_obj_t * obj, page_timeline_t timeline_id)
{
    if ((uint32_t)timeline_id >= _PAGE_TIMELINE_CNT) {
        LV_LOG_WARN("page has no timeline with %d ID", timeline_id);
        return NULL;
    }

    page_t * inst = page_get_instance(obj);
    return inst ? inst->timelines[timeline_id] : NULL;
}

static void int_anim_exec_cb(lv_anim_t * a, int32_t v)
{
    uint32_t data = (lv_uintptr_t)lv_anim_get_user_data(a);
    lv_style_prop_t prop = data >> 24;
    lv_style_selector_t selector = data & 0x00ffffff;

    lv_style_value_t style_value;
    style_value.num = v;
    lv_obj_set_local_style_prop(a->var, prop, style_value, selector);
}

static lv_anim_timeline_t * timeline_timeline_push_create(lv_obj_t * obj)
{
    lv_anim_timeline_t * at = lv_anim_timeline_create();
    lv_anim_timeline_t * at_to_merge = NULL;

    lv_anim_t a;
    uint32_t selector_and_prop;

    selector_and_prop = ((LV_STYLE_TRANSLATE_X & 0xff) << 24) | 0;
    lv_anim_init(&a);
    lv_anim_set_custom_exec_cb(&a, int_anim_exec_cb);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, 800, 0);
    lv_anim_set_duration(&a, 300);
    lv_anim_set_user_data(&a, (void *)((uintptr_t)selector_and_prop));
    lv_anim_set_path_cb(&a, lv_anim_path_custom_bezier3);
    lv_anim_set_bezier3_param(&a, 205, 819, 205, 1024);
    lv_anim_timeline_add(at, 0, &a);

    return at;
}

static void page_get_instance_cb(lv_event_t * e)
{
    void ** out = lv_event_get_param(e);
    *out = lv_event_get_user_data(e);
}

static page_t * page_get_instance(lv_obj_t * obj)
{
    page_t * inst = NULL;
    for(lv_obj_t * o = obj; o && !inst; o = lv_obj_get_parent(o)) {
        lv_obj_send_event(o, page_get_instance_event, &inst);
    }
    return inst;
}

static void page_destroy_instance(page_t * inst)
{
    if(inst->timelines) {
        for(uint32_t i = 0; i < 1; i++) lv_anim_timeline_delete(inst->timelines[i]);
        lv_free(inst->timelines);
    }
    lv_free(inst);
}

static void page_delete_cb(lv_event_t * e)
{
    page_destroy_instance(lv_event_get_user_data(e));
}

static void page_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_bg_grouped), 0);
}

static void page_lv_obj_0_play_timeline_event_cond_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    page_t * inst = lv_observer_get_user_data(observer);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    bool now = (lv_subject_get_int(subject_page_opening) == inst->page_snapshot);
    if(now && !inst->play_timeline_event_lv_obj_0_prev) lv_obj_send_event(obj, page_play_timeline_event_trigger_code, NULL);
    inst->play_timeline_event_lv_obj_0_prev = now;
}

static void page_lv_obj_0_play_timeline_event_cond_cb_2(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    page_t * inst = lv_observer_get_user_data(observer);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    bool now = (lv_subject_get_int(subject_page_closing) == inst->page_snapshot);
    if(now && !inst->play_timeline_event_lv_obj_0_2_prev) lv_obj_send_event(obj, page_play_timeline_event_trigger_code_2, NULL);
    inst->play_timeline_event_lv_obj_0_2_prev = now;
}

