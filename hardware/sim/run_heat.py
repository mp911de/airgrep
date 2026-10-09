#!/usr/bin/env python3
"""Power loss and temperature estimate for the carrier, from the routed board.

    python3 hardware/sim/run_heat.py            # print the report
    python3 hardware/sim/run_heat.py --write    # also store heat/results.md and heat/*.svg

Operating points: battery only at low battery, and USB operation while charging.

  1. Converter losses from data sheet switch resistances (MAX77757, TPS61088, TPS22975), the
     inductor DCR, and a switching term fitted to the TPS61088 efficiency curve used by
     tools/power_budget.py (specs/power_params.json).
  2. Copper losses: every power track of hardware/carrier-max-charger/carrier.kicad_pcb as a resistor
     network on all four layers, the GND pour as a four-layer sheet solved on a grid, plus the
     contact resistances of USB-C, JST-PH, fuse, shunts and module sockets.
  3. Temperatures: the board as four copper sheets on FR4 (stackup LAYERS / CU_T / DIEL), the
     copper share of every grid cell rasterised from the zone fills, tracks, pads and vias;
     junction and inductor nodes on their pads; surface loss to the air inside the enclosure;
     the enclosure as one thermal resistance to ambient, heated by everything inside it
     (board, modules, battery).

The data sheet θJA figures (JEDEC boards, four layers for the MAX77757) are not used for the
result; they are listed for comparison only. Requires NumPy and SciPy, but no simulator.
"""

import argparse
import json
import math
import os
import re
import sys
import time

try:
    import numpy as np
    from scipy.sparse import coo_matrix, csc_matrix
    from scipy.sparse.linalg import factorized
except ImportError as exc:
    raise SystemExit("The heat model needs NumPy and SciPy. Install hardware/sim/requirements.txt "
                     "in your Python environment; see hardware/sim/README.md.") from exc

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import simlib  # noqa: E402
import run_power  # noqa: E402  (boost constants and switching frequency)
import vent_model  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "heat")
RESULTS = os.path.join(OUT, "results.md")
PCB = os.path.join(HERE, "..", "carrier-max-charger", "carrier.kicad_pcb")
PARAMS = os.path.join(HERE, "..", "..", "specs", "power_params.json")

# --- Materials and stackup (board file, JLC04161H-2116C: four layers, 1.6 mm)
LAYERS = ("F", "In1", "In2", "B")
NL = len(LAYERS)
CU_T = {"F": 35e-6, "In1": 15.2e-6, "In2": 15.2e-6, "B": 35e-6}  # 1 oz outer, 0.5 oz inner
DIEL = (0.2004e-3, 1.065e-3, 0.2004e-3)  # dielectric between adjacent copper layers, top down
VIA_SPAN = tuple(1.6e-3 * d / sum(DIEL) for d in DIEL)  # via barrel share between the layers
FR4_SHARE = [(DIEL[k - 1] if k else 0.0) / 2 + (DIEL[k] if k < NL - 1 else 0.0) / 2 for k in range(NL)]
RHO_CU = 1.72e-8  # Ω·m at 20 °C
ALPHA_CU = 0.00393  # 1/K
K_CU, K_FR4_PLANE, K_FR4_THROUGH = 390.0, 0.8, 0.3  # W/m·K
T_PLATING = 20e-6  # via barrel plating, JLCPCB minimum 18 µm
TC_RDSON = 0.004  # 1/K, MOSFET on-resistance rise, typical for low-voltage silicon (estimate)

# --- MAX77757 (data sheet 19-100xxx, Electrical Characteristics)
CHG_R_IN = 45e-3  # RCHGIN2BYP typ
CHG_R_HS, CHG_R_LS = 60e-3, 60e-3  # RHS, RLS typ
CHG_R_BATFET = 20e-3  # RBAT2SYS typ
CHG_IQ = 2.7e-3  # CHGIN supply current, not switching, typ
CHG_FSW = 1.3e6
CHG_VSYSMIN = 3.5  # 4.1 to 4.5 V termination variants
CHG_TREG = 130.0  # thermal regulation: charge current folds back above this junction temperature
TREG_SLOPE = 50.0  # K/A, junction rise per ampere of charge current, for the regulation loop (estimate)
BULK_CAPS = ("C1", "C5", "C6", "C7", "C9", "C10", "C11", "C13", "C14", "C15", "C18", "C19", "C20", "C24", "C25",
             "C26", "C33", "C34")  # X7R (LCSC C105174, C415535, C3039694, C385906, C907991)
CAP_RATING = 125.0  # °C, X7R upper limit
THERMOSTAT_MIN = 55.0  # LIM-P-82-60: 60 °C ± 5 K
CHG_THETA_JC = 7.5  # K/W
CHG_THETA_JA_JEDEC = 31.0  # K/W, four-layer JESD51-7 board, comparison only
L1_DCR, L1_L = 7.4e-3, 1.0e-6  # FXL0630-1R0-M (docs/power.md)
BUCK_SW_HIGH = 1.3  # high corner: switching term 30 % above the TPS61088 coefficient (no MAX77757 curve)

# --- TPS61088 (SLVSCM8D) and L2 (CENKER CKST0603-2.2uH/M)
BST_R_HS, BST_R_LS = 13e-3, 11e-3  # typ at VCC 6 V
BST_THETA_JC = 3.1 + 1.0  # RθJC(bot) plus the solder layer under the 42 % paste windows (estimate)
BST_THETA_JA_JEDEC = 38.8
CURVE_R16 = 150e3  # R16 the efficiency curve in specs/power_params.json is derated for (about 1 MHz)
INDUCTOR_THETA_BODY = 12.0  # K/W, winding to its two terminals (estimate, metal-alloy body)
CORE_LOSS = 0.02  # W per inductor at about 0.5 A ripple and 1 to 1.3 MHz (estimate, no vendor curve)

# --- TPS22975 (SLVSCI1)
LS_RON = 16e-3
# --- LM73100 ideal diodes U11 (ADSBee), U12 (display), SNOSDC0A: forward drop regulated to VFWD at light
# load, resistive above VFWD / RON; (typical, maximum over -40 to 125 C) per corner
ID_RON = (28.4e-3, 44.85e-3)
ID_VFWD = (16.4e-3, 28.4e-3)
LS_THETA_JC = 16.4

# --- Contacts and passives: (nominal, high). Nominal is an estimate, high is the data sheet maximum
USB_CONTACT = (0.020, 0.040)  # Hanbo MC-107DMS: 40 mΩ max per contact; 4 VBUS + 4 GND contacts
FB_R = (0.015, 0.020)  # FB1 BLM31KN471SZ1L: 20 mΩ max DCR (LCSC C710518); nominal estimated
USB_J1 = "USB_VBUS_J1"  # J1 VBUS pins, D3 and C1, ahead of FB1
JST_CONTACT = (0.005, 0.010)  # JST PH: 10 mΩ max initial per contact
SOCKET_CONTACT = (0.010, 0.020)  # 2.54 mm pin socket or shunt contact (estimate, no data sheet on file)
FUSE_R = (0.011, 0.015)  # F1 Littelfuse 0466005.NRHF 5 A (C57525): 11 mΩ nominal cold; high adds self-heating
PACK_R = (0.060, 0.090)  # FE1160100 pack: at most 50 mΩ AC 1 kHz; DC resistance is higher (estimate)

# --- Thermal environment
GRID_MM, SUB = 1.0, 4
ENCLOSURE_MM = (115.0, 105.0, 40.0)  # outer size assumed: board plus 2 mm walls, stack height 40 mm
WALL_MM, K_WALL = 2.0, 0.2  # PETG / PLA
H_IN, H_OUT = 7.0, 9.0  # W/m²K, natural convection plus radiation (ε 0.9) inside and outside the case
PACK_MM = (100.0, 60.0, 11.0)
ENVIRONMENTS = {
    "open": {"label": "open bench, no enclosure", "h_top": 10.0, "h_bot": 10.0, "enclosed": False},
    "closed": {"label": "closed 3D-printed case", "h_top": 6.0, "h_bot": 6.0, "enclosed": True},
}

# U4 heatsink: a 10 x 10 mm aluminium sink (Fischer ICK S 10 x 10 x 6.5 or ICK BGA 10 x 10 x 6) on the
# FC2QFN top. The package is 0.65 mm tall and the 0805 capacitors around it 1.25 mm, so the sink stands on a
# copper spacer bonded with thermal epoxy on both faces. Junction to package top is not published for the
# FC2QFN; the resistances below are estimates.
HEATSINK = {
    "U4": {
        "label": "10 x 10 mm aluminium sink (Fischer ICK S 10 x 10 x 6.5) on a 1.5 mm copper spacer, thermal epoxy",
        "size_mm": 10.0,
        "r_jt": 35.0,  # K/W, junction to package top (estimate, 25 to 45)
        "r_tim": 20.0,  # K/W, two 0.1 mm epoxy layers (k 1.2 W/m·K) on the 9 mm² top plus the copper spacer
        "r_sink_open": 35.0,  # K/W, sink to still air; Fischer gives about 30 K/W at data sheet rise, derated for 30 to 40 K
    }
}


def heatsink_resistance(hs, env):
    """Junction to the surrounding air through package top, epoxy, spacer and sink; the sink's own resistance
    scales with the environment's convection coefficient (6 W/m²K inside the case against 10 on the bench)."""
    return hs["r_jt"] + hs["r_tim"] + hs["r_sink_open"] * ENVIRONMENTS["open"]["h_top"] / env["h_top"]


def under_heatsink(board, ref, size_mm):
    """References with a pad inside the square footprint of the sink on `ref` (collision check)."""
    pads = pads_of(board, ref)
    cx = sum(p.x for p in pads) / len(pads)
    cy = sum(p.y for p in pads) / len(pads)
    h = size_mm / 2
    return sorted({p.ref for p in board["pads"] if p.ref != ref and abs(p.x - cx) <= h and abs(p.y - cy) <= h})


# --- Operating points
VOUT = run_power.VOUT
CHARGE_A = 1.8  # R6 + R37 = 12.4 kΩ
LED_5V_A = 0.0064  # D5 through R26
LED_SYS_A = 0.0035  # each of the D4 halves through R23 / R31
CASES = [
    {"key": "battyp", "label": "Battery 3.1 V, typical 1.22 W load", "mode": "battery", "vin": 3.1,
     "load": 1.22 / 5.06, "ambients": (25.0, 50.0)},
    {"key": "bat08", "label": "Battery 3.1 V, 0.8 A load", "mode": "battery", "vin": 3.1, "load": 0.8,
     "ambients": (25.0, 50.0)},
    {"key": "bat10", "label": "Battery 3.1 V, 1.0 A load", "mode": "battery", "vin": 3.1, "load": 1.0,
     "ambients": (25.0, 50.0)},
    {"key": "usb37", "label": "USB 3 A source, cell 3.7 V, charging, 0.8 A load", "mode": "usb",
     "vcell": 3.7, "load": 0.8, "limit": 3.0, "ambients": (25.0,)},
    {"key": "usb32", "label": "USB 3 A source, cell 3.2 V, charging, 0.8 A load", "mode": "usb",
     "vcell": 3.2, "load": 0.8, "limit": 3.0, "ambients": (25.0,)},
    {"key": "usb32off", "label": "USB 3 A source, cell 3.2 V, charging, device off", "mode": "usb",
     "vcell": 3.2, "load": 0.0, "limit": 3.0, "ambients": (25.0,)},
]
CORNERS = ("nominal", "high")  # index into the (nominal, high) pairs above
U4_LOSSES = ("MAX77757 buck", "MAX77757 BATT-SYS switch")


# ------------------------------------------------------------------------------------------
# Board file


class Pad:
    __slots__ = ("ref", "num", "x", "y", "w", "h", "layers", "net", "tht", "drill")


