#!/usr/bin/env python3
"""Turn a PNG into the C source of an LVGL image in the A8 format: the alpha of each pixel, one byte.

Writes what the LVGL Editor exports for <data color_format="a8"> in globals.xml. Reads the one kind of PNG
the Editor makes, 8 bit RGBA without interlacing.
"""

import argparse
from pathlib import Path
import struct
import sys
import zlib

SIGNATURE = b"\x89PNG\r\n\x1a\n"
RGBA_8_BIT = (8, 6, 0, 0, 0)
BYTES_PER_PIXEL = 4

SOURCE = """
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#elif defined(LV_LVGL_H_INCLUDE_SYSTEM)
#include <lvgl.h>
#elif defined(LV_BUILD_TEST)
#include "../lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_{upper}
#define LV_ATTRIBUTE_{upper}
#endif

static const
LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_{upper}
uint8_t {name}_map[] = {{

{rows}

}};

const lv_image_dsc_t {name} = {{
  .header = {{
    .magic = LV_IMAGE_HEADER_MAGIC,
    .cf = LV_COLOR_FORMAT_A8,
    .flags = 0,
    .w = {width},
    .h = {height},
    .stride = {width},
    .reserved_2 = 0,
  }},
  .data_size = sizeof({name}_map),
  .data = {name}_map,
  .reserved = NULL,
}};

"""


def chunks(data):
    at = len(SIGNATURE)
    while at < len(data):
        length, kind = struct.unpack(">I4s", data[at:at + 8])
        yield kind, data[at + 8:at + 8 + length]
        at += length + 12


def paeth(left, up, up_left):
    estimate = left + up - up_left
    nearest = min((abs(estimate - left), 0, left), (abs(estimate - up), 1, up), (abs(estimate - up_left), 2, up_left))
    return nearest[2]


def unfiltered(packed, width, height):
    """The scanlines of a PNG as rows of bytes, each filter undone."""
    stride = width * BYTES_PER_PIXEL
    before, rows = bytes(stride), []
    for line in range(height):
        start = line * (stride + 1)
        kind, row = packed[start], bytearray(packed[start + 1:start + 1 + stride])
        for at in range(stride):
            left = row[at - BYTES_PER_PIXEL] if at >= BYTES_PER_PIXEL else 0
            up_left = before[at - BYTES_PER_PIXEL] if at >= BYTES_PER_PIXEL else 0
            row[at] = (row[at] + (0, left, before[at], (left + before[at]) // 2, paeth(left, before[at], up_left))[kind]) & 0xFF
        rows.append(row)
        before = row
    return rows


def alpha_rows(path):
    data = path.read_bytes()
    if not data.startswith(SIGNATURE):
        raise ValueError(f"{path} is no PNG")
    header, packed = None, b""
    for kind, body in chunks(data):
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            packed += body
    if header is None or header[2:] != RGBA_8_BIT:
        raise ValueError(f"{path} is not 8 bit RGBA without interlacing")
    width, height = header[:2]
    return [row[3::BYTES_PER_PIXEL] for row in unfiltered(zlib.decompress(packed), width, height)]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("png", type=Path)
    parser.add_argument("source", type=Path, help="The C file to write. Its name without .c names the image.")
    args = parser.parse_args(argv)
    try:
        rows = alpha_rows(args.png)
    except ValueError as problem:
        print(problem, file=sys.stderr)
        return 1
    args.source.parent.mkdir(parents=True, exist_ok=True)
    args.source.write_text(SOURCE.format(
        name=args.source.stem, upper=args.source.stem.upper(), width=len(rows[0]), height=len(rows),
        rows="\n".join("    " + ",".join(f"0x{value:02x}" for value in row) + "," for row in rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
