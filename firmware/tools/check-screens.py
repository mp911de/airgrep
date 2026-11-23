#!/usr/bin/env python3
"""Render every designed screen with test_render, through a fixture device, and compare it with its design image.

A screen is here because it shows a layout no other screen shows. A state that changes a value, a symbol,
a text or the language has no picture: check-behaviour.py holds those cases and is not ported to test_render yet.

The renders go to utility/build/screens/, each with a -diff image that marks what misses the tolerance
and a -delta image, design minus render, that is black where the two agree. A screen that misses its target
gets one -compared image to look at: design, display and difference side by side, each named.
Exit code 1 when a screen misses the target. Needs Pillow.
"""

from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import sys

from PIL import Image, ImageDraw

FIRMWARE = Path(__file__).resolve().parents[1]
DESIGN = FIRMWARE.parent / "docs/ui-design/screens"
OUTPUT = FIRMWARE / "utility/build/screens"
DISPLAY = "800x480-touch"
DISPLAY_LIBRARY = FIRMWARE / "utility/build/airgrep Utility.app/Contents/Frameworks" / f"display-{DISPLAY}.dylib"
RENDER = FIRMWARE / "utility/build/test_render"
# The System page scrolls 360 px: this drag passes its end, so every run rests on the end.
TO_END = "drag:200,460,200,70"
# A tap on the left half of the header opens System, on the right half Wi-Fi and Battery.
SYSTEM, WIFI = ["--do", "tap:100,32"], ["--do", "tap:700,32"]
TO_TIME_ZONE = [*SYSTEM, "--do", "tap:200,256"]
# The Time zone row opens its page only for a zone chosen by hand.
BERLIN_BY_HAND = "normal zone=Europe/Berlin"
# The dots of the logo move, so their part of the startup screen is left out of the comparison.
LOGO_DOTS = ["--ignore", "158,124,84,82"]
# The greeting is one of many, picked anew at every start.
GREETING = ["--ignore", "0,336,800,64"]
CONFLICT = "conflict tracks range=2"
# The design image, the fixture with its display settings, and what the display is asked to do. A fourth entry holds
# options for the comparison.
SCREENS = {
    "1-home": ("normal", []),
    "2-home-alert": ("alert", []),
    "3-home-no-gps-fix": ("no-fix", []),
    "4-system": ("normal", SYSTEM),
    "5-wifi-battery": ("normal", WIFI),
    # Past the end of the table. The finger stays down: the indicator shows only then.
    "6-home-scrolled": ("normal", ["--do", "hold:600,440,600,100"]),
    "7-home-swipe-mute": ("advisory", ["--do", "drag:700,128,540,128"]),
    "8-home-swipe-ownship": ("own-ship", ["--do", "drag:500,128,660,128"]),
    # Past the end of the sheet, so every run rests on the end.
    "9-system-info": ("normal", [*SYSTEM, "--do", "hold:200,460,200,70"]),
    "10-licenses": ("normal", [*SYSTEM, "--do", TO_END, "--do", "tap:200,448"]),
    "11-license-text": ("normal", [*SYSTEM, "--do", TO_END, "--do", "tap:200,448", "--do", "tap:200,192"]),
    "12-icao-address": ("normal", [*SYSTEM, "--do", "tap:600,354"]),
    "13-callsign": ("normal", [*SYSTEM, "--do", "tap:600,291"]),
    "14-startup": ("normal", ["--startup"], LOGO_DOTS, GREETING),
    "15-charger": ("charging", []),
    "16-time-zone": (BERLIN_BY_HAND, TO_TIME_ZONE),
    # America, whose zones have the longest names, scrolled into them.
    "17-time-zone-america": (BERLIN_BY_HAND, [*TO_TIME_ZONE, "--do", "tap:200,416", "--do", "hold:200,440,200,248"]),
    "18-home-tracks": ("normal tracks", []),
    "19-home-range-16-busy": ("busy tracks range=16", []),
    # A device that had a fix and lost it: the zone it detected holds. Screen 3 never had one and shows UTC.
    "20-system-causes": ("causes", ["--do", "gnss:normal", "--do", "gnss:causes", *SYSTEM]),
    "21-altitude": ("normal", [*SYSTEM, "--do", "tap:600,192"]),
    "22-home-beyond": ("beyond tracks range=2", []),
    "23-home-conflict": (CONFLICT, []),
    "24-home-simple": ("normal tracks simple-symbols", []),
    # The Bands of the Conflict Indicator, docs/ui-design/guidance-bands.md. The design shows the Bands the sketch
    # states for each picture, the display those of the conflict engine. With the switch off Home shows none of them.
    "bands-0-off": (CONFLICT, []),
    "bands-1-preventive": ("preventive tracks conflict-indicator", []),
    "bands-2-corrective": ("corrective tracks conflict-indicator", []),
    "bands-3-warning": (f"{CONFLICT} conflict-indicator", []),
    "bands-4-recovery": ("recovery tracks conflict-indicator", []),
}
# The themes differ in the values of their tokens and in nothing else. These two show every token in dark.
# The fill of a wedge is stronger in dark, so the Bands are compared in both themes.
DARK = ("2-home-alert", "4-system", "bands-1-preventive", "bands-2-corrective", "bands-3-warning", "bands-4-recovery")
# Charger mode is dark whatever the theme setting says, so it has no light design.
DARK_ONLY = ("15-charger",)
# Accepted by Mark below the usual 99 %: the text of the display drifts against WebKit's. One that falls below its
# share here has a failure to fix, and Mark sees the affected screens afterwards.
ACCEPTED = {"light-11-license-text": 94, "light-5-wifi-battery": 98, "dark-4-system": 98, "dark-bands-4-recovery": 98,
            "dark-bands-1-preventive": 98, "dark-bands-2-corrective": 98}
