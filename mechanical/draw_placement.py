#!/usr/bin/env python3
"""Placement drawing of the carrier, read from the KiCad board.

Front view, origin upper left, +Y down, mm (same as KiCad). Writes
mechanical/placement.svg and mechanical/placement.png. The board file is only read.

Needs pcbnew and Pillow, so run it from the repository root with KiCad's Python:

  tools/kpy mechanical/draw_placement.py
"""
import re
from pathlib import Path

import pcbnew
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
KICAD = ROOT / "hardware/kicad"
OUT = ROOT / "mechanical"

# Schematic sheet -> (legend text, fill colour). Parts are coloured by the sheet they are on.
SHEETS = {
    "usb_input": ("USB input and protection", "#cbd5e1"),
    "charger": ("Charger", "#fdba74"),
    "boost_switch": ("5 V boost and load switch", "#fca5a5"),
    "battery_control": ("Battery, thermal and power control", "#d8b4fe"),
    "gauge_status": ("Gauge and status LEDs", "#86efac"),
    "buzzer_baro": ("Buzzer and barometer", "#a5b4fc"),
    "modules": ("Module sockets and service shunts", "#7dd3fc"),
}

# Module envelopes sit above the board on the sockets. Each one is anchored to pin 1 of a
# socket on the board; the offsets come from the module geometry in this folder.
#   name, note, anchor ref, envelope origin relative to pin 1 (dx, dy), size (w, h), label height, colour
MODULES = [
    ("GNSS GPS-18037", "", "J5", (-6.35, -36.83), (30.48, 38.10), 0.42, "#0369a1"),
    ("Display", "Seeed 100069905", "J6", (-24.2, -3.88), (26.4, 51.4), 0.53, "#047857"),
    ("ADSBee 1090U", "image estimate", "J10", (-27.8, -38.6), (30.0, 48.0), 0.42, "#b91c1c"),
]
GNSS_SMA = (15.24, 6.1, 6.4)   # axis from the envelope's left edge, length above the top edge, width
ADSBEE_USB = (3.0, 9.0)        # reserved overhang below the envelope, assumed width (position along the edge unknown)

ANTENNA = {"MH6": "GNSS", "MH7": "868 MHz", "MH8": "1090 MHz"}

# Reference labels on the drawing: ref -> side of the part the label goes on (c = centred).
LABELS = {
    "J1": "c", "J2": "t", "J3": "t", "J4": "b", "J5": "b", "J6": "l", "J7": "r", "J8": "b", "J9": "r",
    "J10": "l", "J11": "b", "JP1": "l", "JP2": "b", "SW1": "t", "SW2": "r", "BZ1": "c", "D4": "c", "D5": "c",
    "F1": "b", "L1": "c", "L2": "c", "U4": "c", "U5": "b", "U6": "c", "U7": "r", "U8": "t", "U9": "t", "U10": "b",
    "D3": "c", "TP1": "r", "TP2": "l", "TP3": "t", "TP4": "l", "TP5": "l", "TP6": "l", "TP7": "b",
}
KEY = [
    ("J1", "USB-C charging input"), ("J2 J3 J4", "pack NTC, battery, thermostat"),
    ("J5", "GNSS socket 1x8"), ("J6 J7 J8 J9", "display sockets 1x8, 1x7, 1x4, 1x2"),
    ("J10 J11", "ADSBee sockets 1x13, 1x5"), ("JP1 JP2", "5 V shunts: display, ADSBee"),
    ("SW1 SW2", "POWER and MUTE sliders"), ("D4 D5", "charge LED, power LED"),
    ("U4 L1", "charger MAX77757"), ("U6 L2", "boost TPS61088"), ("U7", "load switch TPS22975"),
    ("U5", "undervoltage supervisor"), ("U8 U10", "fuel gauge, barometer"), ("U9 BZ1", "buzzer driver, buzzer"),
    ("F1 D3", "pack fuse, VBUS TVS"), ("TP1 to TP7", "VBAT, VSYS, PWR_EN, THERM_OK, USB_VBUS, GND, 5 V"),
    ("MH1 to MH4", "case, 2.7 mm"), ("MH6 to MH8", "antenna bulkheads, 6.5 mm"),
    ("MH10 to MH14", "module standoffs, 2.7 mm"),
]

