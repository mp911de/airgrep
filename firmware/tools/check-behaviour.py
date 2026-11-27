#!/usr/bin/env python3
"""Use the display of a fixture device and check what it does, not how it looks.

Two parts. The walk taps every sequence of header left, header right and back up to a length and checks
which page ends up in front, against a model of the navigation in docs/ui-design/screens.md. The cases
are single behaviours: each names its gestures, the page in front at the end and the actions the display
must have asked for, exactly.

The page in front is read from a snapshot. check-screens.py compares the same pages with the design.
Needs Pillow.
"""

import argparse
from concurrent.futures import ThreadPoolExecutor
import itertools
from pathlib import Path
import subprocess
import sys

from PIL import Image, ImageChops

FIRMWARE = Path(__file__).resolve().parents[1]
DISPLAY = "800x480-touch"
RENDER = FIRMWARE / "utility/build/test_render"
DISPLAY_LIBRARY = FIRMWARE / f"utility/build/airgrep Utility.app/Contents/Frameworks/display-{DISPLAY}.dylib"
OUTPUT = FIRMWARE / "utility/build/behaviour"

LEFT, RIGHT, BACK = "tap:100,32", "tap:700,32", "tap:30,96"
# On System: scroll to the end, where Licenses is the last row on the left and DISPLAY the last section on the right.
# On Licenses: the first component.
TO_INFO, LICENSES, FIRST_LICENSE = "drag:200,460,200,70", "tap:200,448", "tap:200,192"
# Theme is on the page as it opens, Track history, the Simple half of Symbols and Conflict Indicator at its end.
DARK, TRACK_HISTORY, SIMPLE_SYMBOLS, CONFLICT_INDICATOR = "tap:735,453", "tap:745,157", "tap:745,221", "tap:745,285"
PLANE_SYMBOLS = "tap:640,221"
TRACK, ZOOM_OUT, ZOOM_IN = "tap:344,104", "tap:56,440", "tap:344,440"
# On System and on the pages of the time zone choice. A fixture starts with Automatic on and detects Berlin, no-fix
# detects no zone.
AUTOMATIC, TIME_ZONE, UTC_ROW, DETECTED_ROW, AFRICA, FIRST_ZONE = ("tap:340,192", "tap:200,256", "tap:200,192", "tap:200,256",
                                                                  "tap:200,352", "tap:200,192")
# On Home, the first row of the table and the action a swipe uncovers.
SWIPE_LEFT, TRAILING_ACTION = "drag:700,128,540,128", "tap:728,128"
SWIPE_RIGHT, LEADING_ACTION = "drag:500,128,660,128", "tap:470,128"
BELL, RADAR = "tap:500,144", "tap:200,300"
# Longer than the 10 s after which the display returns to Home and an open row closes, and the slide after it.
# Pages that are read or typed on return after 30 s.
IDLE, IDLE_READING = "wait:12000", "wait:32000"
# On System, the right column: FILTER, then OWNSHIP.
ALTITUDE_ROW, CALLSIGN_ROW, ICAO_ROW = "tap:600,192", "tap:600,291", "tap:600,354"
# The hex keypad: three columns of digits, three of letters, four rows. A number page has its digits, delete and Save.
HEX = {key: f"tap:{x},{y}" for row, y in zip(("123ABC", "456DEF", "789", " 0"), (188, 268, 348, 428))
       for key, x in zip(row, (76, 204, 332, 468, 596, 724))}
HEX_DELETE, HEX_SAVE = "tap:332,428", "tap:596,428"
# On Home at 2 NM one target lies inside the ring: the band follows its row, then the first row from beyond.
BEYOND_SWIPE_LEFT, BEYOND_TRAILING_ACTION = "drag:700,224,540,224", "tap:728,224"
# The keyboard: the first key of the last row is Z in English and Y in German.
LAST_ROW_FIRST, KEYBOARD_SAVE = "tap:49,428", "tap:712,428"

# The navigation bar, or the top of the radar on Home. Enough to tell the pages apart, whatever they scrolled to.
BAR = (0, 64, 400, 128)
PAGES = {
    "home": [],
    "system": [LEFT],
    "wifi": [RIGHT],
    "licenses": [LEFT, TO_INFO, LICENSES],
    "license": [LEFT, TO_INFO, LICENSES, FIRST_LICENSE],
    "icao": [LEFT, ICAO_ROW],
    "callsign": [LEFT, CALLSIGN_ROW],
    "altitude": [LEFT, ALTITUDE_ROW],
}