ACCEPTED |= {"light-7-home-swipe-mute": 98.5, "light-9-system-info": 98.5, "light-20-system-causes": 98.5,
             "dark-bands-3-warning": 98.5, "dark-2-home-alert": 98.5}


def compared(theme, name):
    """One picture of a failed screen that says which part is which."""
    parts = (("1 DESIGN, from the sketch", DESIGN / f"{theme}-{name}.png"),
             ("2 DISPLAY", OUTPUT / f"{theme}-{name}.png"),
             ("3 DIFFERENCE, in magenta", OUTPUT / f"{theme}-{name}-diff.png"))
    images = [Image.open(path).convert("RGB") for _, path in parts]
    width, bar, gap = images[0].width, 28, 16
    sheet = Image.new("RGB", (len(images) * (width + gap) - gap, images[0].height + bar), "#202020")
    for place, ((label, _), image) in enumerate(zip(parts, images)):
        ImageDraw.Draw(sheet).text((place * (width + gap) + 8, 8), label, fill="white")
        sheet.paste(image, (place * (width + gap), bar))
    target = OUTPUT / f"{theme}-{name}-compared.png"
    sheet.save(target)
    return target


def check(theme, name):
    fixture, asked, *comparison = SCREENS[name]
    render = OUTPUT / f"{theme}-{name}.png"
    subprocess.run([RENDER, DISPLAY_LIBRARY, fixture + (" dark" if theme == "dark" else ""), render, *asked],
                   check=True, capture_output=True, text=True)
    compare = [FIRMWARE / "tools/compare-screens.py", DESIGN / f"{theme}-{name}.png", render,
               "--diff", OUTPUT / f"{theme}-{name}-diff.png", "--delta", OUTPUT / f"{theme}-{name}-delta.png",
               *(option for options in comparison for option in options)]
    if f"{theme}-{name}" in ACCEPTED:
        compare += ["--target", str(ACCEPTED[f"{theme}-{name}"])]
    result = subprocess.run(compare, capture_output=True, text=True)
    look = f"\n  look at {compared(theme, name)}" if result.returncode else ""
    print(f"{theme}-{name}: {result.stdout.strip()}{look}")
    return result.returncode == 0


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    checks = [("light", name) for name in SCREENS if name not in DARK_ONLY]
    checks += [("dark", name) for name in DARK + DARK_ONLY]
    with ThreadPoolExecutor(8) as pool:
        passed = list(pool.map(lambda entry: check(*entry), checks))
    return 0 if all(passed) else 1


if __name__ == "__main__":
    sys.exit(main())
