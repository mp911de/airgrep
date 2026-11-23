#include "logo_private_gen.h"
#include "airgrep_ui.h"
#include <math.h>

/* Where the pieces of assets/logo.svg sit when the logo is 480 px wide. */
#define PILL_X 216
#define PILL_Y 3
#define PILL_WIDTH 261
#define PILL_HEIGHT 120
#define AIR_X 90
#define AIR_Y 15
#define GREP_X 247
#define GREP_Y 33
#define PLANE_X 36
#define PLANE_Y 28

/* Four by four dots, the plane in the place of one. Centres in tenths of a pixel. */
#define GRID 4
#define PLANE_COLUMN 2
#define PLANE_ROW 1
#define DOT_SIZE 9
#define DOT_FIRST_X 75
#define DOT_FIRST_Y 205
#define DOT_PITCH 211

#define RIPPLE_MS 2600
#define RIPPLE_STEP_MS 200
#define RIPPLE_JITTER_MS 90
#define PEAK_PHASE_1024 143
#define REST_PHASE_1024 369
#define PEAK_SCALE_256 358

#define COLOR_GREP lv_color_hex(0xffffff)



static void create_piece(lv_obj_t * logo, const void * image, int32_t x, int32_t y, lv_subject_t * color);
static void create_dot(lv_obj_t * logo, int32_t column, int32_t row);
static void ripple(void * logo, int32_t ms);
static int32_t smoothstep_1024(int32_t share);

void logo_constructor_hook(lv_obj_t * obj)
{
    lv_obj_t * pill = lv_obj_create(obj);
    lv_obj_remove_style_all(pill);
    lv_obj_set_pos(pill, PILL_X, PILL_Y);
    lv_obj_set_size(pill, PILL_WIDTH, PILL_HEIGHT);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_bind_style_prop(pill, LV_STYLE_BG_COLOR, 0, color_green);

    create_piece(obj, logo_air, AIR_X, AIR_Y, color_label);
    create_piece(obj, logo_grep, GREP_X, GREP_Y, NULL);
    create_piece(obj, logo_plane, PLANE_X, PLANE_Y, color_green);

    ((logo_t *)obj)->first_dot = lv_obj_get_child_count(obj);
    for(int32_t row = 0; row < GRID; row++) {
        for(int32_t column = 0; column < GRID; column++) {
            if(column != PLANE_COLUMN || row != PLANE_ROW) {
                create_dot(obj, column, row);
            }
        }
    }

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, obj);
    lv_anim_set_exec_cb(&anim, ripple);
    lv_anim_set_values(&anim, 0, RIPPLE_MS);
    lv_anim_set_duration(&anim, RIPPLE_MS);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);
}

void logo_destructor_hook(lv_obj_t * obj)
{
    lv_anim_delete(obj, ripple);
}

void logo_event_hook(lv_event_t * e)
{
    LV_UNUSED(e);
}

static void create_piece(lv_obj_t * logo, const void * image, int32_t x, int32_t y, lv_subject_t * color)
{
    lv_obj_t * piece = lv_image_create(logo);
    lv_image_set_src(piece, image);
    lv_obj_set_pos(piece, x, y);
    lv_obj_set_style_image_recolor_opa(piece, LV_OPA_COVER, 0);
    if(color) {
        lv_obj_bind_style_prop(piece, LV_STYLE_IMAGE_RECOLOR, 0, color);
    }
    else {
        lv_obj_set_style_image_recolor(piece, COLOR_GREP, 0);
    }
}

/* A dot carries the time the ring takes to reach it. The offset comes from its place, not from a
 * random number, so every start looks the same. */
static void create_dot(lv_obj_t * logo, int32_t column, int32_t row)
{
    lv_obj_t * dot = lv_obj_create(logo);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, DOT_SIZE, DOT_SIZE);
    lv_obj_set_pos(dot, (DOT_FIRST_X + column * DOT_PITCH - DOT_SIZE * 5 + 5) / 10,
                   (DOT_FIRST_Y + row * DOT_PITCH - DOT_SIZE * 5 + 5) / 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_transform_pivot_x(dot, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(dot, lv_pct(50), 0);

    float distance = hypotf((float)(column - PLANE_COLUMN), (float)(row - PLANE_ROW));
    int32_t loose = (column * 7 + row * 13) % 5 * RIPPLE_JITTER_MS / 2 - RIPPLE_JITTER_MS;
    lv_obj_set_user_data(dot, (void *)(intptr_t)(lroundf(distance * RIPPLE_STEP_MS) + loose));
}

static void ripple(void * logo, int32_t ms)
{
    lv_color_t rest = lv_subject_get_color(color_logo_dot);
    lv_color_t peak = lv_subject_get_color(color_green);
    for(uint32_t i = ((logo_t *)logo)->first_dot; i < lv_obj_get_child_count(logo); i++) {
        lv_obj_t * dot = lv_obj_get_child(logo, (int32_t)i);
        int32_t since = (ms - (int32_t)(intptr_t)lv_obj_get_user_data(dot) + RIPPLE_MS) % RIPPLE_MS;
        int32_t share = since * 1024 / RIPPLE_MS;
        int32_t swell = share < PEAK_PHASE_1024 ? smoothstep_1024(share * 1024 / PEAK_PHASE_1024)
                        : share < REST_PHASE_1024 ? 1024 - smoothstep_1024((share - PEAK_PHASE_1024) * 1024 / (REST_PHASE_1024 - PEAK_PHASE_1024)) : 0;
        lv_obj_set_style_transform_scale(dot, LV_SCALE_NONE + (PEAK_SCALE_256 - LV_SCALE_NONE) * swell / 1024, 0);
        lv_obj_set_style_bg_color(dot, lv_color_mix(peak, rest, (uint8_t)(swell * 255 / 1024)), 0);
    }
}

static int32_t smoothstep_1024(int32_t share)
{
    return share * share / 1024 * (3 * 1024 - 2 * share) / 1024;
}