FONTS = ["/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/Supplemental/Arial.ttf",
         "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"]


def sheet_of_refs():
    refs = {}
    for name in SHEETS:
        text = (KICAD / f"{name}.kicad_sch").read_text()
        for ref in set(re.findall(r'\(reference "([A-Z]+[0-9]+)"\)', text)):
            refs[ref] = name
    return refs


def read_board():
    board = pcbnew.LoadBoard(str(KICAD / "carrier.kicad_pcb"))
    mm = pcbnew.ToMM
    edge = board.GetBoardEdgesBoundingBox()
    width, height = round(mm(edge.GetWidth()), 0), round(mm(edge.GetHeight()), 0)
    parts, holes = {}, {}
    for f in board.GetFootprints():
        ref = f.GetReference()
        if ref.startswith("FID"):
            continue
        if ref.startswith("MH"):
            pad = list(f.Pads())[0]
            holes[ref] = (mm(f.GetPosition().x), mm(f.GetPosition().y), mm(pad.GetDrillSize().x), mm(pad.GetSize().x))
            continue
        court = f.GetCourtyard(f.GetLayer())
        box = court.BBox() if court.OutlineCount() else f.GetBoundingBox(False, False)
        pin1 = next((p for p in f.Pads() if p.GetNumber() == "1"), None)
        parts[ref] = dict(x=mm(box.GetX()), y=mm(box.GetY()), w=mm(box.GetWidth()), h=mm(box.GetHeight()),
                          pin1=(mm(pin1.GetPosition().x), mm(pin1.GetPosition().y)) if pin1 else None)
    return width, height, parts, holes


