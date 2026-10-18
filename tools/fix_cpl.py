#!/usr/bin/env python3
"""Post-process the jlcpcb-tools CPL so JLCPCB's part models land on our pads.

Run with KiCad's bundled Python after every CPL export:
  tools/kpy tools/fix_cpl.py

jlcpcb-tools writes the centre of the pad bounding box as Mid X/Y and applies its regex
rotation table. JLCPCB places its own footprint (origin at the package centre) at that point.
For the footprints below the two disagree (JLCDFM SMT report, 3 Oct 2026; K3 supplier CAD,
6 Oct 2026), so this script
rewrites their rows from the board: rotation = board rotation + ROT, position = footprint origin
+ OFFSET (the JLC origin in footprint-local mm, rotated with the footprint). Values were found by
matching the EasyEDA footprint of each LCSC part to our pads by pad number, except the
unnumbered K3 outer contacts: those use the body and common terminal. Its supplier CAD
has a peg-offset discrepancy; verify fit and the assembly preview before ordering. Re-running is safe:
every fixed row is recomputed from the board, not from the previous CPL.

Rows not listed are left alone; the script only reports those whose CPL position is more than
0.02 mm from the footprint origin, so a new asymmetric footprint shows up for checking.
"""
import csv
import math
import sys
from pathlib import Path

import pcbnew

sys.path.insert(0, str(Path(__file__).resolve().parent))
import project  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
PROJECT = Path(project.resolve())  # --project is not parsed here; set CARRIER_PROJECT
BOARD = PROJECT / "carrier.kicad_pcb"
CPL = PROJECT / "jlcpcb/production_files/CPL-carrier.csv"

# footprint name: (rotation added to the board rotation, JLC origin in footprint-local mm)
FIXES = {
    "Texas_DSG0008A_WSON-8-1EP_2x2mm_P0.5mm_EP0.9x1.6mm": (0, (0.0, 0.0)),  # U7, C524777; plugin table says 270
    "TDFN-8_L2.0-W2.0-P0.50-EP0.8x1.38_T822": (270, (0.0, 0.0)),  # U8, C2682616; JLC pin 1 bottom left
    "SW-SMD_K3-1280S-F1": (270, (0.0, 0.0)),  # SW1-SW3, C92658; supplier origin is common pad, actuator points right
    # HRO drawing leaves outer terminals unnumbered. Our 1/3 convention is reversed
    # relative to the supplier CAD after aligning the body. See workflows/physical-controls.md.
    "USB_C_Receptacle_Hanbo_MC-107DMS": (0, (0.0, -1.322)),  # USB-C, C3039295 (slots and pegs, pads moved 3 Oct)
    "FC2QFN-24_L3.0-W3.0-P0.40_MAX77757": (0, (0.0019, 0.0736)),  # U4, C7454852
}


def rot(x, y, deg):  # footprint-local to board offset (KiCad: y down, positive = counter-clockwise)
    a = math.radians(deg)
    return x * math.cos(a) + y * math.sin(a), -x * math.sin(a) + y * math.cos(a)


def num(v):
    return ("%.4f" % v).rstrip("0").rstrip(".")


def main():
    board = pcbnew.LoadBoard(str(BOARD))
    aux = board.GetDesignSettings().GetAuxOrigin()
    ax, ay = pcbnew.ToMM(aux.x), pcbnew.ToMM(aux.y)
    with open(CPL, newline="", encoding="utf-8") as f:
        rows = list(csv.reader(f))
    head, body = rows[0], rows[1:]
    col = {n: head.index(n) for n in ("Designator", "Mid X", "Mid Y", "Rotation", "Layer")}
    fixed = []
    for row in body:
        ref = row[col["Designator"]]
        fp = board.FindFootprintByReference(ref)
        if fp is None:
            sys.exit("%s is in the CPL but not on the board; re-export the CPL" % ref)
        if fp.IsFlipped():
            sys.exit("%s is on the bottom side; this script only handles top-side parts" % ref)
        pos = fp.GetPosition()
        x0, y0 = pcbnew.ToMM(pos.x) - ax, pcbnew.ToMM(pos.y) - ay
        brot = fp.GetOrientationDegrees()
        name = str(fp.GetFPID().GetLibItemName())
        if name in FIXES:
            r, (ox, oy) = FIXES[name]
            dx, dy = rot(ox, oy, brot)
            old = (row[col["Mid X"]], row[col["Mid Y"]], row[col["Rotation"]])
            row[col["Mid X"]] = num(x0 + dx)
            row[col["Mid Y"]] = num(-(y0 + dy))
            row[col["Rotation"]] = num((brot + r) % 360)
            new = (row[col["Mid X"]], row[col["Mid Y"]], row[col["Rotation"]])
            fixed.append("%-4s %s  (%s, %s, %s) -> (%s, %s, %s)" % ((ref, name) + old + new))
        else:
            d = math.hypot(float(row[col["Mid X"]]) - x0, -float(row[col["Mid Y"]]) - y0)
            if d > 0.02:
                print("check %-4s %s: CPL position is %.3f mm from the footprint origin" % (ref, name, d))
    with open(CPL, "w", newline="", encoding="utf-8") as f:
        csv.writer(f).writerows([head] + body)
    print("fixed %d rows in %s" % (len(fixed), CPL.relative_to(ROOT)))
    for line in fixed:
        print("  " + line)


if __name__ == "__main__":
    main()