def read_board(path=PCB):
    with open(path) as pcb:
        text = pcb.read()
    board = {"segments": [], "vias": [], "pads": [], "zones": [], "polys": []}
    # filled copper polygons with a net (graphic shapes, not zones): drawn copper, taken as is
    for gm in re.finditer(r"\n\t\(gr_poly\n", text):
        block = text[gm.start():text.find("\n\t)\n", gm.start())]
        layer = re.search(r"\(layer \"([^\"]+)\"\)", block)
        net = re.search(r"\(net \"([^\"]+)\"\)", block)
        if not (layer and net and layer.group(1).endswith(".Cu") and "(fill yes)" in block):
            continue
        pts = [(float(a), float(b)) for a, b in re.findall(r"\(xy ([-\d.]+) ([-\d.]+)\)", block)]
        board["polys"].append((net.group(1), layer.group(1).split(".")[0], pts))
    for m in re.finditer(r"\n\t\(segment\n\t\t\(start ([-\d.]+) ([-\d.]+)\)\n\t\t\(end ([-\d.]+) "
                         r"([-\d.]+)\)\n\t\t\(width ([\d.]+)\)\n\t\t\(layer \"([^\"]+)\"\)\n"
                         r"\t\t\(net \"([^\"]*)\"\)", text):
        x1, y1, x2, y2, w = (float(v) for v in m.groups()[:5])
        board["segments"].append((x1, y1, x2, y2, w, m.group(6).split(".")[0], m.group(7)))
    for m in re.finditer(r"\n\t\(via\n\t\t\(at ([-\d.]+) ([-\d.]+)\)\n\t\t\(size ([\d.]+)\)\n"
                         r"\t\t\(drill ([\d.]+)\)\n(?:\t\t.*\n)*?\t\t\(net \"([^\"]*)\"\)", text):
        board["vias"].append((float(m.group(1)), float(m.group(2)), float(m.group(3)),
                              float(m.group(4)), m.group(5)))
    for fm in re.finditer(r"\n\t\(footprint \"[^\"]+\"\n", text):
        block = text[fm.start():text.find("\n\t)\n", fm.start())]
        ref = re.search(r"\(property \"Reference\" \"([^\"]+)\"", block).group(1)
        at = re.search(r"\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)", block)
        fx, fy, rot = float(at.group(1)), float(at.group(2)), math.radians(float(at.group(3) or 0))
        for pm in re.finditer(r"\n\t\t\(pad \"([^\"]*)\" (\w+) (\w+)", block):
            pb = block[pm.start():block.find("\n\t\t)", pm.end())]
            net = re.search(r"\(net \"([^\"]*)\"\)", pb)
            if not net or pm.group(2) == "np_thru_hole":
                continue
            pat = re.search(r"\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)", pb)
            size = re.search(r"\(size ([\d.]+) ([\d.]+)\)", pb)
            px, py, pang = float(pat.group(1)), float(pat.group(2)), float(pat.group(3) or 0)
            pad = Pad()
            pad.ref, pad.num, pad.net = ref, pm.group(1), net.group(1)
            pad.x = fx + px * math.cos(rot) + py * math.sin(rot)
            pad.y = fy - px * math.sin(rot) + py * math.cos(rot)
            w, h = float(size.group(1)), float(size.group(2))
            if round(pang / 90) % 2:
                w, h = h, w
            pad.w, pad.h = w, h
            prim = pb.find("(primitives")
            if prim >= 0:
                # custom pad: the anchor plus its polygons, as one bounding rectangle
                a = math.radians(pang)
                xs, ys = [pad.x - w / 2, pad.x + w / 2], [pad.y - h / 2, pad.y + h / 2]
                for qx, qy in re.findall(r"\(xy ([-\d.]+) ([-\d.]+)\)", pb[prim:]):
                    qx, qy = float(qx), float(qy)
                    xs.append(pad.x + qx * math.cos(a) + qy * math.sin(a))
                    ys.append(pad.y - qx * math.sin(a) + qy * math.cos(a))
                pad.x, pad.y = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
                pad.w, pad.h = max(xs) - min(xs), max(ys) - min(ys)
            pad.tht = pm.group(2) == "thru_hole"
            layers = re.search(r"\(layers ([^)]*)\)", pb).group(1)
            pad.layers = LAYERS if pad.tht or "*.Cu" in layers else (("F",) if "F.Cu" in layers else ("B",))
            drill = re.search(r"\(drill ([\d.]+)", pb)
            pad.drill = float(drill.group(1)) if drill else 0.0
            board["pads"].append(pad)
    for zm in re.finditer(r"\n\t\(zone\n", text):
        block = text[zm.start():text.find("\n\t)\n", zm.start())]
        net = re.search(r"^\t\t\(net \"([^\"]*)\"\)", block, re.M)
        if not net:
            continue
        for fp in re.finditer(r"\(filled_polygon\n\t\t\t\(layer \"([^\"]+)\"\)\n\t\t\t\(pts\n", block):
            end = block.find("\n\t\t\t)", fp.end())
            pts = [(float(a), float(b)) for a, b in
                   re.findall(r"\(xy ([-\d.]+) ([-\d.]+)\)", block[fp.end():end])]
            board["zones"].append((net.group(1), fp.group(1).split(".")[0], pts))
    edges = re.findall(r"\(gr_(?:line|rect)\n\t\t\(start ([-\d.]+) ([-\d.]+)\)\n\t\t\(end ([-\d.]+) "
                       r"([-\d.]+)\)(?:\n\t\t.*)*?\(layer \"Edge.Cuts\"\)", text)
    xs = [float(e[i]) for e in edges for i in (0, 2)]
    ys = [float(e[i]) for e in edges for i in (1, 3)]
    board["outline"] = (min(xs), min(ys), max(xs), max(ys))
    return board


def pads_of(board, ref, nums=None, net=None):
    return [p for p in board["pads"] if p.ref == ref and (nums is None or p.num in nums)
            and (net is None or p.net == net)]


# ------------------------------------------------------------------------------------------
# Grid: rasterised copper and sparse multi-layer solver


