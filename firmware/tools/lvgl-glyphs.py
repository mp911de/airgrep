"""Prepare LVGL 9.6's direct static A8 software glyph path in the build directory."""

import argparse
import re
from pathlib import Path


def replace_once(source, before, after):
    if source.count(before) != 1:
        raise ValueError(f"LVGL glyph source changed near {before!r}")
    return source.replace(before, after)


def prepare(source, name):
    if name == "lv_draw_sw_letter.c":
        # A custom draw unit may need scratch even if the same glyph is static.
        direct = "if(lv_font_has_static_bitmap_internal(glyph_draw_dsc->g->resolved_font) &&"
        upright = "if(glyph_draw_dsc->rotation % 3600 == 0 && glyph_draw_dsc->format != LV_FONT_GLYPH_FORMAT_IMAGE)"
        if not all(source.count(s) == 1 for s in (direct, upright, "glyph_draw_dsc->g->format == LV_FONT_GLYPH_FORMAT_A8)")):
            raise ValueError("Check the direct glyph path in LVGL's software renderer")
        end = "#endif /*LV_USE_DRAW_SW*/"
        helper = """
bool airgrep_sw_draws_static_glyph(lv_draw_glyph_cb_t cb)
{
    return cb == draw_letter_cb;
}

"""
        return replace_once(source, end, helper + end)
    if name == "lv_draw_label.c":
        condition = "LV_FONT_GLYPH_FORMAT_NONE < g.format && g.format < LV_FONT_GLYPH_FORMAT_IMAGE"
        guard = f"""        bool bitmap_needs_scratch = {condition};
#if LV_USE_DRAW_SW
        extern bool airgrep_sw_draws_static_glyph(lv_draw_glyph_cb_t cb);
        if(g.format == LV_FONT_GLYPH_FORMAT_A8 && g.resolved_font->static_bitmap &&
           dsc->rotation % 3600 == 0 && airgrep_sw_draws_static_glyph(cb)) {{
            bitmap_needs_scratch = false;
        }}
#endif
        if(bitmap_needs_scratch) {{"""
        return replace_once(source, f"        if({condition}) {{", guard)

    # The XML exporter describes these bytes with an ordinary fmt_txt font.
    # Packed, compressed and file-backed fonts retain their exported descriptor.
    fields = (r"\bconst uint8_t glyph_bitmap\[\] =",
              r"\.glyph_bitmap\s*=\s*glyph_bitmap,", r"\.bpp\s*=\s*8,",
              r"\.bitmap_format\s*=\s*0,", r"\.get_glyph_bitmap\s*=\s*lv_font_get_bitmap_fmt_txt,")
    if (all(re.search(field, source) for field in fields) and
            ".are_glyphs_dynamic_loaded" not in source and ".static_bitmap" not in source):
        source = replace_once(source, "    .get_glyph_bitmap =", "    .static_bitmap = 1,\n    .get_glyph_bitmap =")
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    content = prepare(args.source.read_text(), args.source.name)
    if not args.output.exists() or args.output.read_text() != content:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(content)


if __name__ == "__main__":
    main()