# Page in front and tap to the page in front after it. The back position is the radar on Home.
TAPS = {"left": LEFT, "right": RIGHT, "back": BACK}
NEXT = {
    ("home", "left"): "system", ("home", "right"): "wifi", ("home", "back"): "home",
    ("system", "left"): "system", ("system", "right"): "wifi", ("system", "back"): "home",
    ("wifi", "left"): "system", ("wifi", "right"): "wifi", ("wifi", "back"): "home",
}

TO_LICENSES = PAGES["licenses"]
TO_LICENSE = PAGES["license"]
TO_ICAO = PAGES["icao"]
TO_CALLSIGN = PAGES["callsign"]
TO_ALTITUDE = PAGES["altitude"]


def typed(text):
    return [HEX[character] for character in text]


# Name, fixture with its display settings, gestures, page in front at the end, actions.
# A page of None is not checked. A fixture stores the address 3D2A7F and the callsign DEABC.
MUTE = [SWIPE_LEFT, TRAILING_ACTION]
CASES = [
    ("swipe and tap mutes", "advisory", [SWIPE_LEFT, TRAILING_ACTION], "home", ["mute DDE4A1"]),
    ("swipe alone mutes nothing", "advisory", [SWIPE_LEFT], "home", []),
    ("tap elsewhere closes the row", "advisory", [SWIPE_LEFT, RADAR, TRAILING_ACTION], "home", []),
    ("idle closes the row", "advisory", [SWIPE_LEFT, IDLE, TRAILING_ACTION], "home", []),
    ("bell ends the mute", "advisory", MUTE + [BELL], "home", ["mute DDE4A1", "unmute DDE4A1"]),
    ("swipe and tap ends the mute", "advisory", MUTE + [SWIPE_LEFT, TRAILING_ACTION], "home",
     ["mute DDE4A1", "unmute DDE4A1"]),
    ("swipe and tap sets own ship", "own-ship", [SWIPE_RIGHT, LEADING_ACTION], "home", ["set_own_ship 3D2A7F"]),
    ("no own ship on another target", "normal", [SWIPE_RIGHT, LEADING_ACTION], "home", []),
    ("theme", "normal", [LEFT, DARK], None, ["set_dark_theme 1"]),
    ("track history", "normal", [LEFT, TO_INFO, TRACK_HISTORY], "system", ["set_track_history 1"]),
    ("simple symbols", "normal", [LEFT, TO_INFO, SIMPLE_SYMBOLS], "system", ["set_simple_symbols 1"]),
    ("the chosen symbols again", "normal", [LEFT, TO_INFO, PLANE_SYMBOLS], "system", []),
    ("plane symbols", "normal simple-symbols", [LEFT, TO_INFO, PLANE_SYMBOLS], "system", ["set_simple_symbols 0"]),
    ("conflict indicator", "normal", [LEFT, TO_INFO, CONFLICT_INDICATOR], "system", ["set_conflict_indicator 1"]),
    ("conflict indicator off", "normal conflict-indicator", [LEFT, TO_INFO, CONFLICT_INDICATOR], "system",
     ["set_conflict_indicator 0"]),
    ("track button", "normal", [TRACK], None, ["set_track_history 1"]),
    ("time zone row is display only", "normal", [LEFT, TIME_ZONE], "system", []),
    ("automatic off", "normal", [LEFT, AUTOMATIC], None, ["set_time_zone_automatic 0"]),
    ("automatic off and on", "normal", [LEFT, AUTOMATIC, AUTOMATIC], "system",
     ["set_time_zone_automatic 0", "set_time_zone_automatic 1"]),
    ("pick UTC", "normal", [LEFT, AUTOMATIC, TIME_ZONE, UTC_ROW], None, ["set_time_zone_automatic 0", "set_time_zone UTC"]),
    ("nothing detected takes no tap", "no-fix", [LEFT, AUTOMATIC, TIME_ZONE, DETECTED_ROW, BACK], None,
     ["set_time_zone_automatic 0"]),
    ("pick a zone of a region", "normal", [LEFT, AUTOMATIC, TIME_ZONE, AFRICA, FIRST_ZONE], None,
     ["set_time_zone_automatic 0", "set_time_zone Africa/Abidjan"]),
    ("region and back twice", "normal", [LEFT, AUTOMATIC, TIME_ZONE, AFRICA, BACK, BACK, AUTOMATIC], "system",
     ["set_time_zone_automatic 0", "set_time_zone_automatic 1"]),
    ("track button twice", "normal", [TRACK, TRACK], "home", ["set_track_history 1", "set_track_history 0"]),
    ("zoom out", "normal", [ZOOM_OUT], None, ["set_radar_range 8"]),
    ("zoom in stops at the nearest range", "normal", [ZOOM_IN, ZOOM_IN], None, ["set_radar_range 2"]),
    ("zoom out and in", "normal", [ZOOM_OUT, ZOOM_IN], "home", ["set_radar_range 8", "set_radar_range 4"]),
    ("licenses and back", "normal", TO_LICENSES + [BACK], "system", []),
    ("licenses twice", "normal", TO_LICENSES + [BACK, LICENSES], "licenses", []),
    ("license and back", "normal", TO_LICENSE + [BACK], "licenses", []),
    ("license title goes back one level", "normal", TO_LICENSE + ["tap:100,96"], "licenses", []),
    ("wifi back row reaches the far edge", "normal", [RIGHT, "tap:799,96"], "home", []),
    ("altitude field goes back", "normal", TO_ALTITUDE + ["tap:740,96"], "system", []),
    ("license twice", "normal", TO_LICENSE + [BACK, FIRST_LICENSE], "license", []),
    ("license and back to system", "normal", TO_LICENSE + [BACK, BACK], "system", []),
    ("license and back to home", "normal", TO_LICENSE + [BACK, BACK, BACK], "home", []),
    ("header left from license", "normal", TO_LICENSE + [LEFT], "system", []),
    ("header right from license", "normal", TO_LICENSE + [RIGHT], "wifi", []),
    ("header right from license and back", "normal", TO_LICENSE + [RIGHT, BACK], "home", []),
    ("system after license", "normal", TO_LICENSE + [RIGHT, BACK, LEFT], "system", []),
    ("idle returns home", "normal", [LEFT, IDLE], "home", []),
    ("idle returns home from wifi", "normal", [RIGHT, IDLE], "home", []),
    ("licenses stay longer", "normal", TO_LICENSES + [IDLE], "licenses", []),
    ("idle returns home from licenses", "normal", TO_LICENSES + [IDLE_READING], "home", []),
    ("idle returns home from license", "normal", TO_LICENSE + [IDLE_READING], "home", []),
    ("address entry stays longer", "normal", TO_ICAO + [IDLE], "icao", []),
    ("idle returns home from address entry", "normal", TO_ICAO + [IDLE_READING], "home", []),
    ("idle returns home from callsign entry", "normal", TO_CALLSIGN + [IDLE_READING], "home", []),
    ("address typed and saved", "normal", TO_ICAO + typed("3C65A9") + [HEX_SAVE], "system", ["set_own_ship 3C65A9"]),
    ("back drops the address", "normal", TO_ICAO + typed("3C65A9") + [BACK], "system", []),
    ("five characters are not saved", "normal", TO_ICAO + typed("3C65A") + [HEX_SAVE], "icao", []),
    ("the stored address is not saved again", "normal", TO_ICAO + [HEX_SAVE], "icao", []),
    ("a seventh character is ignored", "normal", TO_ICAO + typed("3C65A91") + [HEX_SAVE], "system",
     ["set_own_ship 3C65A9"]),
    ("delete removes the last character", "normal", TO_ICAO + typed("3C65A9") + [HEX_DELETE] + typed("8") + [HEX_SAVE],
     "system", ["set_own_ship 3C65A8"]),
    ("delete first empties the field, save removes the address", "normal", TO_ICAO + [HEX_DELETE, HEX_SAVE], "system",
     ["set_own_ship none"]),
    ("nothing to remove without an address", "normal no-own-ship", TO_ICAO + [HEX_DELETE, HEX_SAVE], "icao", []),
    ("a draft does not survive back", "normal", TO_ICAO + typed("3C65A9") + [BACK, ICAO_ROW, HEX_SAVE], "icao", []),
    ("callsign typed and saved", "normal", TO_CALLSIGN + [LAST_ROW_FIRST, KEYBOARD_SAVE], "system",
     ["set_own_callsign Z"]),
    ("german keyboard", "normal language=de", TO_CALLSIGN + [LAST_ROW_FIRST, KEYBOARD_SAVE], "system",
     ["set_own_callsign Y"]),
    # A fixture stores an altitude filter of 3000 ft.
    ("altitude typed and saved", "normal", TO_ALTITUDE + typed("2700") + [HEX_SAVE], "system", ["set_altitude_filter 2700"]),
    ("a fifth digit of the altitude is ignored", "normal", TO_ALTITUDE + typed("99991") + [HEX_SAVE], "system",
     ["set_altitude_filter 9999"]),
    ("zero switches the altitude filter off", "normal", TO_ALTITUDE + typed("0") + [HEX_SAVE], "system",
     ["set_altitude_filter 0"]),
    ("a number has no leading zero", "normal", TO_ALTITUDE + typed("0300") + [HEX_SAVE], "system",
     ["set_altitude_filter 300"]),
    ("zeros before the stored number are not saved", "normal", TO_ALTITUDE + typed("003000") + [HEX_SAVE], "altitude", []),
    ("the stored number is not saved again", "normal", TO_ALTITUDE + [HEX_SAVE], "altitude", []),
    ("a number that was typed again is not saved", "normal", TO_ALTITUDE + typed("3000") + [HEX_SAVE], "altitude", []),
    ("no number is not saved", "normal", TO_ALTITUDE + [HEX_DELETE, HEX_SAVE], "altitude", []),
    ("back drops the number", "normal", TO_ALTITUDE + typed("5") + [BACK], "system", []),
    ("number entry stays longer", "normal", TO_ALTITUDE + [IDLE], "altitude", []),
    ("idle returns home from number entry", "normal", TO_ALTITUDE + [IDLE_READING], "home", []),
    ("a row below the band mutes its target", "normal range=2", [BEYOND_SWIPE_LEFT, BEYOND_TRAILING_ACTION], None,
     ["mute DD9F31"]),
]

