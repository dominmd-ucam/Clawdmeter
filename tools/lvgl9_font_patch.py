#!/usr/bin/env python3
"""Patch an lv_font_conv (LVGL 8 format) .c font in place so it builds on LVGL 9.

lv_font_conv emits LVGL 6/7/8 version guards, a `.cache` field and an old
lv_font_t initializer; on LVGL 9 that compiles but renders invisible glyphs
(see CLAUDE.md gotcha #4). This rewrites those parts to match the patched
fonts already in firmware/src (e.g. font_styrene_28.c).

Usage: python tools/lvgl9_font_patch.py firmware/src/font_latin_28.c [...]
"""
import re
import sys
from pathlib import Path


def patch(text: str) -> str:
    guard = re.search(r"^#ifndef (\w+)\n#define \1 1\n#endif\n", text, re.M).group(1)
    name = re.search(r"^const lv_font_t (\w+) = \{", text, re.M).group(1)
    metric = lambda key: re.search(rf"\.{key} = (-?\d+),", text).group(1)
    line_height, base_line = metric("line_height"), metric("base_line")
    ul_pos, ul_thick = metric("underline_position"), metric("underline_thickness")

    # Header: single include, no per-font enable guard.
    text = re.sub(r"#ifdef LV_LVGL_H_INCLUDE_SIMPLE\n.*?#endif\n", '#include "lvgl.h"\n', text,
                  count=1, flags=re.S)
    text = text.replace(f"#ifndef {guard}\n#define {guard} 1\n#endif\n\n#if {guard}\n", "")
    text = text.replace(f"\n#endif /*#if {guard}*/\n", "\n")

    # font_dsc: drop the glyph cache (removed in LVGL 9) and its version guards.
    text = re.sub(r"#if LV_VERSION_CHECK\(8, 0, 0\)\n/\*Store all the custom data of the font\*/\n"
                  r"static  lv_font_fmt_txt_glyph_cache_t cache;\n(static const lv_font_fmt_txt_dsc_t font_dsc = \{)\n"
                  r"#else\n.*?#endif\n", r"\1\n", text, count=1, flags=re.S)
    text = re.sub(r"\n#if LV_VERSION_CHECK\(8, 0, 0\)\n    \.cache = &cache\n#endif\n", "\n", text)

    # Public descriptor: rebuild with the LVGL 9 field set.
    font = (
        f"const lv_font_t {name} = {{\n"
        "    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/\n"
        "    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/\n"
        f"    .line_height = {line_height},          /*The maximum line height required by the font*/\n"
        f"    .base_line = {base_line},             /*Baseline measured from the bottom of the line*/\n"
        "    .subpx = LV_FONT_SUBPX_NONE,\n"
        "    .release_glyph = NULL,\n"
        "    .kerning = 0,\n"
        "    .static_bitmap = 0,\n"
        f"    .underline_position = {ul_pos},\n"
        f"    .underline_thickness = {ul_thick},\n"
        "    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */\n"
        "    .fallback = NULL,\n"
        "    .user_data = NULL,\n"
        "};\n"
    )
    text = re.sub(r"#if LV_VERSION_CHECK\(8, 0, 0\)\nconst lv_font_t \w+ = \{\n.*?\n\};\n", font, text,
                  count=1, flags=re.S)
    assert "LV_VERSION_CHECK" not in text and "LVGL_VERSION_MAJOR" not in text, "unpatched guard left"
    return text


if __name__ == "__main__":
    for path in sys.argv[1:]:
        p = Path(path)
        p.write_text(patch(p.read_text(encoding="utf-8")), encoding="utf-8", newline="\n")
        print("patched", p)
