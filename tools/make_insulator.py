#!/usr/bin/env python3
"""Generate the insulator board (hardware/insulator/) from the carrier geometry.

Run with KiCad's bundled Python:
  tools/kpy tools/make_insulator.py

The insulator sits between the carrier and the battery pack on the four case standoffs and keeps
the carrier's through-hole pin tails away from the pack. 0.8 mm FR-4, no copper (order as a
separate 1-layer or 2-layer board without copper, same cart as the carrier). Everything is taken
from the carrier so the two stay aligned: the Edge.Cuts outline, the case holes MH1 to MH4
(copies of the carrier footprints) and the antenna bulkhead mounts MH6 to MH8, which get 11 mm clearance holes for
the bulkhead nuts and pigtails below the carrier. The carrier-side silkscreen outlines every
through-hole part of the carrier (pads + 0.5 mm) so the pin tails can be checked against the
standoff height. Re-run after moving holes or through-hole parts on the carrier; it overwrites
insulator.kicad_pcb.
"""
from pathlib import Path

import pcbnew

import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
import project  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
CONN = project.connectors(project.resolve())
CARRIER = Path(project.pcb(project.resolve()))  # both carriers share the outline and holes
OUT = ROOT / "hardware/insulator/insulator.kicad_pcb"
REV = "Rev A 2026-10-03"
THICKNESS = 0.8
ANTENNA_CLEAR = 11.0  # mm, clears the bulkhead nut and pigtail at the 6.5 mm antenna holes
MM, TO = pcbnew.FromMM, pcbnew.ToMM

carrier = pcbnew.LoadBoard(str(CARRIER))
OUT.parent.mkdir(parents=True, exist_ok=True)
board = pcbnew.NewBoard(str(OUT))
board.SetCopperLayerCount(2)
ds = board.GetDesignSettings()
ds.SetBoardThickness(MM(THICKNESS))

# outline
for d in carrier.GetDrawings():
    if d.GetLayer() != pcbnew.Edge_Cuts:
        continue
    s = pcbnew.PCB_SHAPE(board)
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(d.GetWidth())
    if d.GetShape() == pcbnew.SHAPE_T_ARC:
        s.SetShape(pcbnew.SHAPE_T_ARC)
        s.SetArcGeometry(d.GetStart(), d.GetArcMid(), d.GetEnd())
    else:
        s.SetShape(d.GetShape())
        s.SetStart(d.GetStart())
        s.SetEnd(d.GetEnd())
    board.Add(s)


# case holes: copies of the carrier footprints, so reference, position and drill match
for ref in ("MH1", "MH2", "MH3", "MH4"):
    fp = pcbnew.FOOTPRINT(carrier.FindFootprintByReference(ref))
    fp.Reference().SetVisible(False)
    board.Add(fp)

# antenna bulkhead mounts: plain 11 mm non-plated holes
for ref in ("MH6", "MH7", "MH8"):
    c = carrier.FindFootprintByReference(ref).GetPosition()
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference(ref)
    fp.Reference().SetVisible(False)
    fp.SetValue("antenna clearance %.0f mm" % ANTENNA_CLEAR)
    fp.SetAttributes(pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES | pcbnew.FP_BOARD_ONLY)
    pad = pcbnew.PAD(fp)
    pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
    pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
    pad.SetSize(pcbnew.VECTOR2I(MM(ANTENNA_CLEAR), MM(ANTENNA_CLEAR)))
    pad.SetDrillSize(pcbnew.VECTOR2I(MM(ANTENNA_CLEAR), MM(ANTENNA_CLEAR)))
    pad.SetLayerSet(pad.UnplatedHoleMask())
    fp.Add(pad)
    board.Add(fp)
    fp.SetPosition(c)
    pad.SetPosition(c)


# cable pass-through for the pack leads (BAT, NTC, thermostat) when the three pack headers are mounted on the back
# of the carrier: one slot between the NTC/battery row and the thermostat, sized for the three 2-wire leads plus one
# more PHR-2 plug (about 6.0 x 4.5 mm) pushed past them without forcing it
SLOT_W, SLOT_H = 15.0, 7.5


def pin_xy(ref, num):
    for p in carrier.FindFootprintByReference(ref).Pads():
        if p.GetNumber() == num:
            return TO(p.GetPosition().x), TO(p.GetPosition().y)