# Two ways to one state show one picture: a gesture, and the fixture that starts in that state.
# No design image is asked, so a change of the design touches none of these.
# Name, then both ways: fixture with its display settings, gestures.
# Where the name says that the picture changes, a third way is the state before the change. It shows another picture:
# two ways that draw nothing agree as well.
MANUAL = "normal zone=Europe/Berlin"
# Chosen by hand with no zone stored, which is UTC: components/airgrep/tz/tz.hpp.
MANUAL_NONE = "normal zone="
# A fixture stays at its scene until the device is let run.
NEXT_SECOND = "device:1000"
SAME = [
    ("the track button shows the tracks", ("normal", [TRACK]), ("normal tracks", []), ("normal", [])),
    ("the track button hides the tracks", ("normal tracks", [TRACK]), ("normal", []), ("normal tracks", [])),
    ("the control shows the simple symbols", ("normal", [LEFT, TO_INFO, SIMPLE_SYMBOLS, BACK]),
     ("normal simple-symbols", []), ("normal", [])),
    # The Remote Display asks the conflict engine once in a second of the device, so the Bands come with the next one.
    ("the switch shows the Bands", ("preventive", [LEFT, TO_INFO, CONFLICT_INDICATOR, BACK, NEXT_SECOND]),
     ("preventive conflict-indicator", [NEXT_SECOND]), ("preventive", [NEXT_SECOND])),
    ("the switch hides the Bands", ("preventive conflict-indicator", [LEFT, TO_INFO, CONFLICT_INDICATOR, BACK]),
     ("preventive", []), ("preventive conflict-indicator", [])),
    ("zoom in reaches 2 NM", ("normal tracks", [ZOOM_IN]), ("normal tracks range=2", []), ("normal tracks", [])),
    ("zoom out twice reaches 16 NM", ("busy tracks", [ZOOM_OUT, ZOOM_OUT]), ("busy tracks range=16", []),
     ("busy tracks", [])),
    ("zoom out stops at 32 NM", ("busy tracks range=32", [ZOOM_OUT]), ("busy tracks range=32", [])),
    # A mute is kept by the display, so no fixture starts with one: the second way mutes, ends the mute and mutes.
    ("swipe and tap shows the target muted", ("advisory", MUTE), ("advisory", MUTE + [BELL] + MUTE), ("advisory", [])),
    ("the bell shows the target as before", ("advisory", MUTE + [BELL]), ("advisory", []), ("advisory", MUTE)),
    ("automatic off shows the manual zone", ("normal", [LEFT, AUTOMATIC]), (MANUAL_NONE, [LEFT]), ("normal", [LEFT])),
    ("time zone and back leaves System as it was", (MANUAL, [LEFT, TIME_ZONE, BACK]), (MANUAL, [LEFT])),
    ("altitude and back leaves System as it was", ("normal", TO_ALTITUDE + [BACK]), ("normal", [LEFT])),
    ("license and back twice rests where System was scrolled to", ("normal", TO_LICENSE + [BACK, BACK]),
     ("normal", [LEFT, TO_INFO])),
]
# docs/ui-design/guidance-bands.md: in a warning the alert bars take the place of both tapes and the wedges stay.
# So the strips beside the radar are those of the same warning without Bands, and the radar is not.
STRIPS = ((0, 64, 16, 480), (384, 64, 400, 480))
RADAR = (16, 64, 384, 480)
WARNING = "conflict tracks range=2"


