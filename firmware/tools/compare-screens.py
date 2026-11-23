#!/usr/bin/env python3
"""Compare a rendered screen with its design image and report how close they are.

Both images are blurred slightly, so anti-aliasing and a glyph that sits one pixel off do not count.
A pixel matches when no colour channel differs by more than the tolerance. The score is the share of
matching pixels. Needs Pillow.
"""

import argparse
from pathlib import Path
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter


def difference(design, render, blur):
    """Largest channel difference per pixel, as a greyscale image."""
    soften = ImageFilter.GaussianBlur(blur)
    channels = ImageChops.difference(design.filter(soften), render.filter(soften)).split()
    return ImageChops.lighter(ImageChops.lighter(channels[0], channels[1]), channels[2])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("design", type=Path)
    parser.add_argument("render", type=Path)
    parser.add_argument("--blur", type=float, default=1.5, help="Blur radius in pixels, default 1.5")
    parser.add_argument("--tolerance", type=int, default=40, help="Allowed channel difference of 255, default 40")
    parser.add_argument("--target", type=float, default=99.0, help="Share of matching pixels to pass, default 99")
    parser.add_argument("--ignore", metavar="X,Y,W,H", action="append", default=[],
                        help="Leave this rectangle out, for content that cannot match such as a real QR code")
    parser.add_argument("--diff", type=Path, help="Write the render with mismatching pixels in magenta")
    parser.add_argument("--delta", type=Path, help="Write design minus render per pixel, unblurred: black where they agree")
    args = parser.parse_args(argv)

    design, render = (Image.open(path).convert("RGB") for path in (args.design, args.render))
    if design.size != render.size:
        print(f"size differs: {design.size} against {render.size}")
        return 2
    for rectangle in args.ignore:
        x, y, width, height = (int(value) for value in rectangle.split(","))
        for image in (design, render):
            ImageDraw.Draw(image).rectangle((x, y, x + width - 1, y + height - 1), fill="black")
    delta = difference(design, render, args.blur)
    histogram = delta.histogram()
    score = 100.0 * sum(histogram[: args.tolerance + 1]) / sum(histogram)
    if args.delta:
        ImageChops.difference(design, render).save(args.delta)
    if args.diff:
        mismatch = delta.point(lambda value: 255 if value > args.tolerance else 0)
        faded = Image.blend(render, Image.new("RGB", render.size, "white"), 0.6)
        faded.paste(Image.new("RGB", render.size, "magenta"), mask=mismatch)
        faded.save(args.diff)
    print(f"{score:.2f} % match, target {args.target:g} %, {'pass' if score >= args.target else 'FAIL'}")
    return 0 if score >= args.target else 1


if __name__ == "__main__":
    sys.exit(main())