def build():
    """Return the drawing as a list of primitives in mm."""
    W, H, parts, holes = read_board()
    sheet = sheet_of_refs()
    d = []
    rect = lambda x, y, w, h, **k: d.append(dict(t="rect", x=x, y=y, w=w, h=h, **k))
    circle = lambda x, y, r, **k: d.append(dict(t="circle", x=x, y=y, r=r, **k))
    line = lambda x1, y1, x2, y2, **k: d.append(dict(t="line", x1=x1, y1=y1, x2=x2, y2=y2, **k))
    text = lambda x, y, s, **k: d.append(dict(t="text", x=x, y=y, s=s, **k))

    # axes
    for v in range(0, int(W) + 1, 10):
        line(v, -1.5, v, 0, stroke="#64748b")
        text(v, -2.2, str(v), size=1.5, anchor="mb", fill="#64748b")
    for v in range(0, int(H) + 1, 10):
        line(-1.5, v, 0, v, stroke="#64748b")
        text(-2.0, v, str(v), size=1.5, anchor="rm", fill="#64748b")

    rect(0, 0, W, H, fill="#f8fafc", stroke="#0f172a", sw=0.3)

    # parts, coloured by schematic sheet
    for ref, p in sorted(parts.items()):
        colour = SHEETS[sheet[ref]][1] if ref in sheet else "#e2e8f0"
        rect(p["x"], p["y"], p["w"], p["h"], fill=colour, stroke="#475569", sw=0.08)

    # mounting holes
    for ref, (x, y, drill, pad) in sorted(holes.items()):
        if ref in ANTENNA:
            circle(x, y, pad / 2, fill="#e2e8f0", stroke="#64748b", sw=0.1)
        circle(x, y, drill / 2, fill="#ffffff", stroke="#0f172a", sw=0.2)
        label = f"{ref} {ANTENNA[ref]}" if ref in ANTENNA else ref
        below = y < H / 2
        if ref in ANTENNA:
            text(x, y + pad / 2 + 0.1, label, size=1.5, anchor="mt")
        else:
            text(x, y + (drill / 2 + 0.4 if below else -drill / 2 - 0.4), label, size=1.5, anchor="mt" if below else "mb")

    # module envelopes above the board
    for name, note, anchor, (dx, dy), (w, h), at, colour in MODULES:
        px, py = parts[anchor]["pin1"]
        x, y = px + dx, py + dy
        rect(x, y, w, h, stroke=colour, sw=0.3, dash=True)
        text(x + w / 2, y + h * at, name, size=2.0, anchor="mm", fill=colour, bold=True)
        for i, sub in enumerate(s for s in (note, f"{w:g} x {h:g} mm") if s):
            text(x + w / 2, y + h * at + 2.6 + 2.2 * i, sub, size=1.5, anchor="mm", fill=colour)
        if anchor == "J5":
            ax, length, width = GNSS_SMA
            rect(x + ax - width / 2, y - length, width, length, stroke=colour, sw=0.2, dash=True)
            text(x + ax, y - length - 0.4, "SMA", size=1.5, anchor="mb", fill=colour)
        if anchor == "J10":
            depth, width = ADSBEE_USB
            rect(x + w / 2 - width / 2, y + h, width, depth, stroke=colour, sw=0.2, dash=True)
            text(x + w / 2, y + h + depth / 2, "USB-C", size=1.5, anchor="mm", fill=colour)
            text(x + 4.0, y + 4.2, "U.FL", size=1.5, anchor="mm", fill=colour)

    # reference labels
    for ref, side in LABELS.items():
        p = parts[ref]
        cx, cy = p["x"] + p["w"] / 2, p["y"] + p["h"] / 2
        pos = {"c": (cx, cy, "mm"), "t": (cx, p["y"] - 0.3, "mb"), "b": (cx, p["y"] + p["h"] + 0.3, "mt"),
               "l": (p["x"] - 0.4, cy, "rm"), "r": (p["x"] + p["w"] + 0.4, cy, "lm")}[side]
        text(pos[0], pos[1], ref, size=1.5, anchor=pos[2], bold=True)

    # key
    kx, ky = W + 6, 0
    text(kx, ky, f"Carrier placement, front view, {W:g} x {H:g} mm", size=2.4, anchor="lt", bold=True)
    ky += 6
    for name, (label, colour) in SHEETS.items():
        rect(kx, ky, 3, 2, fill=colour, stroke="#475569", sw=0.08)
        text(kx + 4.5, ky + 1, label, size=1.7, anchor="lm")
        ky += 3.2
    rect(kx, ky, 3, 2, stroke="#334155", sw=0.3, dash=True)
    text(kx + 4.5, ky + 1, "Module outline above the board", size=1.7, anchor="lm")
    ky += 5.5
    for refs, what in KEY:
        text(kx, ky, refs, size=1.7, anchor="lt", bold=True)
        text(kx + 14.5, ky, what, size=1.7, anchor="lt")
        ky += 3.0
    ky += 2
    for note in ("Origin upper left, +Y down, mm, as in KiCad.",
                 "Part outlines are courtyards read from carrier.kicad_pcb.",
                 "Module outlines follow the socket positions.",
                 "The ADSBee outline is an image estimate.",
                 "Regenerate with mechanical/draw_placement.py."):
        text(kx, ky, note, size=1.5, anchor="lt", fill="#475569")
        ky += 2.6
    return d, (-6, -6, W + 6 + 66, max(H, ky) + 3)   # view box: x0, y0, x1, y1