row_y = max(pin_xy(CONN["ntc"], "1")[1], pin_xy(CONN["battery"], "1")[1])
xs = [pin_xy(r, n)[0] for r in (CONN["ntc"], CONN["battery"], CONN["thermostat"]) for n in ("1", "2")]
SLOT_X = (min(xs) + max(xs)) / 2
SLOT_Y = (row_y + pin_xy(CONN["thermostat"], "1")[1]) / 2
slot = pcbnew.FOOTPRINT(board)
slot.SetReference("H1")
slot.Reference().SetVisible(False)
slot.SetValue("cable slot %.1f x %.1f mm" % (SLOT_W, SLOT_H))
slot.SetAttributes(pcbnew.FP_EXCLUDE_FROM_BOM | pcbnew.FP_EXCLUDE_FROM_POS_FILES | pcbnew.FP_BOARD_ONLY)
sp = pcbnew.PAD(slot)
sp.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
sp.SetShape(pcbnew.PAD_SHAPE_OVAL)
sp.SetSize(pcbnew.VECTOR2I(MM(SLOT_W), MM(SLOT_H)))
sp.SetDrillShape(pcbnew.PAD_DRILL_SHAPE_OBLONG)
sp.SetDrillSize(pcbnew.VECTOR2I(MM(SLOT_W), MM(SLOT_H)))
sp.SetLayerSet(sp.UnplatedHoleMask())
slot.Add(sp)
board.Add(slot)
slot.SetPosition(pcbnew.VECTOR2I(MM(SLOT_X), MM(SLOT_Y)))
sp.SetPosition(pcbnew.VECTOR2I(MM(SLOT_X), MM(SLOT_Y)))
SLOT_BOX = (SLOT_X - SLOT_W / 2 - 0.3, SLOT_Y - SLOT_H / 2 - 0.3, SLOT_X + SLOT_W / 2 + 0.3, SLOT_Y + SLOT_H / 2 + 0.3)


def silk_rect(layer, x0, y0, x1, y1, w=0.15):
    r = pcbnew.PCB_SHAPE(board)
    r.SetShape(pcbnew.SHAPE_T_RECTANGLE)
    r.SetLayer(layer)
    r.SetWidth(MM(w))
    r.SetStart(pcbnew.VECTOR2I(MM(x0), MM(y0)))
    r.SetEnd(pcbnew.VECTOR2I(MM(x1), MM(y1)))
    board.Add(r)


def text(layer, s, x, y, size=1.5, mirror=False):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s)
    t.SetLayer(layer)
    t.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
    t.SetTextSize(pcbnew.VECTOR2I(MM(size), MM(size)))
    t.SetTextThickness(MM(max(0.1524, size * 0.15)))
    t.SetMirrored(mirror)
    board.Add(t)


# carrier through-hole parts: where pin tails come through (carrier side of the insulator),
# clipped 0.5 mm inside the board edge; each label goes below, above, left or right of its
# outline, whichever side is free of the other outlines
edge = carrier.GetBoardEdgesBoundingBox()
EX0, EY0, EX1, EY1 = TO(edge.GetLeft()) + 0.5, TO(edge.GetTop()) + 0.5, TO(edge.GetRight()) - 0.5, TO(edge.GetBottom()) - 0.5
rects = []
for f in sorted(carrier.GetFootprints(), key=lambda f: f.GetReference()):
    pads = [p for p in f.Pads() if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH]
    if not pads:
        continue
    bb = pads[0].GetBoundingBox()
    for p in pads[1:]:
        bb.Merge(p.GetBoundingBox())
    rects.append((f.GetReference(), max(EX0, TO(bb.GetLeft()) - 0.5), max(EY0, TO(bb.GetTop()) - 0.5),
                  min(EX1, TO(bb.GetRight()) + 0.5), min(EY1, TO(bb.GetBottom()) + 0.5)))


def overlaps(box, others, gap=0.3):
    x0, y0, x1, y1 = box
    return any(not (x1 + gap < a or b + gap < x0 or y1 + gap < c or d + gap < y0) for a, c, b, d in others) or \
        x0 < EX0 or y0 < EY0 or x1 > EX1 or y1 > EY1


boxes = [(r[1], r[2], r[3], r[4]) for r in rects] + [SLOT_BOX]
for ref, x0, y0, x1, y1 in rects:
    silk_rect(pcbnew.F_SilkS, x0, y0, x1, y1)
    w, h = len(ref) * 0.9 + 0.2, 1.2
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    for tx, ty in ((cx, y1 + 1.0), (cx, y0 - 1.0), (x0 - w / 2 - 0.5, cy), (x1 + w / 2 + 0.5, cy)):
        box = (tx - w / 2, ty - h / 2, tx + w / 2, ty + h / 2)
        if not overlaps(box, [b for b in boxes if b != (x0, y0, x1, y1)]):
            break
    boxes.append(box)
    text(pcbnew.F_SilkS, ref, tx, ty, size=1.0)

# labels in the free area beside the module sockets
text(pcbnew.F_SilkS, "CARRIER INSULATOR  %s" % REV, 22.0, 58.0, size=1.2)
text(pcbnew.F_SilkS, "CARRIER SIDE  -  0.8 mm FR-4, no copper", 22.0, 60.5, size=1.0)
text(pcbnew.F_SilkS, "outlines: carrier through-hole pins", 22.0, 62.8, size=1.0)
text(pcbnew.B_SilkS, "CABLES", SLOT_X, SLOT_Y + SLOT_H / 2 + 1.5, size=1.2, mirror=True)
text(pcbnew.B_SilkS, "BATTERY SIDE", 52.5, 46.0, size=2.0, mirror=True)
text(pcbnew.B_SilkS, "CARRIER INSULATOR  %s" % REV, 52.5, 50.0, size=1.5, mirror=True)
n = len(rects)

board.Save(str(OUT))
print("wrote %s: outline, MH1-MH4, antenna clearance MH6-MH8, cable slot %.1f x %.1f mm at (%.2f, %.2f), "
      "%d through-hole outlines" % (OUT.relative_to(ROOT), SLOT_W, SLOT_H, SLOT_X, SLOT_Y, n))
