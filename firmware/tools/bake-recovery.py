"""Generate the fixed-radius recovery atlas using the configured Utility renderer."""

import argparse
import collections
import csv
import io
import json
import math
import shlex
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "utility/build"
RADAR = ROOT / "components/display/ui/widgets/radar"


def analytic_stamps(build):
    rows = json.loads((build / "compile_commands.json").read_text())
    compile_row = next(
        row
        for row in rows
        if row["file"].endswith("/radar/radar.c")
        and "800x480-touch-objects" in row["command"]
    )
    args = shlex.split(compile_row["command"])
    with tempfile.TemporaryDirectory() as work:
        exporter = Path(work) / "bake"
        subprocess.run(
            [
                *args[: args.index("-o")],
                "-Wno-unused-function",
                "-I" + str(RADAR),
                Path(__file__).with_suffix(".c"),
                RADAR / "radar_track.c",
                build / "_deps/lvgl-build/liblvgl.a",
                "-o",
                exporter,
            ],
            cwd=build,
            check=True,
        )
        data = subprocess.check_output([exporter], text=True)
    stamps = collections.defaultdict(dict)
    for row in csv.reader(io.StringIO(data)):
        stamp, x, y, alpha, degree = map(int, row)
        if stamp < 8:
            ring = 360 if x * x + y * y > 100 * 100 else 0
            index = ring + stamp * 45 + degree
        else:
            index = 720 + stamp - 8
        stamps[index][x, y] = bytes([alpha])
    return stamps


def encode(stamps, count):
    headers = bytearray()
    runs = bytearray()
    for index in range(count):
        stamp = stamps[index]
        left, top = (
            min((x for x, _ in stamp), default=0),
            min((y for _, y in stamp), default=0),
        )
        headers.extend(struct.pack("<hhI", left, top, len(runs)))
        previous = None
        for x, y in sorted(stamp, key=lambda p: (p[1], p[0])):
            if previous != (x - 1, y):
                runs.extend([x - left, y - top, 0])
                length_byte = len(runs) - 1
            runs[length_byte] += 1
            runs.extend(stamp[x, y])
            previous = x, y
    headers.extend(struct.pack("<hhI", 0, 0, len(runs)))
    return headers + runs


def moving_caps(stamps):
    for angle in range(46):
        sine = math.sin(math.radians(angle))
        cosine = math.cos(math.radians(angle))
        pixels = stamps[766 + angle]
        for y in range(-178, -18):
            x_mid = -(y + 0.5) * sine / cosine - 0.5
            for x in range(math.floor(x_mid) - 3, math.ceil(x_mid) + 4):
                along = (x + 0.5) * sine - (y + 0.5) * cosine - 32
                across = abs((x + 0.5) * cosine + (y + 0.5) * sine)
                if 0 <= along <= 144 and across < 2:
                    pixels[x, y] = bytes([round(along * 16) % 224, round(across * 16)])
    return bytes(
        round(
            255
            * min(
                1,
                max(
                    0,
                    2
                    - math.hypot(
                        perp / 16,
                        max(0, 1.5 - u / 16, min(u / 16 - 6.5, 15.5 - u / 16)),
                    ),
                ),
            )
        )
        for u in range(224)
        for perp in range(33)
    )


if __name__ == "__main__":
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument(
        "--build-dir",
        type=Path,
        default=BUILD,
        help="configured Utility build with compile_commands.json",
    )
    args.add_argument("--output", type=Path, default=RADAR / "radar_recovery_atlas.bin")
    options = args.parse_args()
    stamps = analytic_stamps(options.build_dir.resolve())
    lut = moving_caps(stamps)
    for bank in range(1, 7):
        for angle in range(46):
            fixed = stamps[812 + (bank - 1) * 46 + angle]
            for xy, (along, across) in stamps[766 + angle].items():
                alpha = lut[((along - 32 * bank) % 224) * 33 + across]
                if alpha:
                    fixed[xy] = bytes([alpha])
    options.output.write_bytes(encode(stamps, 1088) + lut)
