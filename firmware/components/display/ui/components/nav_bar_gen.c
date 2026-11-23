#include "nav_bar_gen.h"
#include "../airgrep_ui.h"

typedef struct {
    /* A plain <prop> a binding froze. The runtime inlines it, because it
     * re-parses the view per instance; the generator compiles once, so the value
     * is kept here and read back in the callback. A plain field, not a subject:
     * nothing ever notifies on it. */
    int32_t page_snapshot;
    int32_t back_snapshot;
} nav_bar_t;

static void nav_bar_destroy_instance(nav_bar_t * inst);
static void nav_bar_delete_cb(lv_event_t * e);
static void nav_bar_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void nav_bar_lv_obj_0_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void nav_bar_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject);
static void nav_bar_set_subject_cb(lv_event_t * e);
static void nav_bar_set_subject_cb_2(lv_event_t * e);

lv_obj_t * nav_bar_create(lv_obj_t * parent, const char * title, int32_t page, int32_t back)
{
    LV_TRACE_OBJ_CREATE("begin");


    lv_obj_t * the_root = NULL;

    nav_bar_t * inst = lv_malloc(sizeof(nav_bar_t));
    lv_memzero(inst, sizeof(nav_bar_t));
    inst->page_snapshot = page;
    inst->back_snapshot = back;

    #if AIRGREP_UI_CHECK_COMPILE_TARGET(AIRGREP_UI_TARGET_ALL)
    if (airgrep_ui_check_target(AIRGREP_UI_TARGET_ALL)) {
        lv_obj_t * lv_obj_0 = lv_obj_create(parent);
        lv_obj_set_name_static(lv_obj_0, "nav_bar_#");
        lv_obj_set_width(lv_obj_0, lv_pct(100));
        lv_obj_set_height(lv_obj_0, TOUCH);
        lv_obj_set_flex_flow(lv_obj_0, LV_FLEX_FLOW_ROW);
        lv_obj_set_scrollable(lv_obj_0, false);
        lv_obj_set_style_flex_cross_place(lv_obj_0, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_flex_track_place(lv_obj_0, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_bottom(lv_obj_0, 2, 0);
        lv_subject_add_observer_obj(color_surface, nav_bar_lv_obj_0_style_bg_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_bg_opa(lv_obj_0, (255 * 100 / 100), 0);
        lv_subject_add_observer_obj(color_separator, nav_bar_lv_obj_0_style_border_color_binding_cb, lv_obj_0, NULL);
        lv_obj_set_style_border_width(lv_obj_0, HAIRLINE, 0);
        lv_obj_set_style_border_side(lv_obj_0, LV_BORDER_SIDE_BOTTOM, 0);

        lv_obj_remove_theme(lv_obj_0, LV_PART_MAIN);
        lv_obj_t * back_button = lv_obj_create(lv_obj_0);
        lv_obj_set_name(back_button, "back_button");
        lv_obj_set_width(back_button, TOUCH);
        lv_obj_set_height(back_button, TOUCH);
        lv_obj_set_clickable(back_button, true);
        lv_obj_set_scrollable(back_button, false);
        lv_obj_remove_theme(back_button, LV_PART_MAIN);
        lv_obj_add_style(back_button, &style_pressed_clear, LV_STATE_PRESSED);
        lv_obj_t * lv_image_0 = lv_image_create(back_button);
        lv_image_set_src(lv_image_0, icon_chevron_left);
        lv_obj_set_align(lv_image_0, LV_ALIGN_CENTER);
        lv_subject_add_observer_obj(color_tint, nav_bar_lv_image_0_style_image_recolor_binding_cb, lv_image_0, NULL);
        lv_obj_set_style_image_recolor_opa(lv_image_0, (255 * 100 / 100), 0);

        lv_obj_add_event_cb(back_button, nav_bar_set_subject_cb, LV_EVENT_CLICKED, inst);
        lv_obj_add_event_cb(back_button, nav_bar_set_subject_cb_2, LV_EVENT_CLICKED, inst);

        lv_obj_t * bar_title = lv_label_create(lv_obj_0);
        lv_obj_set_name(bar_title, "bar_title");
        lv_label_set_translation_tag(bar_title, title);
        lv_obj_set_flex_grow(bar_title, 1);
        lv_obj_set_style_text_font(bar_title, font_title, 0);

        lv_obj_t * trailing = lv_obj_create(lv_obj_0);
        lv_obj_set_name(trailing, "trailing");
        lv_obj_set_width(trailing, LV_SIZE_CONTENT);
        lv_obj_set_height(trailing, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(trailing, LV_FLEX_FLOW_ROW);
        lv_obj_set_clickable(trailing, false);
        lv_obj_set_scrollable(trailing, false);
        lv_obj_set_style_flex_cross_place(trailing, LV_FLEX_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_column(trailing, 8, 0);
        lv_obj_set_style_pad_right(trailing, 14, 0);
        lv_obj_remove_theme(trailing, LV_PART_MAIN);

        the_root = lv_obj_0;
    }
    #endif

    if(the_root) lv_obj_add_event_cb(the_root, nav_bar_delete_cb, LV_EVENT_DELETE, inst);
    else nav_bar_destroy_instance(inst); /* no target matched, so nothing owns it */

    LV_TRACE_OBJ_CREATE("finished");

    return the_root;
}

lv_obj_t * nav_bar_get_trailing(lv_obj_t *parent)
{
    return lv_obj_find_by_name(parent, "trailing");
}

static void nav_bar_destroy_instance(nav_bar_t * inst)
{
    lv_free(inst);
}

static void nav_bar_delete_cb(lv_event_t * e)
{
    nav_bar_destroy_instance(lv_event_get_user_data(e));
}

static void nav_bar_lv_obj_0_style_bg_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_bg_color(obj, lv_subject_get_color(color_surface), 0);
}

static void nav_bar_lv_obj_0_style_border_color_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_border_color(obj, lv_subject_get_color(color_separator), 0);
}

static void nav_bar_lv_image_0_style_image_recolor_binding_cb(lv_observer_t * observer, lv_subject_t * subject)
{
    LV_UNUSED(subject);
    lv_obj_t * obj = lv_observer_get_target_obj(observer);
    lv_obj_set_style_image_recolor(obj, lv_subject_get_color(color_tint), 0);
}

static void nav_bar_set_subject_cb(lv_event_t * e)
{
    nav_bar_t * inst = lv_event_get_user_data(e);
    lv_subject_set_int(subject_page_closing, inst->page_snapshot);
}

static void nav_bar_set_subject_cb_2(lv_event_t * e)
{
    nav_bar_t * inst = lv_event_get_user_data(e);
    lv_subject_set_int(subject_page, inst->back_snapshot);
}