def write_svg(d, view, path):
    x0, y0, x1, y1 = view
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{(x1 - x0) * 10:g}" height="{(y1 - y0) * 10:g}" '
         f'viewBox="{x0:g} {y0:g} {x1 - x0:g} {y1 - y0:g}" font-family="Helvetica,Arial,sans-serif">',
         f'<rect x="{x0:g}" y="{y0:g}" width="{x1 - x0:g}" height="{y1 - y0:g}" fill="#ffffff"/>']
    for e in d:
        style = f'fill="{e.get("fill", "none")}" stroke="{e.get("stroke", "none")}" stroke-width="{e.get("sw", 0.15):g}"'
        if e.get("dash"):
            style += ' stroke-dasharray="1.2 0.8"'
        if e["t"] == "rect":
            o.append(f'<rect x="{e["x"]:.3f}" y="{e["y"]:.3f}" width="{e["w"]:.3f}" height="{e["h"]:.3f}" {style}/>')
        elif e["t"] == "circle":
            o.append(f'<circle cx="{e["x"]:.3f}" cy="{e["y"]:.3f}" r="{e["r"]:.3f}" {style}/>')
        elif e["t"] == "line":
            o.append(f'<line x1="{e["x1"]:.3f}" y1="{e["y1"]:.3f}" x2="{e["x2"]:.3f}" y2="{e["y2"]:.3f}" {style}/>')
        else:
            h, v = e["anchor"]
            ta = {"l": "start", "m": "middle", "r": "end"}[h]
            dy = {"t": 0.8, "m": 0.35, "b": 0.0}[v] * e["size"]
            weight = ' font-weight="bold"' if e.get("bold") else ""
            o.append(f'<text x="{e["x"]:.3f}" y="{e["y"] + dy:.3f}" font-size="{e["size"]:g}" text-anchor="{ta}"{weight} '
                     f'fill="{e.get("fill", "#0f172a")}">{e["s"]}</text>')
    o.append("</svg>")
    path.write_text("\n".join(o) + "\n")


def font(size_px, bold):
    for f in FONTS:
        try:
            return ImageFont.truetype(f, size_px, index=1 if bold and f.endswith(".ttc") else 0)
        except OSError:
            continue
    return ImageFont.load_default(size_px)


def write_png(d, view, path, px_per_mm=12, oversample=2):
    k = px_per_mm * oversample
    x0, y0, x1, y1 = view
    im = Image.new("RGB", (int((x1 - x0) * k), int((y1 - y0) * k)), "#ffffff")
    g = ImageDraw.Draw(im)
    X = lambda x: (x - x0) * k
    Y = lambda y: (y - y0) * k

    def dashed(xa, ya, xb, yb, colour, width):
        length = ((xb - xa) ** 2 + (yb - ya) ** 2) ** 0.5
        on, period, t = 1.2 * k, 2.0 * k, 0.0
        while t < length:
            a, b = t / length, min(t + on, length) / length
            g.line([xa + (xb - xa) * a, ya + (yb - ya) * a, xa + (xb - xa) * b, ya + (yb - ya) * b], fill=colour, width=width)
            t += period

    for e in d:
        width = max(1, round(e.get("sw", 0.15) * k))
        fill, stroke = e.get("fill"), e.get("stroke")
        if e["t"] == "rect":
            xa, ya, xb, yb = X(e["x"]), Y(e["y"]), X(e["x"] + e["w"]), Y(e["y"] + e["h"])
            if e.get("dash"):
                for seg in ((xa, ya, xb, ya), (xb, ya, xb, yb), (xb, yb, xa, yb), (xa, yb, xa, ya)):
                    dashed(*seg, stroke, width)
            else:
                g.rectangle([xa, ya, xb, yb], fill=fill, outline=stroke, width=width)
        elif e["t"] == "circle":
            r = e["r"] * k
            g.ellipse([X(e["x"]) - r, Y(e["y"]) - r, X(e["x"]) + r, Y(e["y"]) + r], fill=fill, outline=stroke, width=width)
        elif e["t"] == "line":
            g.line([X(e["x1"]), Y(e["y1"]), X(e["x2"]), Y(e["y2"])], fill=stroke, width=width)
        else:
            g.text((X(e["x"]), Y(e["y"])), e["s"], font=font(round(e["size"] * k), e.get("bold")),
                   fill=e.get("fill", "#0f172a"), anchor=e["anchor"][0] + {"t": "a", "m": "m", "b": "d"}[e["anchor"][1]])
    im = im.resize((im.width // oversample, im.height // oversample), Image.LANCZOS)
    im.save(path, optimize=True)


if __name__ == "__main__":
    drawing, view = build()
    write_svg(drawing, view, OUT / "placement.svg")
    write_png(drawing, view, OUT / "placement.png")
    print("wrote", OUT / "placement.svg", "and", OUT / "placement.png")
