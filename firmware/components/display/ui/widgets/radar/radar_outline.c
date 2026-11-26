#include "radar_outline.h"
#include "radar.h"
#include "radar_grow.h"

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl_private.h"
#else
    #include "lvgl_private/lvgl_private.h"
#endif

enum {
    RING_GLYPH_COUNT = sizeof("0123456789NM ") - 1,
    ALTITUDE_GLYPH_COUNT = sizeof(U"0123456789+−–") / sizeof(U"0"[0]) - 1,
    TARGET_SYMBOL_COUNT = 7,
    OWN_SYMBOL_COUNT = 2,
    GLYPH_MASK_CAPACITY = RING_GLYPH_COUNT + ALTITUDE_GLYPH_COUNT,
    SYMBOL_MASK_CAPACITY = TARGET_SYMBOL_COUNT + OWN_SYMBOL_COUNT,
};

static struct {
    const lv_font_t * font;
    uint32_t letter;
    lv_draw_buf_t * mask;
} glyph_cache[GLYPH_MASK_CAPACITY];

static struct {
    const lv_image_dsc_t * image;
    int32_t source_radius_px;
    lv_draw_buf_t * mask;
} symbol_cache[SYMBOL_MASK_CAPACITY];

void radar_draw_symbol_image(lv_layer_t * layer, lv_draw_image_dsc_t * image, const lv_area_t * area)
{
    if(!radar_symbol_atlas_draw(layer, image, area)) {
        lv_draw_image(layer, image, area);
    }
}

static lv_draw_buf_t * grow(const uint8_t * mask, uint32_t width, uint32_t height,
                            uint32_t stride, uint32_t radius)
{
    lv_draw_buf_t * grown =
        lv_draw_buf_create(width + 2 * radius, height + 2 * radius, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
    if(grown) {
        radar_grow(mask, width, height, stride, grown->data, grown->header.stride, radius);
    }
    return grown;
}

static lv_draw_buf_t * grown_glyph(const lv_font_t * font, uint32_t letter, lv_font_glyph_dsc_t * glyph)
{
    size_t slot = 0;
    while(slot < GLYPH_MASK_CAPACITY && glyph_cache[slot].mask) {
        if(glyph_cache[slot].font == font && glyph_cache[slot].letter == letter) {
            return glyph_cache[slot].mask;
        }
        slot++;
    }
    if(slot == GLYPH_MASK_CAPACITY) {
        return NULL;
    }
    lv_draw_buf_t * grown = NULL;

    lv_draw_buf_t * plain = lv_draw_buf_create(glyph->box_w, glyph->box_h, LV_COLOR_FORMAT_A8, LV_STRIDE_AUTO);
    if(plain == NULL) {
        return NULL;
    }
    glyph->req_raw_bitmap = 0;
    const lv_draw_buf_t * bitmap = lv_font_get_glyph_bitmap(glyph, plain);
    if(bitmap) {
        grown = grow(bitmap->data, glyph->box_w, glyph->box_h, bitmap->header.stride, RADAR_OUTLINE_WIDTH_PX);
    }
    if(grown) {
        glyph_cache[slot].font = font;
        glyph_cache[slot].letter = letter;
        glyph_cache[slot].mask = grown;
    }
    lv_font_glyph_release_draw_data(glyph);
    lv_draw_buf_destroy(plain);
    return grown;
}

void radar_outline_text(lv_layer_t * layer, const char * text, const lv_font_t * font, lv_color_t color,
                        const lv_area_t * area)
{
    lv_draw_image_dsc_t image;
    lv_draw_image_dsc_init(&image);
    image.recolor = color;
    image.recolor_opa = LV_OPA_COVER;

    int32_t x = area->x1;
    int32_t baseline = area->y1 + font->line_height - font->base_line;
    uint32_t at = 0;
    uint32_t letter = lv_text_encoded_next(text, &at);
    while(letter) {
        uint32_t next = lv_text_encoded_next(text, &at);
        lv_font_glyph_dsc_t glyph;
        if(lv_font_get_glyph_dsc(font, &glyph, letter, next)) {
            image.src = glyph.box_w && glyph.box_h ? grown_glyph(font, letter, &glyph) : NULL;
            if(image.src) {
                lv_area_t grown;
                grown.x1 = x + glyph.ofs_x - RADAR_OUTLINE_WIDTH_PX;
                grown.y1 = baseline - glyph.box_h - glyph.ofs_y - RADAR_OUTLINE_WIDTH_PX;
                grown.x2 = grown.x1 + glyph.box_w + 2 * RADAR_OUTLINE_WIDTH_PX - 1;
                grown.y2 = grown.y1 + glyph.box_h + 2 * RADAR_OUTLINE_WIDTH_PX - 1;
                lv_draw_image(layer, &image, &grown);
            }
            x += glyph.adv_w;
        }
        letter = next;
    }
}

const lv_draw_buf_t * radar_symbol_outline_mask(const lv_image_dsc_t * plain, int32_t scale)
{
    int32_t source_radius_px = (RADAR_OUTLINE_WIDTH_PX * LV_SCALE_NONE + scale / 2) / scale;
    size_t slot = 0;
    while(slot < SYMBOL_MASK_CAPACITY && symbol_cache[slot].mask) {
        if(symbol_cache[slot].image == plain && symbol_cache[slot].source_radius_px == source_radius_px) {
            break;
        }
        slot++;
    }
    if(slot == SYMBOL_MASK_CAPACITY) {
        return NULL;
    }
    lv_draw_buf_t * grown = symbol_cache[slot].mask;
    if(!grown) {
        grown = grow(plain->data, plain->header.w, plain->header.h, plain->header.stride, source_radius_px);
        if(!grown) {
            return NULL;
        }
        symbol_cache[slot].image = plain;
        symbol_cache[slot].source_radius_px = source_radius_px;
        symbol_cache[slot].mask = grown;
    }

    return grown;
}

void radar_outline_symbol(lv_layer_t * layer, const void * symbol, lv_point_t at, lv_color_t color, int32_t scale,
                          int32_t rotation_deg)
{
    const lv_draw_buf_t * grown = radar_symbol_outline_mask(symbol, scale);
    if(!grown) {
        return;
    }
    int32_t size = (int32_t)grown->header.w;
    lv_draw_image_dsc_t image;
    lv_draw_image_dsc_init(&image);
    image.src = grown;
    image.recolor = color;
    image.recolor_opa = LV_OPA_COVER;
    image.scale_x = scale;
    image.scale_y = scale;
    image.rotation = rotation_deg * 10;
    image.pivot.x = size / 2;
    image.pivot.y = size / 2;
    lv_area_t area = {at.x - size / 2, at.y - size / 2, at.x + size / 2 - 1, at.y + size / 2 - 1};
    radar_draw_symbol_image(layer, &image, &area);
}