class Grid:
    def __init__(self, outline, dx=GRID_MM, sub=SUB):
        self.x0, self.y0 = outline[0], outline[1]
        self.dx, self.sub, self.ds = dx, sub, dx / sub
        self.nx = int(math.ceil((outline[2] - outline[0]) / dx - 1e-9))
        self.ny = int(math.ceil((outline[3] - outline[1]) / dx - 1e-9))
        self.sx, self.sy = self.nx * sub, self.ny * sub
        self.nc = self.nx * self.ny

    def mask(self):
        return bytearray(self.sx * self.sy)

    def _rows(self, ya, yb):
        r0 = max(0, int(math.ceil((ya - self.y0) / self.ds - 0.5)))
        r1 = min(self.sy - 1, int(math.floor((yb - self.y0) / self.ds - 0.5)))
        return r0, r1

    def _cols(self, xa, xb):
        c0 = max(0, int(math.ceil((xa - self.x0) / self.ds - 0.5)))
        c1 = min(self.sx - 1, int(math.floor((xb - self.x0) / self.ds - 0.5)))
        return c0, c1

    def polygon(self, mask, pts):
        crossings = {}
        n = len(pts)
        for i in range(n):
            xa, ya = pts[i]
            xb, yb = pts[(i + 1) % n]
            if ya == yb:
                continue
            r0, r1 = self._rows(min(ya, yb), max(ya, yb))
            for r in range(r0, r1 + 1):
                y = self.y0 + (r + 0.5) * self.ds
                if min(ya, yb) <= y < max(ya, yb):
                    crossings.setdefault(r, []).append(xa + (y - ya) * (xb - xa) / (yb - ya))
        for r, xs in crossings.items():
            xs.sort()
            for a, b in zip(xs[0::2], xs[1::2]):
                c0, c1 = self._cols(a, b)
                if c1 >= c0:
                    mask[r * self.sx + c0:r * self.sx + c1 + 1] = b"\x01" * (c1 - c0 + 1)

    def rect(self, mask, cx, cy, w, h):
        r0, r1 = self._rows(cy - h / 2, cy + h / 2)
        c0, c1 = self._cols(cx - w / 2, cx + w / 2)
        if c1 >= c0:
            for r in range(r0, r1 + 1):
                mask[r * self.sx + c0:r * self.sx + c1 + 1] = b"\x01" * (c1 - c0 + 1)

    def segment_samples(self, x1, y1, x2, y2, w):
        """Sample indices covered by a track of width w."""
        r0, r1 = self._rows(min(y1, y2) - w / 2, max(y1, y2) + w / 2)
        c0, c1 = self._cols(min(x1, x2) - w / 2, max(x1, x2) + w / 2)
        dx, dy = x2 - x1, y2 - y1
        l2 = dx * dx + dy * dy
        out = []
        for r in range(r0, r1 + 1):
            y = self.y0 + (r + 0.5) * self.ds
            for c in range(c0, c1 + 1):
                x = self.x0 + (c + 0.5) * self.ds
                t = 0.0 if l2 == 0 else max(0.0, min(1.0, ((x - x1) * dx + (y - y1) * dy) / l2))
                if math.hypot(x - x1 - t * dx, y - y1 - t * dy) <= w / 2:
                    out.append(r * self.sx + c)
        return out

    def fractions(self, mask):
        s, sx = self.sub, self.sx
        frac = [0.0] * self.nc
        for iy in range(self.ny):
            row = [0] * self.nx
            for k in range(s):
                base = (iy * s + k) * sx
                line = mask[base:base + sx]
                for ix in range(self.nx):
                    row[ix] += sum(line[ix * s:(ix + 1) * s])
            for ix in range(self.nx):
                frac[iy * self.nx + ix] = row[ix] / (s * s)
        return frac

    def cell(self, x, y):
        ix = min(self.nx - 1, max(0, int((x - self.x0) / self.dx)))
        iy = min(self.ny - 1, max(0, int((y - self.y0) / self.dx)))
        return iy * self.nx + ix

    def rect_cells(self, cx, cy, w, h):
        """{cell: area share} of a rectangle, from the sample grid."""
        r0, r1 = self._rows(cy - h / 2, cy + h / 2)
        c0, c1 = self._cols(cx - w / 2, cx + w / 2)
        out = {}
        for r in range(r0, r1 + 1):
            for c in range(c0, c1 + 1):
                cell = (r // self.sub) * self.nx + c // self.sub
                out[cell] = out.get(cell, 0) + 1
        if not out:
            return {self.cell(cx, cy): 1.0}
        total = sum(out.values())
        return {c: n / total for c, n in out.items()}


class LinearSystem:
    """A fixed conductance matrix, factored once and checked on every solve."""

    def __init__(self, matrix):
        self.matrix = csc_matrix(matrix, dtype=float, copy=True)
        if not np.isfinite(self.matrix.data).all():
            raise ValueError("non-finite conductance")
        try:
            self._solve = factorized(self.matrix)
        except RuntimeError as exc:
            raise RuntimeError("singular conductance system: check connections and references") from exc

    def solve(self, b, tol=1e-8):
        rhs = np.asarray(b, dtype=float)
        if rhs.shape != (self.matrix.shape[0],) or not np.isfinite(rhs).all():
            raise ValueError("source vector must match the matrix and contain only finite values")
        if not math.isfinite(tol) or tol <= 0:
            raise ValueError("residual tolerance must be positive and finite")
        x = self._solve(rhs)
        residual = np.linalg.norm(self.matrix @ x - rhs) / (np.linalg.norm(rhs) or 1.0)
        if not np.isfinite(x).all() or not math.isfinite(residual) or residual > tol:
            raise RuntimeError("conductance solve residual %.3g exceeds tolerance %.3g" % (residual, tol))
        return x.tolist()


class Sheet:
    """NL-layer grid network: sheet conductances in x and y per layer, a vertical link per cell
    between adjacent layers, a conductance to a reference per cell, and a few extra nodes.
    Symmetric; factorize after assembling the conductances to solve several source vectors."""

    def __init__(self, grid):
        self.grid = grid
        nc = grid.nc
        self.gx = [[0.0] * nc for _ in range(NL)]  # cell i to i+1, per layer
        self.gy = [[0.0] * nc for _ in range(NL)]  # cell i to i+nx
        self.gz = [[0.0] * nc for _ in range(NL - 1)]  # layer k to k+1, per cell
        self.gref = [[0.0] * nc for _ in range(NL)]
        self.extra = []  # per extra node: (gref, [(layer, cell, g)])

    def set_sheet(self, layer, conductance):
        """Square cells: edge conductance is the harmonic mean of the two sheet values."""
        g, nx = self.grid, self.grid.nx
        for i in range(g.nc):
            a = conductance[i]
            if (i + 1) % nx:
                b = conductance[i + 1]
                self.gx[layer][i] = 0.0 if a <= 0 or b <= 0 else 2 * a * b / (a + b)
            if i + nx < g.nc:
                b = conductance[i + nx]
                self.gy[layer][i] = 0.0 if a <= 0 or b <= 0 else 2 * a * b / (a + b)

    def add_node(self, gref, links):
        self.extra.append((gref, links))
        return NL * self.grid.nc + len(self.extra) - 1

    def factorize(self):
        """Snapshot the conductances; later sheet edits need a new factorization."""
        nc, nx = self.grid.nc, self.grid.nx
        n = NL * nc + len(self.extra)
        diagonal = np.concatenate([np.asarray(g) for g in self.gref]
                                  + [np.asarray([gref for gref, _ in self.extra])])
        rows, cols, values = [np.arange(n)], [np.arange(n)], [diagonal]

        def links(a, b, conductance):
            g = np.asarray(conductance)
            present = g != 0
            a, b, g = a[present], b[present], g[present]
            rows.extend((a, b, a, b))
            cols.extend((a, b, b, a))
            values.extend((g, g, -g, -g))

        cells = np.arange(nc)
        east = cells[(cells + 1) % nx != 0]
        south = cells[cells + nx < nc]
        for layer in range(NL):
            offset = layer * nc
            links(offset + east, offset + east + 1, np.asarray(self.gx[layer])[east])
            links(offset + south, offset + south + nx, np.asarray(self.gy[layer])[south])
            if layer < NL - 1:
                links(offset + cells, offset + cells + nc, self.gz[layer])
        for k, (_, node_links) in enumerate(self.extra):
            cells = np.asarray([layer * nc + cell for layer, cell, _ in node_links], dtype=int)
            links(np.full(len(cells), NL * nc + k), cells, [g for _, _, g in node_links])
        matrix = coo_matrix((np.concatenate(values), (np.concatenate(rows), np.concatenate(cols))),
                            shape=(n, n))
        return LinearSystem(matrix)

    def solve(self, b, tol=1e-8):
        """Solve one source vector using the current conductances."""
        return self.factorize().solve(b, tol)


# ------------------------------------------------------------------------------------------
# Copper model of the board


class Copper:
    def __init__(self, board):
        self.board = board
        self.grid = grid = Grid(board["outline"])
        all_cu = [grid.mask() for _ in LAYERS]
        gnd_cu = [grid.mask() for _ in LAYERS]
        for net, layer, pts in board["zones"] + board["polys"]:
            li = LAYERS.index(layer)
            grid.polygon(all_cu[li], pts)
            if net == "GND":
                grid.polygon(gnd_cu[li], pts)
        for x1, y1, x2, y2, w, layer, net in board["segments"]:
            li = LAYERS.index(layer)
            for s in grid.segment_samples(x1, y1, x2, y2, w):
                all_cu[li][s] = 1
                if net == "GND":
                    gnd_cu[li][s] = 1
        for p in board["pads"]:
            for li, name in enumerate(LAYERS):
                if name in p.layers:
                    grid.rect(all_cu[li], p.x, p.y, p.w, p.h)
                    if p.net == "GND":
                        grid.rect(gnd_cu[li], p.x, p.y, p.w, p.h)
        for x, y, size, drill, net in board["vias"]:
            for li in range(NL):
                grid.rect(all_cu[li], x, y, size * 0.886, size * 0.886)
                if net == "GND":
                    grid.rect(gnd_cu[li], x, y, size * 0.886, size * 0.886)
        self.cu = [grid.fractions(m) for m in all_cu]
        self.gnd = [grid.fractions(m) for m in gnd_cu]
        # vertical links per cell: via barrels and plated through-hole pins, as the conductance of
        # the full 1.6 mm length; the sheets split it over the layer gaps (VIA_SPAN). A via also
        # couples to copper of other nets on the inner layers (antipads are not resolved).
        self.via_g_th = [0.0] * grid.nc
        self.gnd_via_count = [0.0] * grid.nc
        for x, y, size, drill, net in board["vias"]:
            c = grid.cell(x, y)
            self.via_g_th[c] += K_CU * math.pi * drill * 1e-3 * T_PLATING / 1.6e-3
            if net == "GND":
                self.gnd_via_count[c] += 1
        for p in board["pads"]:
            if p.tht:
                c = grid.cell(p.x, p.y)
                # barrel plus a 0.64 mm square brass pin (k 120) through 1.6 mm
                self.via_g_th[c] += K_CU * math.pi * p.drill * 1e-3 * T_PLATING / 1.6e-3 \
                    + 120 * 0.64e-3 ** 2 / 1.6e-3
                if p.net == "GND":
                    self.gnd_via_count[c] += 4  # pin and barrel, about four vias electrically


# ------------------------------------------------------------------------------------------
# Track networks of the power nets


def point_segment(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    l2 = dx * dx + dy * dy
    t = 0.0 if l2 == 0 else max(0.0, min(1.0, ((px - x1) * dx + (py - y1) * dy) / l2))
    return math.hypot(px - x1 - t * dx, py - y1 - t * dy), t


AREA_RES = 0.25  # mm, cell size of the copper areas (zone fills and filled polygons) of a power net


def area_cells(polygons, res=AREA_RES):
    """Cells (ix, iy) of size res whose centres lie inside any of the polygons (even-odd rule per
    polygon, union over polygons)."""
    cells = set()
    for pts in polygons:
        n = len(pts)
        ys = [p[1] for p in pts]
        for iy in range(int(math.floor(min(ys) / res)), int(math.ceil(max(ys) / res)) + 1):
            y = (iy + 0.5) * res
            xs = []
            for i in range(n):
                xa, ya = pts[i]
                xb, yb = pts[(i + 1) % n]
                if (ya <= y < yb) or (yb <= y < ya):
                    xs.append(xa + (y - ya) * (xb - xa) / (yb - ya))
            xs.sort()
            for a, b in zip(xs[0::2], xs[1::2]):
                for ix in range(int(math.ceil(a / res - 0.5)), int(math.floor(b / res - 0.5)) + 1):
                    cells.add((ix, iy))
    return cells


class TrackNet:
    """Resistor network of one net: tracks, vias, pads as equipotential nodes, and the net's copper
    areas (zone fills and filled polygons) as a grid of AREA_RES cells."""

    def __init__(self, board, grid, net):
        self.net, self.grid = net, grid
        segs = [s for s in board["segments"] if s[6] == net]
        self.pads = [p for p in board["pads"] if p.net == net]
        vias = [v for v in board["vias"] if v[4] == net]

        links = []

        def attach(x, y, layer, w=0.0):
            """Node of a track end: a pad its copper overlaps, a via, or a free point."""
            touching = []
            for p in self.pads:
                if layer in p.layers:
                    dx = max(abs(x - p.x) - p.w / 2, 0.0)
                    dy = max(abs(y - p.y) - p.h / 2, 0.0)
                    d = math.hypot(dx, dy)
                    if d <= w / 2 + 1e-3:
                        touching.append((d, ("pad", p.ref, p.num)))
            for i, v in enumerate(vias):
                d = math.hypot(x - v[0], y - v[1])
                if d <= v[2] / 2 + 1e-3:
                    touching.append((d, ("via", i, layer)))
            if touching:
                touching.sort()
                for _, other in touching[1:]:
                    links.append((touching[0][1], other))
                return touching[0][1]
            return ("pt", layer, round(x, 3), round(y, 3))

        ends = [(attach(s[0], s[1], s[5], s[4]), attach(s[2], s[3], s[5], s[4])) for s in segs]
        # a track end inside the copper of another track's end region (overlapping, not coincident,
        # ends) joins that end; ends on the interior are handled as T-junctions below
        for k, s in enumerate(segs):
            for end, (px, py) in zip(ends[k], ((s[0], s[1]), (s[2], s[3]))):
                for j, o in enumerate(segs):
                    if j == k or o[5] != s[5]:
                        continue
                    d, t = point_segment(px, py, *o[:4])
                    if d <= o[4] / 2 + 1e-3 and not 1e-3 < t < 1 - 1e-3:
                        other = ends[j][0 if t < 0.5 else 1]
                        if other != end:
                            links.append((end, other))
        splits = [[] for _ in segs]
        # T-junctions: an end, via or pad lying on the interior of another track
        probes = [(s[0], s[1], s[5], e[0], 0.0) for s, e in zip(segs, ends)] \
            + [(s[2], s[3], s[5], e[1], 0.0) for s, e in zip(segs, ends)] \
            + [(v[0], v[1], layer, ("via", i, layer), 0.0) for i, v in enumerate(vias) for layer in LAYERS] \
            + [(p.x, p.y, layer, ("pad", p.ref, p.num), min(p.w, p.h) / 2) for p in self.pads
               for layer in p.layers]
        for k, s in enumerate(segs):
            for px, py, layer, key, reach in probes:
                if layer != s[5] or key in ends[k]:
                    continue
                d, t = point_segment(px, py, *s[:4])
                if d <= s[4] / 2 + reach and 1e-3 < t < 1 - 1e-3:
                    splits[k].append((t, key))
        self.elements = []
        for k, s in enumerate(segs):
            x1, y1, x2, y2, w, layer, _ = s
            pts = [(0.0, ends[k][0])] + sorted(set(splits[k])) + [(1.0, ends[k][1])]
            for (ta, ka), (tb, kb) in zip(pts, pts[1:]):
                if ka == kb:
                    continue
                ax, ay = x1 + ta * (x2 - x1), y1 + ta * (y2 - y1)
                bx, by = x1 + tb * (x2 - x1), y1 + tb * (y2 - y1)
                length = math.hypot(bx - ax, by - ay) * 1e-3
                cells = self._cells(ax, ay, bx, by)
                self.elements.append({"a": ka, "b": kb, "r0": RHO_CU * length / (w * 1e-3 * CU_T[layer]),
                                      "layers": (LAYERS.index(layer),), "cells": cells, "w": w, "len": length})
        for i, v in enumerate(vias):
            for k in range(NL - 1):
                r_via = RHO_CU * VIA_SPAN[k] / (math.pi * v[3] * 1e-3 * T_PLATING)
                self.elements.append({"a": ("via", i, LAYERS[k]), "b": ("via", i, LAYERS[k + 1]), "r0": r_via,
                                      "layers": (k, k + 1), "cells": {grid.cell(v[0], v[1]): 1.0}})
        # a via inside a pad joins it on that layer
        for i, v in enumerate(vias):
            for p in self.pads:
                if abs(v[0] - p.x) <= p.w / 2 and abs(v[1] - p.y) <= p.h / 2:
                    for layer in p.layers:
                        self.elements.append({"a": ("via", i, layer), "b": ("pad", p.ref, p.num), "r0": 0.0,
                                              "layers": (LAYERS.index(layer),), "cells": {}})
        for a, b in set(links):
            self.elements.append({"a": a, "b": b, "r0": 0.0, "layers": (0,), "cells": {}})
        # overlapping pads of the net (USB-C A/B rows share lands) are one node
        for i, p in enumerate(self.pads):
            for q in self.pads[i + 1:]:
                if set(p.layers) & set(q.layers) and abs(p.x - q.x) < (p.w + q.w) / 2 \
                        and abs(p.y - q.y) < (p.h + q.h) / 2:
                    self.elements.append({"a": ("pad", p.ref, p.num), "b": ("pad", q.ref, q.num), "r0": 0.0,
                                          "layers": (0,), "cells": {}})
        # through-hole pads join all layers
        for p in self.pads:
            if p.tht:
                self.elements.append({"a": ("pad", p.ref, p.num), "b": ("pad", p.ref, p.num), "r0": 0.0,
                                      "layers": (0,), "cells": {}})
        # copper areas: zone fills and filled polygons of the net, per layer one cell grid; cells
        # join their four neighbours (one square of sheet resistance), pads, vias and track ends
        res = AREA_RES
        for layer in LAYERS:
            polygons = [pts for n_, l_, pts in board["zones"] + board["polys"] if n_ == net and l_ == layer]
            if not polygons:
                continue
            cells = area_cells(polygons)
            r_sq = RHO_CU / CU_T[layer]
            for ix, iy in cells:
                for nb in ((ix + 1, iy), (ix, iy + 1)):
                    if nb in cells:
                        self.elements.append({"a": ("cell", layer, ix, iy), "b": ("cell", layer) + nb,
                                              "r0": r_sq, "layers": (LAYERS.index(layer),),
                                              "cells": {grid.cell((ix + 0.5) * res, (iy + 0.5) * res): 1.0}})

            def cell_of(x, y):
                c = (int(math.floor(x / res)), int(math.floor(y / res)))
                return ("cell", layer) + c if c in cells else None

            for p in self.pads:
                if layer not in p.layers:
                    continue
                inside = [("cell", layer, ix, iy) for ix, iy in cells
                          if abs((ix + 0.5) * res - p.x) <= p.w / 2 and abs((iy + 0.5) * res - p.y) <= p.h / 2]
                centre = cell_of(p.x, p.y)
                for key in inside or ([centre] if centre else []):
                    self.elements.append({"a": ("pad", p.ref, p.num), "b": key, "r0": 0.0, "layers": (LAYERS.index(layer),),
                                          "cells": {}})
            for i, v in enumerate(vias):
                key = cell_of(v[0], v[1])
                if key:
                    self.elements.append({"a": ("via", i, layer), "b": key, "r0": 0.0, "layers": (LAYERS.index(layer),), "cells": {}})
            for s, e in zip(segs, ends):
                if s[5] != layer:
                    continue
                for (x, y), end in zip(((s[0], s[1]), (s[2], s[3])), e):
                    key = cell_of(x, y)
                    if key:
                        self.elements.append({"a": end, "b": key, "r0": 0.0, "layers": (LAYERS.index(layer),), "cells": {}})
        self.nodes = sorted({e["a"] for e in self.elements} | {e["b"] for e in self.elements}
                            | {("pad", p.ref, p.num) for p in self.pads}, key=str)
        self.index = {k: i for i, k in enumerate(self.nodes)}

    def _cells(self, ax, ay, bx, by):
        n = max(1, int(math.hypot(bx - ax, by - ay) / (self.grid.dx / 4)))
        out = {}
        for i in range(n):
            t = (i + 0.5) / n
            c = self.grid.cell(ax + t * (bx - ax), ay + t * (by - ay))
            out[c] = out.get(c, 0) + 1.0 / n
        return out

    def solve(self, injections, temps=None):
        """injections: {pad key or (ref, [nums]): current into the net}. Returns node voltages
        (relative), per-element loss, and the cell-wise loss map."""
        n = len(self.nodes)
        inj = [0.0] * n
        for (ref, nums), current in injections.items():
            keys = [("pad", p.ref, p.num) for p in self.pads if p.ref == ref and (nums is None or p.num in nums)]
            if not keys:
                sys.exit("%s: no pad %s %s on the net" % (self.net, ref, nums))
            for k in keys:
                inj[self.index[k]] += current / len(keys)
        if not all(math.isfinite(i) for i in inj):
            raise ValueError("%s: currents must be finite" % self.net)
        if abs(sum(inj)) > 1e-10 * max(1.0, sum(abs(i) for i in inj)):
            raise ValueError("%s: source and sink currents must balance" % self.net)
        if not any(inj):
            return dict.fromkeys(self.nodes, 0.0), 0.0, {}
        # connectivity: drop islands without injections
        adj = [[] for _ in range(n)]
        for e in self.elements:
            a, b = self.index[e["a"]], self.index[e["b"]]
            adj[a].append(b)
            adj[b].append(a)
        comp = [-1] * n
        for start in range(n):
            if comp[start] >= 0:
                continue
            stack = [start]
            comp[start] = start
            while stack:
                u = stack.pop()
                for w in adj[u]:
                    if comp[w] < 0:
                        comp[w] = start
                        stack.append(w)
        active = {comp[i] for i in range(n) if abs(inj[i]) > 0}
        if len(active) > 1:
            sys.exit("%s: terminals are not connected by copper" % self.net)
        live = [i for i in range(n) if comp[i] in active]
        # zero-resistance joints merge their nodes (union-find), the rest is the conductance matrix
        parent = {i: i for i in live}

        def find(i):
            while parent[i] != i:
                parent[i] = parent[parent[i]]
                i = parent[i]
            return i

        for e in self.elements:
            a, b = self.index[e["a"]], self.index[e["b"]]
            if e["r0"] == 0.0 and a in parent and a != b:
                ra, rb = find(a), find(b)
                if ra != rb:
                    parent[ra] = rb
        reps = sorted({find(i) for i in live})
        pos = {r: k for k, r in enumerate(reps)}
        m = len(reps)
        rows = [dict() for _ in range(m)]
        rvals = []
        for e in self.elements:
            r = e["r0"]
            if temps is not None and e["cells"]:
                tc = sum(temps[layer * self.grid.nc + c] * f
                         for layer in e["layers"] for c, f in e["cells"].items()) / len(e["layers"])
                r *= 1 + ALPHA_CU * (tc - 20)
            rvals.append(r)
            a, b = self.index[e["a"]], self.index[e["b"]]
            if a == b or a not in parent or r == 0.0:
                continue
            i, j = pos[find(a)], pos[find(b)]
            if i == j:
                continue
            gg = 1.0 / r
            rows[i][i] = rows[i].get(i, 0.0) + gg
            rows[j][j] = rows[j].get(j, 0.0) + gg
            rows[i][j] = rows[i].get(j, 0.0) - gg
            rows[j][i] = rows[j].get(i, 0.0) - gg
        rhs = [0.0] * m
        for i in live:
            rhs[pos[find(i)]] += inj[i]
        ground = min(range(m), key=lambda k: rhs[k])
        v = solve_grounded(rows, rhs, ground)
        volts = {self.nodes[i]: v[pos[find(i)]] for i in live}
        loss_map, total = {}, 0.0
        for e, r in zip(self.elements, rvals):
            if e["a"] == e["b"] or e["a"] not in volts or r == 0:
                continue
            p = (volts[e["a"]] - volts[e["b"]]) ** 2 / r
            total += p
            for layer in e["layers"]:
                for c, f in e["cells"].items():
                    key = (layer, c)
                    loss_map[key] = loss_map.get(key, 0.0) + p * f / len(e["layers"])
        return volts, total, loss_map

    def resistance(self, src, dst, temps=None):
        volts, _, _ = self.solve({src: 1.0, dst: -1.0}, temps)
        def mean(ref, nums):
            vals = [volts[("pad", p.ref, p.num)] for p in self.pads
                    if p.ref == ref and (nums is None or p.num in nums)]
            return sum(vals) / len(vals)
        return mean(*src) - mean(*dst)


def solve_grounded(rows, rhs, ground, tol=1e-11):
    """Solve a resistor network with one reference node held at zero volts."""
    n = len(rows)
    a, b, conductance = [], [], []
    for i, row in enumerate(rows):
        for j, value in row.items():
            a.append(i)
            b.append(j)
            conductance.append(value)
    matrix = coo_matrix((conductance, (a, b)), shape=(n, n)).tocsc()
    keep = np.arange(n) != ground
    result = np.zeros(n)
    if keep.any():
        system = LinearSystem(matrix[keep][:, keep])
        result[keep] = system.solve(np.asarray(rhs)[keep], tol)
    return result.tolist()


# ------------------------------------------------------------------------------------------
# Converter loss models


def boost_conduction(vin, iin, tj, tl):
    """Conduction losses of the TPS61088 switches and L2 DCR at input current iin."""
    r_hs = BST_R_HS * (1 + TC_RDSON * (tj - 25))
    r_ls = BST_R_LS * (1 + TC_RDSON * (tj - 25))
    dcr = run_power.L2_DCR * (1 + ALPHA_CU * (tl - 25))
    d = max(0.0, 1 - (vin - iin * (dcr + r_ls)) / VOUT)
    fsw = run_power.fsw_boost(vin)
    ripple = vin * d / (run_power.L2 * fsw)
    i2 = iin ** 2 + ripple ** 2 / 12
    return {"d": d, "fsw": fsw, "ripple": ripple, "ls": d * i2 * r_ls, "hs": (1 - d) * i2 * r_hs,
            "dcr": i2 * dcr}


def fit_boost(curve):
    """Fit P_sw = K * VOUT * IIN * fSW + P0 to the residual loss of the efficiency curve at 3.6 V.
    The curve is derated for R16 = CURVE_R16 (about 1 MHz), so the fit uses that frequency."""
    rows = []
    for iout, eta in curve:
        if not 0.2 <= iout <= 2.0:
            continue
        vin = 3.6
        iin = VOUT * iout / (eta * vin)
        c = boost_conduction(vin, iin, 25 + 30 * iout, 25 + 20 * iout)
        resid = VOUT * iout * (1 / eta - 1) - c["ls"] - c["hs"] - c["dcr"] - CORE_LOSS
        fsw = 1 / (CURVE_R16 * run_power.C_FREQ / 4 + run_power.T_DELAY * VOUT / vin)
        rows.append((VOUT * iin * fsw, resid, iout, eta))
    n = len(rows)
    mx = sum(r[0] for r in rows) / n
    my = sum(r[1] for r in rows) / n
    k = sum((r[0] - mx) * (r[1] - my) for r in rows) / sum((r[0] - mx) ** 2 for r in rows)
    p0 = my - k * mx
    return max(k, 0.0), max(p0, 0.0), rows


def boost_losses(vin, iout, tj, tl, fit):
    if not iout:
        c = boost_conduction(vin, 0.0, tj, tl)
        return {"iin": 0.0, "ic": 0.0, "l2": 0.0, "d": c["d"], "ripple": c["ripple"], "eta": 0.0}
    k, p0 = fit
    iin = VOUT * iout / (0.92 * vin)
    for _ in range(30):
        c = boost_conduction(vin, iin, tj, tl)
        ic = c["ls"] + c["hs"] + k * VOUT * iin * c["fsw"] + p0
        l2 = c["dcr"] + CORE_LOSS
        iin = (VOUT * iout + ic + l2) / vin
    return {"iin": iin, "ic": ic, "l2": l2, "d": c["d"], "ripple": c["ripple"],
            "eta": VOUT * iout / (vin * iin)}


def buck_losses(vbus, vsys, isys, tj, tl, k_sw):
    """MAX77757 buck from CHGIN to SYS, delivering isys out of L1."""
    rs = 1 + TC_RDSON * (tj - 25)
    r_in, r_hs, r_ls = CHG_R_IN * rs, CHG_R_HS * rs, CHG_R_LS * rs
    dcr = L1_DCR * (1 + ALPHA_CU * (tl - 25))
    iin = vsys * isys / (0.9 * vbus)
    for _ in range(30):
        vbyp = vbus - iin * r_in
        d = min(1.0, (vsys + isys * (dcr + r_ls)) / (vbyp + isys * (r_ls - r_hs)))
        ripple = (vbyp - vsys) * d / (L1_L * CHG_FSW)
        i2 = isys ** 2 + ripple ** 2 / 12
        cond = i2 * (d * r_hs + (1 - d) * r_ls)
        p_sw = k_sw * vbyp * isys * CHG_FSW
        p_in = iin ** 2 * r_in
        p_q = vbus * CHG_IQ
        l1 = i2 * dcr + CORE_LOSS
        iin = (vsys * isys + cond + p_sw + p_in + p_q + l1) / vbus
    return {"iin": iin, "ic": cond + p_sw + p_in + p_q, "l1": l1, "d": d, "ripple": ripple,
            "parts": {"switches": cond, "switching": p_sw, "input switch": p_in, "quiescent": p_q}}


# ------------------------------------------------------------------------------------------
# Thermal and electrical solves on the grid


class Model:
    def __init__(self, board, copper, fit, params, corner, heatsink=None):
        self.board, self.copper, self.grid, self.fit = board, copper, copper.grid, fit
        self.corner = corner  # 0 nominal, 1 high
        self.heatsink = heatsink or {}  # {ref: HEATSINK entry}
        self.nets = {n: TrackNet(board, self.grid, n) for n in
                     ("USB_VBUS", USB_J1, "CHG_SW", "VSYS", "VBAT", "BAT_IN", "BOOST_SW", "BOOST_5V", "RAIL_5V",
                      "ESP_5V_D", "ADS_5V_D", "ESP_5V", "ADS_5V")}
        ads = next(l for l in params["loads_5v"] if l["name"].startswith("ADSBee"))
        gnss = params["gnss"]
        ads_ma = ads["wifi_on_ma"] + gnss["module_tracking_ma"] + gnss["breakout_led_ma"] + gnss["active_antenna_ma"]
        esp_ma = sum(l["ma"] for l in params["loads_5v"] if l["name"].startswith("Display"))
        self.ads_share = ads_ma / (ads_ma + esp_ma)

    # -- electrical: GND pour
    def gnd_sheet(self, temps):
        g = self.grid
        sheet = Sheet(g)
        for li, name in enumerate(LAYERS):
            cond = [5.8e7 / (1 + ALPHA_CU * ((temps[li * g.nc + c] if temps else 25) - 20)) * CU_T[name] * f
                    for c, f in enumerate(self.copper.gnd[li])]
            sheet.set_sheet(li, cond)
            sheet.gref[li] = [1e-3] * g.nc  # regularisation, far below the copper conductances
        for k in range(NL - 1):
            r_via = RHO_CU * VIA_SPAN[k] / (math.pi * 0.3048e-3 * T_PLATING)
            sheet.gz[k] = [n / r_via for n in self.copper.gnd_via_count]
        return sheet

    def gnd_solve(self, injections, temps):
        """injections: list of (pads, current into GND); the last entry balances the others and is
        also the reference. Returns node voltages, loss and loss map."""
        g = self.grid
        sheet = self.gnd_sheet(temps)
        for p in injections[-1][0]:
            for c in g.rect_cells(p.x, p.y, p.w, p.h):
                sheet.gref[0][c] = 1e4
        b = [0.0] * (NL * g.nc)
        for pads, current in injections:
            area = sum(p.w * p.h for p in pads)
            for p in pads:
                for c, f in g.rect_cells(p.x, p.y, p.w, p.h).items():
                    b[c] += current * p.w * p.h / area * f
        v = sheet.solve(b, tol=1e-7)
        nc, nx = g.nc, g.nx
        loss_map, total = {}, 0.0
        for li in range(NL):
            off = li * nc
            for i in range(nc):
                for j, gg in ((i + 1, sheet.gx[li][i]), (i + nx, sheet.gy[li][i])):
                    if gg and j < nc:
                        p = gg * (v[off + i] - v[off + j]) ** 2
                        total += p
                        for c in (i, j):
                            loss_map[(li, c)] = loss_map.get((li, c), 0.0) + p / 2
        for k in range(NL - 1):
            for i in range(nc):
                if sheet.gz[k][i]:
                    p = sheet.gz[k][i] * (v[k * nc + i] - v[(k + 1) * nc + i]) ** 2
                    total += p
                    for layer in (k, k + 1):
                        loss_map[(layer, i)] = loss_map.get((layer, i), 0.0) + p / 2
        return v, total, loss_map

    # -- thermal sheet
    def thermal_sheet(self, env):
        g = self.grid
        sheet = Sheet(g)
        area = (g.dx * 1e-3) ** 2
        for li, name in enumerate(LAYERS):
            cond = [K_CU * CU_T[name] * f + K_FR4_PLANE * FR4_SHARE[li] for f in self.copper.cu[li]]
            sheet.set_sheet(li, cond)
        for k in range(NL - 1):
            scale = 1.6e-3 / VIA_SPAN[k]
            sheet.gz[k] = [K_FR4_THROUGH * area / DIEL[k] + gv * scale for gv in self.copper.via_g_th]
        sheet.gref[0] = [env["h_top"] * area] * g.nc
        sheet.gref[NL - 1] = [env["h_bot"] * area] * g.nc
        # junction and inductor body nodes
        self.nodes = {}
        for ref, theta in (("U4", CHG_THETA_JC), ("U6", BST_THETA_JC), ("U7", LS_THETA_JC),
                           ("L1", INDUCTOR_THETA_BODY), ("L2", INDUCTOR_THETA_BODY)):
            pads = [p for p in pads_of(self.board, ref) if p.net or not ref.startswith("U")]
            total = sum(p.w * p.h for p in pads)
            links = {}
            for p in pads:
                for c, f in g.rect_cells(p.x, p.y, p.w, p.h).items():
                    links[c] = links.get(c, 0.0) + p.w * p.h / total * f / theta
            # junction to the surrounding air directly: only through a heatsink
            gref = 1.0 / heatsink_resistance(self.heatsink[ref], env) if ref in self.heatsink else 0.0
            self.nodes[ref] = sheet.add_node(gref, [(0, c, gg) for c, gg in links.items()])
        return sheet


def pad_heat(heat, grid, pads, power):
    area = sum(p.w * p.h for p in pads)
    for p in pads:
        for c, f in grid.rect_cells(p.x, p.y, p.w, p.h).items():
            heat[(0, c)] = heat.get((0, c), 0.0) + power * p.w * p.h / area * f


def enclosure_resistance():
    a, b, c = (v * 1e-3 for v in ENCLOSURE_MM)
    area = 2 * (a * b + a * c + b * c)
    return (1 / H_IN + WALL_MM * 1e-3 / K_WALL + 1 / H_OUT) / area, area


def pack_resistance():
    a, b, c = (v * 1e-3 for v in PACK_MM)
    return 1 / (H_IN * 2 * (a * b + a * c + b * c))


def solve_paths(model, board):
    """Copper path resistances at 20 °C that run_case uses for its voltage bookkeeping:
    (r_net, r_gnd_bat, r_gnd_usb). Assign them to every Model before run_case."""
    r_net = {
        "USB_VBUS J1": model.nets[USB_J1].resistance(("J1", ("A4", "A9", "B4", "B9")), ("FB1", ("1",))),
        "USB_VBUS": model.nets["USB_VBUS"].resistance(("FB1", ("2",)), ("U4", ("23", "24"))),
        "CHG_SW": model.nets["CHG_SW"].resistance(("U4", ("20", "21")), ("L1", ("1",))),
        "VSYS": model.nets["VSYS"].resistance(("U4", ("15", "16")), ("L2", ("1",))),
        "VSYS L1": model.nets["VSYS"].resistance(("L1", ("2",)), ("U4", ("15", "16"))),
        "VBAT": model.nets["VBAT"].resistance(("F1", ("2",)), ("U4", ("13", "14"))),
        "BAT_IN": model.nets["BAT_IN"].resistance(("J3", ("1",)), ("F1", ("1",))),
        "BOOST_SW": model.nets["BOOST_SW"].resistance(("L2", ("2",)), ("U6", ("4", "5", "6", "7"))),
        "BOOST_5V": model.nets["BOOST_5V"].resistance(("U6", ("14", "15", "16")), ("U7", ("1", "2", "4"))),
        "RAIL_5V": model.nets["RAIL_5V"].resistance(("U7", ("7", "8")), ("U11", ("5",))),
        "ESP_5V_D": model.nets["ESP_5V_D"].resistance(("U12", ("6",)), ("JP1", ("1",))),
        "ADS_5V_D": model.nets["ADS_5V_D"].resistance(("U11", ("6",)), ("JP2", ("1",))),
        "ADS_5V": model.nets["ADS_5V"].resistance(("JP2", ("2",)), ("J10", ("1",))),
        "ESP_5V": model.nets["ESP_5V"].resistance(("JP1", ("2",)), ("J7", ("1",))),
    }
    # GND at 1 A: the loss in W equals the resistance in Ω
    _, r_gnd_bat, _ = model.gnd_solve([(pads_of(board, "U6", net="GND"), 1.0),
                                       (pads_of(board, "J3", ("2",)), -1.0)], None)
    _, r_gnd_usb, _ = model.gnd_solve([(pads_of(board, "U4", ("18", "19")), 1.0),
                                       (pads_of(board, "J1", ("A1", "A12", "B1", "B12")), -1.0)], None)
    return r_net, r_gnd_bat, r_gnd_usb


def run_case(model, case, env_key, ambient, k_buck, max_iterations=16,
             vent_area_cm2=vent_model.VENT_AREA_CM2, charge_current=CHARGE_A,
             regulation_temperature=CHG_TREG):
    """Solve an independent operating point; reject an unconverged thermal iteration."""
    if max_iterations < 1:
        raise ValueError("thermal iteration limit must be positive")
    env = ENVIRONMENTS[env_key]
    g, board, nets = model.grid, model.board, model.nets
    corner = model.corner
    thermal = model.thermal_sheet(env).factorize()
    nc = g.nc
    temps = None  # board cell temperatures, all layers
    tj = {"U4": ambient + 30, "U6": ambient + 30, "U7": ambient + 10, "L1": ambient + 20, "L2": ambient + 20}
    r_enc, _ = enclosure_resistance()
    vsys_drop = 0.0
    load = case["load"]
    i_ads, i_esp = load * model.ads_share, load * (1 - model.ads_share)
    chg_cap = charge_current  # charge current allowed by the junction thermal regulation
    regulated = False
    for outer in range(max_iterations):
        losses, heat = {}, {}

        def track(net, inj):
            _, p, lmap = nets[net].solve(inj, temps)
            losses["copper " + net] = losses.get("copper " + net, 0.0) + p
            for k, v in lmap.items():
                heat[k] = heat.get(k, 0.0) + v

        def contact(label, pads, current, r):
            p = current ** 2 * r
            losses[label] = losses.get(label, 0.0) + p
            pad_heat(heat, g, pads, p)

        r_pack = PACK_R[corner]
        # path resistances for the voltage bookkeeping (25 °C copper is close enough here)
        r_batpath = (2 * JST_CONTACT[corner] + FUSE_R[corner]
                     + model.r_net["BAT_IN"] + model.r_net["VBAT"] + model.r_gnd_bat)
        rs = 1 + TC_RDSON * (tj["U4"] - 25)
        if case["mode"] == "battery":
            vin = case["vin"]
            b = boost_losses(vin, load, tj["U6"], tj["L2"], model.fit)
            i_b = i_bat = b["iin"]
            p_batfet = i_bat ** 2 * CHG_R_BATFET * rs
            vcell = vin + i_bat * (r_pack + r_batpath + CHG_R_BATFET * rs + model.r_net["VSYS"])
            op = {"VSYS at L2": vin, "cell open-circuit voltage": vcell, "battery current": i_bat,
                  "boost input current": b["iin"], "boost duty": b["d"], "boost efficiency": b["eta"]}
        else:
            vcell = case["vcell"]
            i_chg = chg_cap
            for _ in range(200):
                vbatt = vcell + i_chg * (r_pack + r_batpath)
                if vbatt < CHG_VSYSMIN and i_chg > 0:
                    vsys = CHG_VSYSMIN
                    p_batfet = (vsys - vbatt) * i_chg
                else:
                    vsys = vbatt + i_chg * CHG_R_BATFET * rs
                    p_batfet = i_chg ** 2 * CHG_R_BATFET * rs
                vin = vsys - vsys_drop
                b = boost_losses(vin, load, tj["U6"], tj["L2"], model.fit)
                i_l1 = i_chg + b["iin"] + 2 * LED_SYS_A
                r_usb = (2 * USB_CONTACT[corner] / 4 + model.r_net["USB_VBUS J1"] + FB_R[corner]
                         + model.r_net["USB_VBUS"] + model.r_gnd_usb)
                i_in = 2.5
                for _ in range(20):
                    vbus = 5.0 - i_in * r_usb
                    buck = buck_losses(vbus, vsys, i_l1, tj["U4"], tj["L1"], k_buck)
                    i_in = buck["iin"]
                if i_in <= case["limit"] + 1e-6:
                    break
                i_chg -= 0.02
            else:
                raise RuntimeError("%s: USB current-limit iteration did not converge" % case["key"])
            i_b = abs(i_chg)
            op = {"charge current allowed by thermal regulation": chg_cap,
                  "VBUS at the connector": 5.0, "CHGIN at U4": vbus, "VSYS": vsys, "BATT at U4": vbatt,
                  "input current": i_in, "charge current": i_chg, "buck output current": i_l1,
                  "buck duty": buck["d"], "boost input current": b["iin"],
                  "BATFET mode": ("linear (VSYS held at VSYSMIN)" if vbatt < CHG_VSYSMIN and i_chg > 0
                                  else "closed, battery supplements the load" if i_chg < 0 else "closed"),
                  "charger efficiency": vsys * i_l1 / (vbus * i_in)}
            if load:
                op["boost efficiency"] = b["eta"]
            losses["MAX77757 buck"] = buck["ic"]
            losses["L1"] = buck["l1"]
        losses["MAX77757 BATT-SYS switch"] = p_batfet
        if load:
            losses["TPS61088"] = b["ic"]
            losses["L2"] = b["l2"]
            losses["TPS22975"] = load ** 2 * LS_RON * (1 + TC_RDSON * (tj["U7"] - 25))
            # ideal diodes: regulated drop at light load, resistive above VFWD / RON
            for ref, i in (("U11", i_ads), ("U12", i_esp)):
                p = max(i * ID_VFWD[corner], i * i * ID_RON[corner])
                losses["LM73100 ideal diodes"] = losses.get("LM73100 ideal diodes", 0.0) + p
                pad_heat(heat, g, pads_of(board, ref, ("5", "6")), p)

        # copper and contacts
        if case["mode"] == "battery":
            track("VSYS", {("U4", ("15", "16")): i_bat, ("L2", ("1",)): -i_bat})
        else:
            track(USB_J1, {("J1", ("A4", "A9", "B4", "B9")): i_in, ("FB1", ("1",)): -i_in})
            contact("FB1 ferrite bead", pads_of(board, "FB1"), i_in, FB_R[corner])
            track("USB_VBUS", {("FB1", ("2",)): i_in, ("U4", ("23", "24")): -i_in})
            track("CHG_SW", {("U4", ("20", "21")): i_l1, ("L1", ("1",)): -i_l1})
            inj = {("L1", ("2",)): i_l1, ("U4", ("15", "16")): -i_chg}
            if b["iin"]:
                inj[("L2", ("1",))] = -(i_l1 - i_chg)
            else:
                inj[("U4", ("15", "16"))] = -i_l1
            track("VSYS", inj)
            contact("USB-C contacts", pads_of(board, "J1", ("A4", "A9", "B4", "B9", "A1", "A12", "B1", "B12")),
                    i_in, 2 * USB_CONTACT[corner] / 4)
        track("VBAT", {("F1", ("2",)): i_b, ("U4", ("13", "14")): -i_b})
        track("BAT_IN", {("J3", ("1",)): i_b, ("F1", ("1",)): -i_b})
        contact("F1 fuse", pads_of(board, "F1"), i_b, FUSE_R[corner])
        contact("JST-PH battery contacts", pads_of(board, "J3"), i_b, 2 * JST_CONTACT[corner])
        if load:
            track("BOOST_SW", {("L2", ("2",)): b["iin"], ("U6", ("4", "5", "6", "7")): -b["iin"]})
            track("BOOST_5V", {("U6", ("14", "15", "16")): load, ("U7", ("1", "2", "4")): -load})
            track("RAIL_5V", {("U7", ("7", "8")): load + LED_5V_A, ("U12", ("5",)): -i_esp,
                              ("U11", ("5",)): -i_ads, ("R26", None): -LED_5V_A})
            track("ESP_5V_D", {("U12", ("6",)): i_esp, ("JP1", ("1",)): -i_esp})
            track("ADS_5V_D", {("U11", ("6",)): i_ads, ("JP2", ("1",)): -i_ads})
            track("ESP_5V", {("JP1", ("2",)): i_esp, ("J7", ("1",)): -i_esp})
            track("ADS_5V", {("JP2", ("2",)): i_ads, ("J10", ("1",)): -i_ads})
            r_sock = SOCKET_CONTACT[corner]
            contact("shunts and module sockets", pads_of(board, "JP1"), i_esp, 2 * r_sock)
            contact("shunts and module sockets", pads_of(board, "JP2"), i_ads, 2 * r_sock)
            contact("shunts and module sockets", pads_of(board, "J7", ("1", "2", "4")), i_esp, r_sock + r_sock / 2)
            contact("shunts and module sockets", pads_of(board, "J10", ("1", "2", "13")), i_ads, r_sock + r_sock / 2)

        # GND pour, DC currents into the pour (AC loops stay at the capacitors)
        gnd_inj = []
        if load:
            gnd_inj.append((pads_of(board, "U6", net="GND"), b["d"] * b["iin"]))
            gnd_inj.append((pads_of(board, "J10", ("2", "13")), i_ads))
            gnd_inj.append((pads_of(board, "J7", ("2", "4")), i_esp))
        if case["mode"] == "battery":
            gnd_inj.append((pads_of(board, "J3", ("2",)), -sum(c for _, c in gnd_inj)))
        else:
            gnd_inj.append((pads_of(board, "J3", ("2",)), i_chg))
            gnd_inj.append((pads_of(board, "U4", ("18", "19")), -(1 - buck["d"]) * i_l1))
            gnd_inj.append((pads_of(board, "J1", ("A1", "A12", "B1", "B12")), -sum(c for _, c in gnd_inj)))
        _, p_gnd, gmap = model.gnd_solve(gnd_inj, temps)
        losses["copper GND pour"] = p_gnd
        for k, v in gmap.items():
            heat[k] = heat.get(k, 0.0) + v

        # small parts: indicator LEDs and their resistors
        if load:
            p = VOUT * LED_5V_A
            losses["LEDs and series resistors"] = losses.get("LEDs and series resistors", 0.0) + p
            pad_heat(heat, g, pads_of(board, "R26") + pads_of(board, "D5"), p)
        if case["mode"] == "usb":
            p = 2 * LED_SYS_A * op["VSYS"]
            losses["LEDs and series resistors"] = losses.get("LEDs and series resistors", 0.0) + p
            pad_heat(heat, g, pads_of(board, "R23") + pads_of(board, "R31") + pads_of(board, "D4"), p)

        # inside the case: board losses, modules, battery
        board_loss = sum(losses.values())
        p_modules = VOUT * load - sum(v for k, v in losses.items()
                                      if k in ("TPS22975", "LM73100 ideal diodes", "copper BOOST_5V", "copper RAIL_5V",
                                               "copper ESP_5V_D", "copper ADS_5V_D", "copper ESP_5V",
                                               "copper ADS_5V", "shunts and module sockets"))
        p_pack = i_b ** 2 * r_pack
        inside = board_loss + max(p_modules, 0.0) + p_pack
        t_air = ambient + (vent_model.air_rise(inside, ambient, vent_area_cm2, wall_resistance=r_enc)
                           if env["enclosed"] else 0.0)

        # thermal solve
        b_vec = [0.0] * (NL * nc + len(model.nodes))
        for (li, c), p in heat.items():
            b_vec[li * nc + c] += p
        for ref, keys in (("U4", U4_LOSSES), ("U6", ("TPS61088",)), ("U7", ("TPS22975",)),
                          ("L1", ("L1",)), ("L2", ("L2",))):
            b_vec[model.nodes[ref]] += sum(losses.get(k, 0.0) for k in keys)
        rise = thermal.solve(b_vec, tol=1e-7)
        temps = [t_air + r for r in rise[:NL * nc]]
        new_tj = {ref: t_air + rise[model.nodes[ref]] for ref in tj}
        converged = all(abs(new_tj[r] - tj[r]) < 0.2 for r in tj)
        tj = new_tj
        if case["mode"] == "usb":
            # MAX77757 junction regulation: the charge current, then the input current, folds back
            # so that the junction stays at TREG; a negative value means the battery supplements
            if tj["U4"] > regulation_temperature + 0.5 or (chg_cap < charge_current and tj["U4"] < regulation_temperature - 0.5):
                chg_cap = min(charge_current, chg_cap - 0.8 * (tj["U4"] - regulation_temperature) / TREG_SLOPE)
                chg_cap = max(chg_cap, -(b["iin"] + 2 * LED_SYS_A))  # the buck cannot sink current
                regulated = chg_cap < charge_current
                converged = False
        # VSYS track drop between U4 and L2 for the boost input in the USB case
        if case["mode"] == "usb" and load:
            vsys_drop = b["iin"] * model.r_net["VSYS"]
        if converged and outer:
            break
    else:
        raise RuntimeError("%s (%s, %.0f °C, corner %d): thermal iteration did not converge after %d iterations"
                           % (case["key"], env_key, ambient, corner, max_iterations))
    battery = case["mode"] == "battery"
    sink = {ref: (tj[ref] - t_air) / heatsink_resistance(hs, env) for ref, hs in model.heatsink.items()}
    return {"op": op, "losses": losses, "board_loss": board_loss, "modules": p_modules, "pack": p_pack, "sink": sink,
            "inside": inside, "t_air": t_air, "tj": tj, "temps": temps,
            "board_max": max(temps), "t_pack": t_air + p_pack * pack_resistance(), "iterations": outer + 1,
            "efficiency": VOUT * load / (vcell * i_bat) if battery else None, "i_bat": i_b,
            "i_in": op.get("input current", 0.0), "regulated": regulated, "i_chg": 0.0 if battery else i_chg,
            "caps": max((max(temps[g.cell(p.x, p.y)] for p in pads_of(board, ref)), ref) for ref in BULK_CAPS)}


# ------------------------------------------------------------------------------------------
# Validation: point source on an infinite plate against the Bessel solution


def bessel_k0(x):
    """K0(x) = ∫0^∞ exp(-x cosh t) dt, numerically."""
    n, upper = 4000, 12.0
    h = upper / n
    return h * (0.5 * math.exp(-x) + sum(math.exp(-x * math.cosh(i * h)) for i in range(1, n)))


def validate():
    """A single copper layer (1 oz plus FR4) with h on both faces, 1 W at the centre of a
    201 x 201 mm grid, against the infinite-plate solution P K0(m r) / (2π k t)."""
    outline = (0.0, 0.0, 201.0, 201.0)
    g = Grid(outline)
    sheet = Sheet(g)
    kt = K_CU * 35e-6 + K_FR4_PLANE * 1.51e-3
    h = 2 * 10.0
    sheet.set_sheet(0, [kt] * g.nc)
    sheet.gref[0] = [h * 1e-6] * g.nc
    for li in range(1, NL):  # the other layers are unused: no sheet, pinned to the reference
        sheet.gref[li] = [1.0] * g.nc
    b = [0.0] * (NL * g.nc)
    centre = g.cell(100.5, 100.5)
    b[centre] = 1.0
    t = sheet.solve(b, tol=1e-10)
    m = math.sqrt(h / kt)
    rows = []
    for r_mm in (3, 5, 10, 20):
        fd = t[g.cell(100.5 + r_mm, 100.5)]
        exact = bessel_k0(m * r_mm * 1e-3) / (2 * math.pi * kt)
        rows.append((r_mm, fd, exact))
    return rows


# ------------------------------------------------------------------------------------------
# Heat map


def heat_map(path, model, result, title, scale_range=None, heatsink=None):
    """Top-copper temperature map. `scale_range` fixes the colour scale (to compare two maps);
    `heatsink` draws the sink footprints of a HEATSINK table as dashed squares."""
    g = model.grid
    temps = result["temps"][:g.nc]
    lo, hi = scale_range if scale_range else (min(temps), max(temps))
    scale = 6
    w, h = g.nx * scale, g.ny * scale
    colors = [(49, 54, 149), (69, 117, 180), (116, 173, 209), (171, 217, 233), (254, 224, 144),
              (253, 174, 97), (244, 109, 67), (215, 48, 39), (165, 0, 38)]

    def color(t):
        f = 0 if hi == lo else (t - lo) / (hi - lo) * (len(colors) - 1)
        i = min(int(f), len(colors) - 2)
        a, b = colors[i], colors[i + 1]
        u = f - i
        return "#%02x%02x%02x" % tuple(int(a[k] + (b[k] - a[k]) * u) for k in range(3))

    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" '
           'font-family="Helvetica, Arial, sans-serif">' % (w + 170, h + 40, w + 170, h + 40),
           '<rect width="100%" height="100%" fill="#ffffff"/>',
           '<text x="4" y="16" font-size="13">%s</text>' % title, '<g transform="translate(0,24)">']
    for iy in range(g.ny):
        for ix in range(g.nx):
            out.append('<rect x="%d" y="%d" width="%d" height="%d" fill="%s"/>'
                       % (ix * scale, iy * scale, scale, scale, color(temps[iy * g.nx + ix])))
    for ref in ("U4", "L1", "U6", "L2", "U7", "F1", "J3", "J1", "JP1", "JP2"):
        pads = pads_of(model.board, ref)
        xs = [p.x - p.w / 2 for p in pads] + [p.x + p.w / 2 for p in pads]
        ys = [p.y - p.h / 2 for p in pads] + [p.y + p.h / 2 for p in pads]
        x0, y0 = (min(xs) - g.x0) * scale, (min(ys) - g.y0) * scale
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="none" stroke="#000" '
                   'stroke-width="1"/>' % (x0, y0, (max(xs) - min(xs)) * scale, (max(ys) - min(ys)) * scale))
        out.append('<text x="%.1f" y="%.1f" font-size="11">%s</text>' % (x0, y0 - 2, ref))
    for ref, hs in (heatsink or {}).items():
        pads = pads_of(model.board, ref)
        cx = sum(p.x for p in pads) / len(pads) - g.x0
        cy = sum(p.y for p in pads) / len(pads) - g.y0
        half = hs["size_mm"] / 2
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="none" stroke="#000" '
                   'stroke-width="1.5" stroke-dasharray="4,3"/>' % ((cx - half) * scale, (cy - half) * scale,
                                                                   hs["size_mm"] * scale, hs["size_mm"] * scale))
        out.append('<text x="%.1f" y="%.1f" font-size="11">%s sink %.0f x %.0f</text>'
                   % ((cx - half) * scale, (cy + half) * scale + 12, ref, hs["size_mm"], hs["size_mm"]))
    for k in range(len(colors)):
        t = lo + (hi - lo) * k / (len(colors) - 1)
        y = h - k * (h / len(colors)) - h / len(colors)
        out.append('<rect x="%d" y="%.1f" width="16" height="%.1f" fill="%s"/>' % (w + 12, y, h / len(colors), color(t)))
        out.append('<text x="%d" y="%.1f" font-size="11">%.1f °C</text>' % (w + 32, y + h / len(colors) / 2 + 4, t))
    out.append("</g></svg>")
    with open(path, "w") as f:
        f.write("\n".join(out))