def run(name, gestures, fixture="normal"):
    """Picture after the gestures and the actions the display was asked for."""
    target = OUTPUT / f"{name.replace(' ', '-').replace(',', '')}.png"
    options = [option for gesture in gestures for option in ("--do", gesture)]
    output = subprocess.run([RENDER, DISPLAY_LIBRARY, fixture, target, *options], check=True, capture_output=True,
                            text=True).stdout
    actions = [line.removeprefix("asked ") for line in output.splitlines() if line.startswith("asked ")]
    return Image.open(target).convert("RGB"), actions


def same(a, b, box=None):
    return ImageChops.difference(a.crop(box), b.crop(box)).getbbox() is None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    # The page after a tap depends on the page before it alone, so two taps pass every transition once.
    # A longer walk only finds a page that remembers how it was reached.
    parser.add_argument("--length", type=int, default=2, help="Longest sequence of the walk, default 2")
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    reference = {page: run(page, gestures)[0] for page, gestures in PAGES.items()}

    def walk(taps):
        page = "home"
        for tap in taps:
            page = NEXT[page, tap]
        image, _ = run("-".join(taps), [TAPS[tap] for tap in taps])
        return " ".join(taps), None if same(image, reference[page]) else f"expected {page}"

    def case(entry):
        name, fixture, gestures, page, actions = entry
        image, asked = run(name, gestures, fixture)
        front = [known for known in PAGES if same(image, reference[known], BAR)]
        wrong = []
        if page and front != [page]:
            wrong.append(f"expected {page}, in front {front or 'unknown'}")
        if asked != actions:
            wrong.append(f"expected actions {actions}, got {asked}")
        return name, ", ".join(wrong) or None

    def one_picture(entry):
        name, *ways = entry
        first, second, *before = (run(f"{name} {side}", gestures, fixture)[0]
                                  for side, (fixture, gestures) in zip("abc", ways))
        if not same(first, second):
            return name, "the two ways show two pictures"
        return name, "the state before shows the same picture" if before and same(first, before[0]) else None

    def bars_for_tapes():
        with_bands, without = (run(f"warning {side}", [], fixture)[0]
                               for side, fixture in (("bands", f"{WARNING} conflict-indicator"), ("bars", WARNING)))
        if not all(same(with_bands, without, strip) for strip in STRIPS):
            return "the strips differ from those of the warning without Bands"
        return "the wedges are gone" if same(with_bands, without, RADAR) else None

    sequences = [taps for length in range(2, args.length + 1) for taps in itertools.product(TAPS, repeat=length)]
    with ThreadPoolExecutor(8) as pool:
        walked = list(pool.map(walk, sequences))
        cases = list(pool.map(case, CASES))
        pictures = list(pool.map(one_picture, SAME))
    pictures.append(("an alert takes the place of the tapes alone", bars_for_tapes()))
    for name, wrong in walked + cases + pictures:
        if wrong:
            print(f"FAIL {name}: {wrong}")
    print(f"walk: {sum(not wrong for _, wrong in walked)} of {len(walked)} sequences end on the expected page")
    print(f"cases: {sum(not wrong for _, wrong in cases)} of {len(cases)} behave as expected")
    print(f"same: {sum(not wrong for _, wrong in pictures)} of {len(pictures)} pairs of ways show one picture")
    return 1 if any(wrong for _, wrong in walked + cases + pictures) else 0


if __name__ == "__main__":
    sys.exit(main())