# ------------------------------------------------------------------------------------------
# Report


def fmt_w(p):
    return "%.0f mW" % (p * 1e3) if p < 1 else "%.2f W" % p


def u4_loss(result):
    return sum(result["losses"].get(k, 0.0) for k in U4_LOSSES)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--write", action="store_true", help="store heat/results.md and the heat maps")
    ap.add_argument("--pcb", default=PCB, help="board file to read, e.g. a copy with refilled zones")
    ap.add_argument("--no-heatsink", action="store_true", help="model the board without the U4 heatsink")
    args = ap.parse_args()
    started = time.time()
    with open(PARAMS) as f:
        params = json.load(f)
    board = read_board(args.pcb)
    copper = Copper(board)
    k_sw, p0, fit_rows = fit_boost(params["boost_efficiency_curve"])
    rep = simlib.Report()
    rep.add("# Power loss and temperature estimate")
    rep.add()
    rep.add("Generated by `hardware/sim/run_heat.py` from `hardware/carrier-max-charger/carrier.kicad_pcb`. "
            "See `hardware/sim/README.md` for what is and is not modelled.")
    rep.add()

    # -- board
    g = copper.grid
    rep.add("## Board as read")
    rep.add()
    rep.add("%.0f x %.0f mm, four layers (JLC04161H-2116C): 35 µm outer and 15.2 µm inner copper, "
            "%.2f / %.3f / %.2f mm dielectric. Copper share from the zone fills, tracks, pads and vias on a "
            "%.1f mm grid: %s. %d vias, %d of them GND. Zone fills are taken as stored in the file. "
            "Refill them before running after a layout change."
            % (board["outline"][2] - board["outline"][0], board["outline"][3] - board["outline"][1],
               DIEL[0] * 1e3, DIEL[1] * 1e3, DIEL[2] * 1e3, g.dx,
               ", ".join("%s %.0f %%" % (name, 100 * sum(cu) / g.nc) for name, cu in zip(LAYERS, copper.cu)),
               len(board["vias"]), sum(1 for v in board["vias"] if v[4] == "GND")))
    rep.add()

    # -- validation
    rows = validate()
    rep.add("## Solver check")
    rep.add()
    rep.add("One copper layer with FR4, 10 W/m²K on each face, 1 W point source, against the "
            "infinite-plate solution P·K0(m·r) / (2π·k·t):")
    rep.add()
    rep.table(["Distance", "Grid", "Analytic", "Error"],
              [["%d mm" % r, "%.2f K" % fd, "%.2f K" % ex, "%+.1f %%" % (100 * (fd / ex - 1))]
               for r, fd, ex in rows])
    worst = max(abs(fd / ex - 1) for r, fd, ex in rows if r >= 5)
    rep.check(worst < 0.05, "grid solution within 5 %% of the analytic plate from 5 mm on (worst %.1f %%)"
              % (100 * worst))
    rep.add()

    # -- inputs
    rep.add("## Loss model inputs")
    rep.add()
    rep.table(["Element", "Value", "Source"], [
        ["MAX77757 CHGIN-BYP / high side / low side / BATT-SYS", "45 / 60 / 60 / 20 mΩ typ",
         "data sheet, Electrical Characteristics"],
        ["MAX77757 switching", "1.3 MHz; edge loss with the TPS61088 coefficient below", "estimate, no efficiency curve published"],
        ["MAX77757 CHGIN supply current", "2.7 mA", "data sheet, typ"],
        ["MAX77757 VSYSMIN", "3.5 V; below it the BATT-SYS switch regulates linearly", "data sheet, SYS Regulation Voltage"],
        ["TPS61088 high side / low side", "13 / 11 mΩ typ", "SLVSCM8D"],
        ["L1 FXL0630-1R0-M / L2 CKST0603-2.2uH", "7.4 / 15.5 mΩ DCR, core loss %.0f mW each" % (CORE_LOSS * 1e3),
         "docs/power.md, CENKER table; core loss estimate"],
        ["TPS22975", "16 mΩ typ", "SLVSCI1"],
        ["LM73100 ideal diodes U11, U12", "drop regulated to %.1f / %.1f mV, %.1f / %.1f mΩ above that" % (
            ID_VFWD[0] * 1e3, ID_VFWD[1] * 1e3, ID_RON[0] * 1e3, ID_RON[1] * 1e3), "SNOSDC0A, typ / max over temperature"],
        ["On-resistance temperature coefficient", "+%.1f %%/K" % (TC_RDSON * 100), "estimate"],
        ["USB-C contacts (4 VBUS, 4 GND)", "%.0f / %.0f mΩ each" % (USB_CONTACT[0] * 1e3, USB_CONTACT[1] * 1e3),
         "Hanbo MC-107DMS: 40 mΩ max"],
        ["JST-PH battery contacts", "%.0f / %.0f mΩ each" % (JST_CONTACT[0] * 1e3, JST_CONTACT[1] * 1e3), "JST PH: 10 mΩ max initial"],
        ["Shunts and module sockets", "%.0f / %.0f mΩ per contact" % (SOCKET_CONTACT[0] * 1e3, SOCKET_CONTACT[1] * 1e3), "estimate"],
        ["F1 Littelfuse 0466005.NRHF 5 A (C57525)", "%.0f / %.0f mΩ" % (FUSE_R[0] * 1e3, FUSE_R[1] * 1e3),
         "nominal cold resistance, datasheets/littelfuse-466.pdf; high corner estimate"],
        ["Battery pack, DC", "%.0f / %.0f mΩ" % (PACK_R[0] * 1e3, PACK_R[1] * 1e3), "at most 50 mΩ AC 1 kHz in the pack data sheet; DC estimate"],
        ["Copper", "1.72 µΩ·cm at 20 °C, +0.393 %/K, tracks and pour at their local temperature", ""],
    ])
    rep.add("Where two values are given, the first is the nominal corner and the second the high corner.")
    rep.add()
    rep.add("TPS61088 switching term: P = K · VOUT · IIN · fSW + P0 with K = %.2f ns and P0 = %.0f mW. "
            "It is fitted to what the efficiency curve in `specs/power_params.json` at 3.6 V leaves after the "
            "conduction, DCR and core losses. The curve is TI Figure 6-2 less 2 points for 1 MHz and the CKST0603 DCR."
            % (k_sw * 1e9, p0 * 1e3))
    rep.add()
    fit = (k_sw, p0)
    rep.table(["IOUT", "Curve η", "Model η"],
              [["%.1f A" % iout, "%.1f %%" % (eta * 100),
                "%.1f %%" % (boost_losses(3.6, iout, 25 + 30 * iout, 25 + 20 * iout, fit)["eta"] * 100)]
               for _, _, iout, eta in fit_rows])

    # -- path resistances
    heatsink = None if args.no_heatsink else HEATSINK
    nominal = Model(board, copper, fit, params, 0, heatsink)
    high = Model(board, copper, fit, params, 1, heatsink)
    bare = (Model(board, copper, fit, params, 0), Model(board, copper, fit, params, 1))  # no heatsink, for comparison
    r_net, r_gnd_bat, r_gnd_usb = solve_paths(nominal, board)
    for m in (nominal, high) + bare:
        m.r_net, m.r_gnd_bat, m.r_gnd_usb = r_net, r_gnd_bat, r_gnd_usb
    rep.add("## Copper path resistances at 20 °C")
    rep.add()
    rep.add("Each power net is solved as a resistor network of its tracks and vias. Pads are nodes, and the "
            "pins of a group share the current equally. GND is a four-layer sheet on the grid.")
    rep.add()
    rep.table(["Path", "Resistance"], [[k, "%.1f mΩ" % (v * 1e3)] for k, v in (
        ("USB_VBUS_J1: J1 VBUS pins to FB1", r_net["USB_VBUS J1"]),
        ("USB_VBUS: FB1 to U4 CHGIN", r_net["USB_VBUS"]),
        ("CHG_SW: U4 LX to L1", r_net["CHG_SW"]),
        ("VSYS: L1 to U4 SYS", r_net["VSYS L1"]),
        ("VSYS: U4 SYS to L2", r_net["VSYS"]),
        ("VBAT: F1 to U4 BATT", r_net["VBAT"]),
        ("BAT_IN: J3 to F1", r_net["BAT_IN"]),
        ("BOOST_SW: L2 to U6 SW", r_net["BOOST_SW"]),
        ("BOOST_5V: U6 VOUT to U7 VIN", r_net["BOOST_5V"]),
        ("RAIL_5V: U7 VOUT to U11 IN", r_net["RAIL_5V"]),
        ("U12 OUT to JP1", r_net["ESP_5V_D"]),
        ("U11 OUT to JP2", r_net["ADS_5V_D"]),
        ("ADS_5V: JP2 to J10", r_net["ADS_5V"]),
        ("ESP_5V: JP1 to J7", r_net["ESP_5V"]),
        ("GND: J3 pin 2 to U6 ground pads", r_gnd_bat),
        ("GND: J1 GND pins to U4 PGND", r_gnd_usb))])

    # -- cases
    r_enc, a_enc = enclosure_resistance()
    rep.add("## Thermal environment")
    rep.add()
    rep.add("- Open bench: board in still air, %.0f W/m²K on both faces (convection plus radiation), "
            "no enclosure." % ENVIRONMENTS["open"]["h_top"])
    rep.add("- Closed case (not yet designed; assumed): %.0f x %.0f x %.0f mm outside, %.0f mm printed wall "
            "(k %.1f W/m·K), %.0f W/m²K inside and %.0f W/m²K outside. Wall path to ambient %.1f K/W "
            "over %.0f cm². Inside it the board loses %.0f W/m²K per face to the case air (modules above, "
            "battery below). The air carries everything dissipated in the case: board losses, the modules' "
            "5 V power and the pack's own I²R. The pack: %.1f K/W to the case air."
            % (ENCLOSURE_MM + (WALL_MM, K_WALL, H_IN, H_OUT, r_enc, a_enc * 1e4,
                               ENVIRONMENTS["closed"]["h_top"], pack_resistance())))
    rep.add("- Vents: %.1f cm² each for the lower inlet and upper outlet, %.0f %% open grille, "
            "%.0f mm vertical separation, discharge coefficient %.1f. Natural draft acts in parallel "
            "with the wall path; no fan."
            % (vent_model.VENT_AREA_CM2, vent_model.BLOCKAGE * 100, vent_model.VENT_HEIGHT_M * 1000,
               vent_model.CD))
    rep.add("- Junction to board: MAX77757 θJC %.1f K/W onto its pins, TPS61088 RθJC(bot) 3.1 K/W plus 1 K/W "
            "solder onto its pad, TPS22975 RθJC(bot) %.1f K/W; inductor winding to terminals %.0f K/W."
            % (CHG_THETA_JC, LS_THETA_JC, INDUCTOR_THETA_BODY))
    for ref, hs in (heatsink or {}).items():
        under = under_heatsink(board, ref, hs["size_mm"])
        rep.add("- %s heatsink: %s. Junction to package top %.0f K/W, epoxy and spacer %.0f K/W, sink to air "
                "%.0f K/W on the bench and %.0f K/W in the case air. That gives %.0f and %.0f K/W from the junction, "
                "in parallel with the pins. Parts with pads inside the %.0f x %.0f mm footprint of the sink: %s. "
                "The spacer must lift the sink base above the tallest of them."
                % (ref, hs["label"], hs["r_jt"], hs["r_tim"], hs["r_sink_open"],
                   hs["r_sink_open"] * ENVIRONMENTS["open"]["h_top"] / ENVIRONMENTS["closed"]["h_top"],
                   heatsink_resistance(hs, ENVIRONMENTS["open"]), heatsink_resistance(hs, ENVIRONMENTS["closed"]),
                   hs["size_mm"], hs["size_mm"], ", ".join(under) or "none"))
    if args.no_heatsink:
        rep.add("- No heatsink (--no-heatsink).")
    rep.add()

    # every case in the closed case at both corners; on the open bench only 25 °C, nominal.
    # The tables below rely on this order: case, environment, ambient, corner.
    label = {c["key"]: c["label"] for c in CASES}
    results = {}
    for case in CASES:
        for env_key in ENVIRONMENTS:
            for ambient in case["ambients"]:
                if env_key == "open" and ambient != 25.0:
                    continue
                for corner, model in ((0, nominal), (1, high)):
                    if corner and env_key == "open":
                        continue
                    results[(case["key"], env_key, ambient, corner)] = run_case(
                        model, case, env_key, ambient, k_sw * (1.0, BUCK_SW_HIGH)[corner])

    rep.add("## Operating points and losses")
    rep.add()
    rep.add("Board losses for the closed case at 25 °C ambient, with copper and on-resistance at their computed "
            "temperatures. Battery cases: the boost input is held at 3.1 V, the low-battery design point of "
            "docs/power.md. The cell open-circuit voltage that gives it follows from the path. USB cases: 5.0 V at "
            "the connector (cable drop not included), charge current 1.8 A unless the 3 A input limit reduces it.")
    rep.add()
    for case in CASES:
        rn = results[(case["key"], "closed", 25.0, 0)]
        rh = results[(case["key"], "closed", 25.0, 1)]
        rep.add("### %s" % case["label"])
        rep.add()
        ops = []
        for k, v in rn["op"].items():
            if isinstance(v, str):
                ops.append("%s: %s" % (k, v))
            elif "efficiency" in k or "duty" in k:
                ops.append("%s %.1f %%" % (k, v * 100))
            elif "current" in k:
                ops.append("%s %.2f A" % (k, v))
            else:
                ops.append("%s %.2f V" % (k, v))
        rep.add("Nominal corner: " + ", ".join(ops) + ".")
        rep.add()
        rep.table(["Element", "Nominal", "High"],
                  [[k, fmt_w(rn["losses"][k]), fmt_w(rh["losses"].get(k, 0.0))]
                   for k in sorted(rn["losses"], key=lambda k: -rn["losses"][k])]
                  + [["**Board total**", "**%s**" % fmt_w(rn["board_loss"]), "**%s**" % fmt_w(rh["board_loss"])],
                     ["Modules (5 V power, heat inside the case)", fmt_w(rn["modules"]), fmt_w(rh["modules"])],
                     ["Battery pack I²R", fmt_w(rn["pack"]), fmt_w(rh["pack"])],
                     ["Inside the case", fmt_w(rn["inside"]), fmt_w(rh["inside"])]])
        if rn["efficiency"]:
            rep.add("Cell to 5 V output: %.1f %% nominal, %.1f %% high corner."
                    % (rn["efficiency"] * 100, rh["efficiency"] * 100))
            rep.add()

    rep.add("## Temperatures")
    rep.add()
    rep.add("Hot spots from the grid solution. \"θJA\" is the naive figure from the data sheet's JEDEC θJA "
            "with the same loss at the same ambient, for comparison only.")
    rep.add()
    rows = []
    for (ck, env_key, ambient, corner), r in results.items():
        l = r["losses"]
        rows.append([label[ck] if corner == 0 else "", env_key if corner == 0 else "",
                     "%.0f °C" % ambient if corner == 0 else "", CORNERS[corner],
                     "%.1f" % r["t_air"],
                     "%.0f (θJA %.0f)" % (r["tj"]["U4"], ambient + u4_loss(r) * CHG_THETA_JA_JEDEC),
                     "%.0f (θJA %.0f)" % (r["tj"]["U6"], ambient + l["TPS61088"] * BST_THETA_JA_JEDEC)
                     if "TPS61088" in l else "-",
                     "%.0f" % r["tj"]["U7"] if "TPS22975" in l else "-",
                     "%.0f" % r["tj"]["L1"] if "L1" in l else "-",
                     "%.0f" % r["tj"]["L2"] if "L2" in l else "-",
                     "%.0f" % r["board_max"], "%.0f (%s)" % r["caps"], "%.1f" % r["t_pack"]])
    rep.table(["Case", "Environment", "Ambient", "Corner", "Case air", "U4 Tj", "U6 Tj", "U7 Tj", "L1", "L2",
               "Board max", "Hottest X7R", "Pack"], rows)
    rep.add("All temperatures in °C. Board max is the hottest grid cell (1 mm), not a pad.")
    rep.add()

    # -- checks
    rep.add("## Checks")
    rep.add()

    def worst(value):
        """Highest value over the closed-case results: (value, case key, ambient, corner)."""
        return max(((value(r), ck, amb, corner) for (ck, ek, amb, corner), r in results.items() if ek == "closed"),
                   key=lambda item: item[0])

    usb = [c["key"] for c in CASES if c["mode"] == "usb"]
    bat = [c["key"] for c in CASES if c["mode"] == "battery"]
    for ck in usb:
        r = results[(ck, "open", 25.0, 0)]
        rep.check(not r["regulated"], "%s, open bench, 25 °C: %s (U4 junction %.0f °C, regulation at %.0f °C)"
                  % (label[ck], "full %.1f A charge current" % CHARGE_A if not r["regulated"]
                     else "thermal regulation cuts the charge current to %.2f A" % r["i_chg"], r["tj"]["U4"], CHG_TREG))
    for ref, limit, what in (("U6", 125.0, "TPS61088 junction"), ("U7", 125.0, "TPS22975 junction"),
                             ("L1", 125.0, "L1 winding"), ("L2", 125.0, "L2 winding")):
        v, ck, amb, corner = worst(lambda r: r["tj"][ref])
        rep.check(v < limit, "%s at most %.0f °C (%s, %.0f °C ambient, %s corner, closed case), limit %.0f °C"
                  % (what, v, label[ck], amb, CORNERS[corner], limit))
    v, ck, amb, corner = worst(lambda r: r["board_max"])
    rep.check(v < 125, "hottest board cell %.1f °C (%s, %.0f °C ambient, %s corner, closed case); limit 125 °C "
              "(FR4 Tg 130 °C and up)" % (v, label[ck], amb, CORNERS[corner]))
    for amb_limit in (25.0, 50.0):
        hot = max(((r["caps"][0], r["caps"][1], ck, corner) for (ck, ek, amb, corner), r in results.items()
                   if ek == "closed" and amb == amb_limit), default=None)
        if hot:
            rep.check(hot[0] <= CAP_RATING, "X7R bulk capacitors at %.0f °C ambient, closed case: hottest %s at %.0f °C "
                      "(%s, %s corner), rating %.0f °C" % (amb_limit, hot[1], hot[0], label[hot[2]],
                                                         CORNERS[hot[3]], CAP_RATING))
    for ck in bat:
        r = results[(ck, "closed", 25.0, 1)]
        rep.check(r["t_pack"] < THERMOSTAT_MIN, "%s, closed case, 25 °C ambient, high corner: pack at %.1f °C; "
                  "it must stay below the thermostat's lowest opening point %.0f °C (60 °C ± 5 K)"
                  % (label[ck], r["t_pack"], THERMOSTAT_MIN))
    i_jst = max(r["i_bat"] for r in results.values())
    rep.check(i_jst <= 2.0, "battery current at most %.2f A through J3, JST PH rating 2 A (AWG 24)" % i_jst)
    i_usb = max(r["i_in"] for r in results.values())
    rep.check(i_usb <= 5.0, "USB input current at most %.2f A, MC-107DMS rating 5 A" % i_usb)
    rep.add()

    # -- thermal regulation
    rep.add("## Charger thermal regulation")
    rep.add()
    rep.add("The MAX77757 holds its junction at %.0f °C by reducing the charge current. Below zero charge "
            "current it reduces the input current, so that the battery supplements the system (data sheet, "
            "Thermal Regulation and Thermal Shutdown State). The model applies the same rule. The table gives the "
            "charge current the board sustains in steady state. Negative: the battery discharges into the load "
            "although USB is present." % CHG_TREG)
    rep.add()
    rep.table(["Case", "Environment", "Corner", "Charge current", "Input current", "U4 Tj", "U4 loss"],
              [[label[ck], ENVIRONMENTS[env_key]["label"], CORNERS[corner], "%.2f A" % r["i_chg"], "%.2f A" % r["i_in"],
                "%.0f °C" % r["tj"]["U4"], "%.2f W" % u4_loss(r)]
               for (ck, env_key, _, corner), r in results.items() if ck in usb])

    # -- heatsink against the bare package
    bare_results = {}
    if heatsink:
        rep.add("## U4 heatsink against the bare package")
        rep.add()
        rep.add("The same operating points solved without the heatsink. The sink moves heat from the die into the "
                "surrounding air. In the closed case that air is the case air, whose temperature it does not change.")
        rep.add()
        rows = []
        for ck in usb + ["bat08", "bat10"]:
            case = next(c for c in CASES if c["key"] == ck)
            for env_key, corner in (("open", 0), ("closed", 0), ("closed", 1)):
                if ck.startswith("bat") and corner:
                    continue
                r = results[(ck, env_key, 25.0, corner)]
                r0 = bare_results[(ck, env_key, 25.0, corner)] = run_case(bare[corner], case, env_key, 25.0, k_sw)
                p_u4 = u4_loss(r)
                rows.append([label[ck], ENVIRONMENTS[env_key]["label"], CORNERS[corner],
                             "%.0f to %.0f °C" % (r0["tj"]["U4"], r["tj"]["U4"]),
                             ("%.2f to %.2f A" % (r0["i_chg"], r["i_chg"])) if case["mode"] == "usb" else "-",
                             "%.2f W of %.2f W" % (r["sink"]["U4"], p_u4) if p_u4 else "-",
                             "%.0f to %.0f K/W" % ((r0["tj"]["U4"] - r0["t_air"]) / p_u4,
                                                   (r["tj"]["U4"] - r["t_air"]) / p_u4) if p_u4 else "-"])
        rep.table(["Case", "Environment", "Corner", "U4 junction, bare to sink", "Charge current, bare to sink",
                   "Heat through the sink", "U4 junction to air, bare to sink"], rows)
        rep.add("Junction to air is the junction rise over the surrounding air divided by the U4 loss at the "
                "operating point with the sink. In regulation the loss differs between the two solutions.")
        rep.add()
    rep.add("## Charging and the pack temperature window")
    rep.add()
    rep.add("The charger reads the pack NTC. Above 31.4 °C it lowers the termination voltage to 4.05 V, above "
            "42.1 °C it stops (nominal thresholds, docs/power.md). In the closed case the pack sits in the case "
            "air, so the ambient at which these are reached is:")
    rep.add()
    rows = []
    for ck in usb:
        for corner in (0, 1):
            r = results[(ck, "closed", 25.0, corner)]
            rise = r["t_pack"] - 25.0
            rows.append([label[ck] if corner == 0 else "", CORNERS[corner], "%.1f K" % rise,
                         "%.1f °C" % (31.4 - rise), "%.1f °C" % (42.1 - rise)])
    rep.table(["Case", "Corner", "Pack above ambient", "Ambient for 4.05 V limit", "Ambient for charge stop"], rows)
    rep.add("On the open bench the pack rises only by its own I²R loss (a few tenths of a kelvin at 1.8 A).")
    rep.add()

    # -- limits
    rep.add("## Limits of the model")
    rep.add()
    for line in (
        "Steady state only. The 1.5 A output margin is short-term headroom and not a thermal case.",
        "The enclosure is not designed. Its size, material, vents and the battery position decide the case-air "
        "rise, which dominates the absolute temperatures in the closed case. The modules' 5 V power is most of "
        "the heat in it.",
        "Board-to-air coefficients are lumped (one value per face). Modules on the top side shade the board; their "
        "own heat reaches the board only through the case air, not through the header pins.",
        "The MAX77757 has no published efficiency curve. Its switching loss uses the edge coefficient fitted to "
        "the TPS61088, scaled with voltage, current and frequency. The fit itself rests on the project's curve, "
        "which is TI's Figure 6-2 less an estimated 2 points.",
        "θJC values are JEDEC figures. For the flip-chip MAX77757 (no exposed pad) the 7.5 K/W is applied from "
        "the junction to its pins as a group.",
        "The U4 heatsink path is an estimate on top of that: junction to package top is not published for the "
        "FC2QFN (25 to 45 K/W assumed), the epoxy layers depend on the assembly, and the sink rating is a free-air "
        "figure derated for the lower temperature rise and the case air. Bench item: thermocouple on the sink base.",
        "Contact, fuse and pack resistances are data sheet maxima or estimates (see the input table). The USB "
        "cable is not included; its drop lowers CHGIN towards the 4.5 V input regulation threshold.",
        "Only DC currents are solved in the copper. Ripple currents in the capacitor loops and their ESR losses "
        "are small and stay inside the converter footprints.",
        "Bench verification: thermocouple or IR camera on U4, U6, L1, L2 and the pack at the charging case with "
        "the real enclosure, and at the 3.1 V / 0.8 A battery case.",
    ):
        rep.add("- " + line)
    rep.add()
    rep.add("Run time %.0f s, grid %d x %d x %d nodes." % (time.time() - started, g.nx, g.ny, NL))
    rep.add()

    if args.write:
        os.makedirs(OUT, exist_ok=True)
        for ck, env_key, amb in (("usb32", "closed", 25.0), ("bat08", "closed", 25.0), ("bat08", "open", 25.0),
                                 ("usb37", "open", 25.0)):
            r = results[(ck, env_key, amb, 0)]
            name = "map_%s_%s_%d" % (ck, env_key, amb)
            head = "%s, %s, %.0f °C ambient, nominal: top copper" % (label[ck], ENVIRONMENTS[env_key]["label"], amb)
            if heatsink:
                # the map as modelled (with the sink) and the bare board on one colour scale, for comparison
                case = next(c for c in CASES if c["key"] == ck)
                r0 = bare_results.get((ck, env_key, amb, 0)) or run_case(bare[0], case, env_key, amb, k_sw)
                both = r["temps"][:g.nc] + r0["temps"][:g.nc]
                rng = (min(both), max(both))
                heat_map(os.path.join(OUT, name + ".svg"), nominal, r, head + ", with the U4 heatsink", rng, heatsink)
                heat_map(os.path.join(OUT, name + "_bare.svg"), bare[0], r0, head + ", without heatsink", rng)
            else:
                heat_map(os.path.join(OUT, name + ".svg"), nominal, r, head + ", without heatsink")
    return rep.finish(RESULTS if args.write else None)


if __name__ == "__main__":
    sys.exit(main())
