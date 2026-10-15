#!/usr/bin/env python3
"""EMI estimate for the carrier: emission spectra of the two converters and of the carrier's
digital signals, radiated and conducted levels against CISPR 32 class B, and the converter
lines that fall into the receiver bands. Hot loops, track parasitics and return-path gaps are
read from hardware/carrier-max-charger/carrier.kicad_pcb.

    python3 hardware/sim/run_emi.py            # print the report
    python3 hardware/sim/run_emi.py --write    # also store emi/results.md and emi/spectrum_flarm.svg/.pdf
    python3 hardware/sim/run_emi.py --fast     # closed-form spectra only, no ngspice

Parts:
  1. Converter sources: one switching period of each hot loop in ngspice (emi/switch_edge.cir)
     at 10 ps with the loop inductance from the board; switch-node and loop-current line
     spectra, ring frequency and amplitude, edge time and switch capacitance sweeps, the
     boost snubber.
  2. Digital signals: edge time after series resistors and track capacitance, trapezoid
     spectra, loop area from track length and dielectric height.
  3. Radiated field at 3 m from every loop against CISPR 32 class B.
  4. Converter and digital lines inside the ADS-B, FLARM and GNSS bands at the antenna mounts.
  5. Conducted emission on the USB input through a LISN (emi/usb_lisn.cir).
  6. Return paths: bottom-layer signal tracks over In2 power copper.
  7. Rail ripple at the module sockets and the battery harness loop (power/*.cir benches).

Needs numpy (hardware/sim/.venv) and ngspice for parts 1, 5 and 7.
"""

import argparse
import math
import os
import re
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import boosts  # noqa: E402
import run_heat  # noqa: E402  (board reader, stackup)
import run_power  # noqa: E402  (operating points, benches)
import simlib  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
BENCHES = os.path.join(HERE, "emi")
RESULTS = os.path.join(BENCHES, "results_max.md")  # results_<charger>.md, bind()
BENCH = simlib.Bench(BENCHES)

# --- limits. CISPR 32 class B at 3 m, quasi-peak in dBµV/m; above 1 GHz the average limit.
RADIATED = ((30e6, 230e6, 40.0), (230e6, 1e9, 47.0), (1e9, 3e9, 50.0), (3e9, 6e9, 54.0))
# conducted, mains port, quasi-peak dBµV: (f_low, f_high, limit at f_low, limit at f_high), log slope
CONDUCTED = ((150e3, 500e3, 66.0, 56.0), (500e3, 5e6, 56.0, 56.0), (5e6, 30e6, 60.0, 60.0))
CONDUCTED_AVERAGE_BELOW = 10.0
RBW = ((0.0, 30e6, 9e3), (30e6, 1e9, 120e3), (1e9, 1e11, 1e6))
MARGIN = 6.0  # dB pre-compliance target
DISTANCE = 3.0  # m
ETA = 376.73
C0 = 299792458.0
KT = 1.380649e-23 * 290

# --- stackup and copper (run_heat reads the board file)
H_REF = run_heat.DIEL[0]  # outer track to the adjacent plane, both sides
ER = 4.0  # effective permittivity of the two prepreg sublayers (4.16 and 3.91)
RHO = run_heat.RHO_CU
L_VIA = 0.3e-9  # one via through the 0.2 mm prepreg to the plane with its pads (estimate)
R_VIA = 0.5e-3

# --- converter hot loops. Pads in loop order; consecutive pads on the same net are copper runs.
L_PKG = 1.0e-9  # QFN lead frame and bond wires, two pins in the loop (estimate)
ESL = {"0603": 0.4e-9, "0805": 0.5e-9, "1206": 0.7e-9}
# switch output capacitance, not specified: assumed from the switch size (10 A class for the
# TPS61088, 3 A for the MAX77757), swept by a factor of two either way
COSS = {"buck": 300e-12}
EDGE_CASES = (2e-9, 5e-9, 10e-9)  # gate ramp, not specified ("very fast" per TI); current slew 10 A per ramp
EDGE_NOMINAL = 5e-9
BOOST_VIN, BOOST_IOUT = 3.1, 1.5  # lowest input, short-term peak: highest loop current and duty
BUCK_VSYS, BUCK_ILOAD = 4.2, 3.0
CONVERTERS, BOOST_KEY, BUCK_KEY = {}, None, None


def bind():
    """CONVERTERS for the project's charger: the boost, the charger buck at its PROG frequency, and for a
    charger with a selectable frequency one more row per option."""
    global CONVERTERS, BOOST_KEY, BUCK_KEY, RESULTS, SPECTRUM
    chg = run_power.CHG
    boost = run_heat.BOOST
    suffix = "" if run_heat.BOOST_NAME == boosts.DEFAULT else "_%s" % run_heat.BOOST_NAME
    RESULTS = os.path.join(BENCHES, "results_%s%s.md" % (run_heat.CHARGER, suffix))
    SPECTRUM = os.path.join(BENCHES, "spectrum_flarm_%s%s" % (run_heat.CHARGER, suffix))
    l1 = chg["l1"]["l"]

    def buck(fsw):
        d = BUCK_VSYS / 5.0
        return dict(dir=-1, mg=0, mb=1, sg=1, sb=0, vbank=5.0, fsw=fsw, d=d, idc=BUCK_ILOAD,
                    ron_main=chg["r_hs"], ron_sync=chg["r_ls"], cbank=19.2e-6, esr=1.5e-3,
                    esl=ESL["0805"] * ESL["1206"] / (ESL["0805"] + ESL["1206"]), loop=chg["emi_loop"],
                    caps=chg["emi_caps"], sw_net=chg["nets"]["sw"], coss=COSS["buck"], snubber=None, inductor="L1",
                    ripple=(5.0 - BUCK_VSYS) * d / (l1 * fsw),
                    point="5 V in, %.1f V / %.1f A out, %.2g MHz" % (BUCK_VSYS, BUCK_ILOAD, fsw / 1e6))
    BUCK_KEY = "buck U4 (%s, L1)" % chg["part"]
    eta = boost["emi_efficiency"]
    boost_fsw = boost.get("emi_fsw", boost["fsw"](BOOST_VIN))
    vout = boost.get("vout", run_power.VOUT)
    BOOST_KEY = "boost U6 (%s, L2)" % boost["part"]
    def boost_converter(fsw):
        return dict(
            dir=1, mg=1, mb=0, sg=0, sb=1, vbank=vout, fsw=fsw,
            d=1 - eta * BOOST_VIN / vout,
            idc=vout * BOOST_IOUT / (BOOST_VIN * eta),
            ron_main=boost["r_ls"], ron_sync=boost["r_hs"],
            cbank=run_power.C_MAIN * run_power.C_MAIN_BIAS,
            esr=1e-3, esl=ESL["0805"] / 3,
            loop=(("U6", "15"), ("C18", "1"), ("C18", "2"), ("U6", "13")),
            caps=("C18",), sw_net="BOOST_SW", coss=boost["coss"],
            snubber=(("U6", "4"), ("R39", "1"), ("R39", "2"), ("C36", "1"), ("C36", "2"))
            if boost.get("snubber", boost["load_switch"]) else None,
            spread=boost.get("spread_fraction", 0.0),
            inductor="L2", ripple=BOOST_VIN * (1 - eta * BOOST_VIN / vout)
            / (boost["l2_l"] * fsw),
            point="%.1f V in, %.1f A out" % (BOOST_VIN, BOOST_IOUT))
    CONVERTERS = {BOOST_KEY: boost_converter(boost_fsw)}
    for fsw in boost.get("emi_fsw_options", ()):
        CONVERTERS["boost U6 %.0f kHz (%s, L2)" % (fsw / 1e3, boost["part"])] = boost_converter(fsw)
    CONVERTERS[BUCK_KEY] = buck(chg["fsw"])
    for fsw in chg.get("emi_fsw_options", ()):
        if fsw != chg["fsw"]:
            CONVERTERS["%s %.2g MHz" % (BUCK_KEY, fsw / 1e6)] = buck(fsw)

INDUCTOR_TURNS, INDUCTOR_AREA, INDUCTOR_LEAK = 5, 10e-6, 0.1  # as in run_power

# --- digital signals on the carrier. Driver edge times are not on any data sheet: assumed.
T_ESP, T_RP, T_GNSS, T_U13, T_U9 = 2e-9, 2e-9, 3e-9, 10e-9, 2e-9
R_OUT = 40.0  # driver output resistance (estimate)
C_PIN = 5e-12  # one receiver pin
# net, driver edge, edges per second, swing, series resistor before this net, extra load C, note
DIGITAL = (
    ("I2C_SCL", T_U13, 1e5, 3.3, 0.0, 60e-12, "100 kHz clock; falling edge by the driver, rise 5.1 kΩ x bus"),
    ("I2C_SDA", T_U13, 5e4, 3.3, 0.0, 60e-12, "data, half the clock edges"),
    ("ESP_UART_TX", T_ESP, 115200 / 2, 3.3, 0.0, 0.0, "display TX to R30, 115200 baud"),
    ("ADS_COMMS_RX", T_ESP, 115200 / 2, 3.3, 1e3, C_PIN, "behind R30 1 kΩ to the ADSBee"),
    ("ADS_COMMS_TX", T_RP, 115200 / 2, 3.3, 0.0, 0.0, "ADSBee TX to R29, 115200 baud"),
    ("ESP_UART_RX", T_RP, 115200 / 2, 3.3, 1e3, C_PIN, "behind R29 1 kΩ to the display"),
    ("GNSS_TXO", T_GNSS, 38400 / 2, 3.3, 0.0, C_PIN, "GNSS TX, 38400 baud"),
    ("GNSS_RXI", T_RP, 38400 / 2, 3.3, 0.0, C_PIN, "ADSBee TX to the GNSS"),
    ("GNSS_PPS", T_GNSS, 2.0, 3.3, 0.0, C_PIN, "1 Hz pulse"),
    ("BUZZ_PWM", T_ESP, 8e3, 3.3, 0.0, C_PIN, "4 kHz buzzer PWM to U9"),
    ("BUZZ_DRV", T_U9, 8e3, 3.3, 0.0, 0.0, "U9 output to R28"),
)
NOT_MODELLED = ("USB_DP and USB_DN carry only the BC1.2 detection at plug-in, no traffic. "
                "PWR_EN, SW_EN, the LED and status nets are static.")

# --- receivers at the antenna mounts. Interference threshold: 6 dB below the thermal noise
# floor kTB + NF of the receiver, so the noise floor rises by less than 1 dB.
ANT_GAIN = 1.64  # dipole
ANTENNA_OFFSET = 0.03  # m from the mount to the antenna's radiating centre on the enclosure wall (assumed)
RECEIVERS = (("ADS-B 1090 MHz", 1090e6, 4e6, 5.0, "MH8ADS980"),
             ("FLARM 868 MHz", 868.2e6, 250e3, 5.0, "MH7ADS1090"),
             ("GNSS L1 1575 MHz", 1575.42e6, 2e6, 2.0, "MH6ANTGNSS"))

# --- USB input and harness
USB_CABLE = (0.3, 1e-6)  # 1 m cable: R, L of the VBUS/GND loop (estimate)
# FB1 BLM31KN471SZ1L: DCR, inductance at 1.3 MHz without bias, loss resistance. The low-frequency
# inductance is not on the LCSC listing and falls with the up to 3 A DC current: estimated, swept.
FB1 = (20e-3, 1e-6, 600.0)
FB1_L_BIAS = (0.3e-6, 0.1e-6)
HARNESS = dict(length=0.1, spacing=5e-3, r=0.15, l=0.2e-6)  # pack leads: untwisted, pack + harness + fuse resistance
C_MODULE = 20e-6  # input capacitance of one module (assumed, as in run_power)


# ------------------------------------------------------------------------------------------
# Helpers

def dbuv(x):
    return 20 * math.log10(max(x, 1e-15) / 1e-6)


def dbm(watts):
    return 10 * math.log10(max(watts, 1e-30) / 1e-3)


def mhz(f):
    return "%.0f MHz" % (f / 1e6) if f >= 10e6 else "%.2f MHz" % (f / 1e6)


def rbw_at(f):
    return next(b for lo, hi, b in RBW if lo <= f < hi)


def radiated_limit(f):
    return next((lim for lo, hi, lim in RADIATED if lo <= f < hi), None)


def conducted_limit(f):
    for lo, hi, a, b in CONDUCTED:
        if lo <= f <= hi:
            return a + (b - a) * math.log(f / lo) / math.log(hi / lo)
    return None


def loop_field(f, area, current, r=DISTANCE, ground=True):
    """Far field of a small current loop (Ott): E = 1.316e-14 f² A I / r, doubled over the
    test-site ground plane. The near-field term 1/(kr)² is added for the in-enclosure case."""
    k = 2 * math.pi * f / C0
    e = 1.316e-14 * f * f * area * current / r * math.sqrt(1 + 1 / (k * r) ** 2)
    return 2 * e if ground else e


def received_power(e, f, gain=ANT_GAIN):
    lam = C0 / f
    return e * e / ETA * lam * lam * gain / (4 * math.pi)


def microstrip(width, h=H_REF, er=ER):
    """Z0, L' and C' per metre of a track over the plane (Hammerstad)."""
    u = width / h
    eeff = (er + 1) / 2 + (er - 1) / 2 / math.sqrt(1 + 12 / u)
    if u <= 1:
        z0 = ETA / (2 * math.pi * math.sqrt(eeff)) * math.log(8 / u + u / 4)
    else:
        z0 = ETA / (math.sqrt(eeff) * (u + 1.393 + 0.667 * math.log(u + 1.444)))
    v = C0 / math.sqrt(eeff)
    return z0, z0 / v, 1 / (z0 * v)


def sinc(x):
    return 1.0 if x == 0 else math.sin(math.pi * x) / (math.pi * x)


def trapezoid_lines(amplitude, duty, edge, fsw, freqs):
    """Line amplitudes of a trapezoidal train (closed form) at freqs = n fsw."""
    return np.array([2 * amplitude * duty * abs(sinc(f / fsw * duty)) * abs(sinc(f * edge))
                     for f in freqs])


def edge_transform(t, x, dt):
    """Fourier transform of a transient: |X(f)| from the transform of dx/dt over 2πf, on a
    uniform grid. Returns (freqs, complex spectrum of the derivative)."""
    grid = np.arange(t[0], t[-1], dt)
    d = np.diff(np.interp(grid, t, x)) / dt
    n = 1 << max(16, int(math.ceil(math.log2(len(d))) + 2))
    spectrum = np.fft.rfft(d, n) * dt * np.exp(-2j * math.pi * np.fft.rfftfreq(n, dt) * grid[0])
    return np.fft.rfftfreq(n, dt), spectrum


def period_lines(t, x, dt, fsw, freqs):
    """Line amplitudes of the periodic repetition of one simulated period: 2|X(n fsw)| / T."""
    f_axis, spectrum = edge_transform(t, x, dt)
    mag = np.abs(spectrum) / (2 * math.pi * np.maximum(f_axis, f_axis[1]))
    return 2 * fsw * np.interp(freqs, f_axis, mag)


def single_edge_spectrum(amplitude, edge, freqs):
    """|V(f)| of one edge of a trapezoid: V / (2πf) · |sinc(f tr)|."""
    return np.array([amplitude / (2 * math.pi * f) * abs(sinc(f * edge)) for f in freqs])


def windowed_lines(t, x, dt, freqs):
    """Amplitudes at freqs from a steady-state waveform, Hann window on a uniform grid."""
    grid = np.arange(t[0], t[-1], dt)
    y = np.interp(grid, t, x)
    w = np.hanning(len(y))
    n = 1 << int(math.ceil(math.log2(len(y))) + 3)
    spectrum = np.abs(np.fft.rfft((y - y.mean()) * w, n)) * 2 / w.sum()
    return np.interp(freqs, np.fft.rfftfreq(n, dt), spectrum)


def merged(level, spacing, bandwidth):
    """Several lines inside one receiver bandwidth add as power."""
    return level * math.sqrt(max(1.0, bandwidth / spacing))


def spread_overlap(f, spread, f0, bandwidth, spacing):
    """Fraction of one frequency-dithered harmonic captured by the receiver bandwidth."""
    if not spread:
        return 1.0 if abs(f - f0) <= max(bandwidth / 2, spacing / 2) else 0.0
    low, high = f * (1 - spread), f * (1 + spread)
    overlap = max(0.0, min(high, f0 + bandwidth / 2) - max(low, f0 - bandwidth / 2))
    return overlap / (high - low)


def converter_received_power(src, amplitudes, f0, bandwidth, distance):
    """Mean received power from fixed or triangle-dithered converter harmonics."""
    spread = src["conv"].get("spread", 0.0)
    power = 0.0
    for f, current in zip(src["freqs"], amplitudes):
        fraction = spread_overlap(f, spread, f0, bandwidth, src["conv"]["fsw"])
        if not fraction:
            continue
        at = f0 if spread else f
        e = loop_field(at, src["loop"]["area"], current, r=distance, ground=False)
        power += received_power(e, at) / 2 * fraction
    return power


def band_margins(freqs, fields, spacing):
    """Worst margin to the radiated limit per band, and the worst lines overall."""
    per_band, lines = {}, []
    for f, e in zip(freqs, fields):
        lim = radiated_limit(f)
        if lim is None or e <= 0:
            continue
        level = dbuv(merged(e, spacing, rbw_at(f)))
        margin = lim - level
        lines.append((margin, f, level, lim))
        band = next(b for b in RADIATED if b[0] <= f < b[1])
        if band not in per_band or margin < per_band[band][0]:
            per_band[band] = (margin, f, level)
    lines.sort()
    return per_band, lines


def polygon_area(points):
    return abs(sum(x1 * y2 - x2 * y1 for (x1, y1), (x2, y2) in zip(points, points[1:] + points[:1]))) / 2


def inside(point, polygon):
    x, y = point
    hit = False
    for (x1, y1), (x2, y2) in zip(polygon, polygon[1:] + polygon[:1]):
        if (y1 > y) != (y2 > y) and x < x1 + (y - y1) * (x2 - x1) / (y2 - y1):
            hit = not hit
    return hit


def segments_cross(a, b):
    def orient(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    p1, p2, p3, p4 = (a[0], a[1]), (a[2], a[3]), (b[0], b[1]), (b[2], b[3])
    return (orient(p1, p2, p3) * orient(p1, p2, p4) < 0) and (orient(p3, p4, p1) * orient(p3, p4, p2) < 0)


# ------------------------------------------------------------------------------------------
# Board

class Board:
    def __init__(self, path=None):
        path = path or run_heat.PCB
        self.data = run_heat.read_board(path)
        self.pads = {(p.ref, p.num): p for p in self.data["pads"]}
        self.refs = {}
        for p in self.data["pads"]:
            self.refs.setdefault(p.ref, []).append(p)
        # footprints without pads (mounting holes): reference and position from the file
        with open(path) as pcb:
            text = pcb.read()
        self.mounts = {}
        for fm in re.finditer(r"\n\t\(footprint \"[^\"]+\"\n", text):
            block = text[fm.start():text.find("\n\t)\n", fm.start())]
            ref = re.search(r"\(property \"Reference\" \"([^\"]+)\"", block).group(1)
            at = re.search(r"\n\t\t\(at ([-\d.]+) ([-\d.]+)", block)
            self.mounts[ref] = (float(at.group(1)), float(at.group(2)))

    def pad(self, ref, num):
        return self.pads[(ref, num)]

    def position(self, ref):
        pads = self.refs.get(ref)
        if pads:
            return sum(p.x for p in pads) / len(pads), sum(p.y for p in pads) / len(pads)
        return self.mounts[ref]

    def net_centre(self, net):
        tracks = self.net_tracks(net)
        return (sum(s[0] + s[2] for s in tracks) / (2 * len(tracks)),
                sum(s[1] + s[3] for s in tracks) / (2 * len(tracks)))

    def net_tracks(self, net):
        net = run_power.CHG.get("board_nets", {}).get(net, net)
        return [s for s in self.data["segments"] if s[6] == net]

    def net_parasitics(self, net):
        """Length, R, L and C of all tracks and vias of a net: a bound for a branching net."""
        net = run_power.CHG.get("board_nets", {}).get(net, net)
        if not any(p.net == net for p in self.data["pads"]):
            raise ValueError("no pads on net %s" % net)
        length = r = l = c = 0.0
        for x1, y1, x2, y2, w, layer, _ in self.net_tracks(net):
            seg = math.hypot(x2 - x1, y2 - y1) * 1e-3
            _, lp, cp = microstrip(w * 1e-3)
            length += seg
            r += RHO * seg / (w * 1e-3 * run_heat.CU_T[layer])
            l += lp * seg
            c += cp * seg
        vias = sum(1 for v in self.data["vias"] if v[4] == net)
        return length, r + vias * R_VIA, l + vias * L_VIA, c, vias

    def hot_loop(self, path, caps):
        """Area of the polygon through the pad centres, and the loop inductance: copper runs
        between consecutive pads of one net as microstrip, capacitor ESL, vias and package."""
        pads = [self.pad(*p) for p in path]
        points = [(p.x, p.y) for p in pads]
        area = polygon_area(points) * 1e-6
        l_copper, runs = 0.0, []
        for a, b in zip(pads, pads[1:] + pads[:1]):
            if a.net != b.net or a.ref == b.ref:
                continue
            width = max(0.5e-3, min(a.w, a.h, b.w, b.h) * 1e-3)
            run = math.dist((a.x, a.y), (b.x, b.y)) * 1e-3
            # a pour under the run: its chord across the run direction at the midpoint sets the width
            width = max(width, self.pour_width(a.net, (a.x + b.x) / 2, (a.y + b.y) / 2, b.x - a.x, b.y - a.y))
            l_copper += microstrip(width)[1] * run
            runs.append((a.net, run, width))
        esl = sum(ESL[self.cap_package(c)] for c in caps)
        return dict(area=area, l_copper=l_copper, esl=esl, l_total=l_copper + esl + L_PKG + L_VIA * len(caps),
                    runs=runs, perimeter=sum(math.dist(p, q) for p, q in zip(points, points[1:] + points[:1])) * 1e-3)

    def cap_package(self, ref):
        p = self.refs[ref][0]
        size = max(p.w, p.h)
        return "1206" if size > 1.5 else "0805" if size > 1.1 else "0603"

    def pour_width(self, net, x, y, dx, dy, layer="F", limit=5.0):
        """Chord of the net's pours on the layer through (x, y) across the direction (dx, dy), mm,
        at most limit. Zero when no pour covers the point."""
        polys = [pts for n, lay, pts in self.data["zones"] + self.data["polys"] if n == net and lay == layer]
        if not polys or not any(inside((x, y), pts) for pts in polys):
            return 0.0
        norm = math.hypot(dx, dy) or 1.0
        nx, ny = -dy / norm, dx / norm
        width = 0.0
        for sign in (1, -1):
            step = 0.1
            while step <= limit / 2 and any(inside((x + sign * nx * step, y + sign * ny * step), pts) for pts in polys):
                step += 0.1
            width += step - 0.1
        return width * 1e-3

    def copper_path(self, path):
        """Length of the copper between consecutive pads, for the snubber lead inductance."""
        pads = [self.pad(*p) for p in path]
        return sum(math.dist((a.x, a.y), (b.x, b.y)) * 1e-3 for a, b in zip(pads, pads[1:]))

    def zone_area(self, net, layer="F"):
        return sum(polygon_area(pts) for n, lay, pts in self.data["zones"] if n == net and lay == layer) * 1e-6

    def return_path_gaps(self, layer, ref_layer):
        """Signal tracks on layer over non-GND copper of ref_layer: mm over zones and crossings
        of power tracks (gap width = track width)."""
        islands = [(n, pts) for n, lay, pts in self.data["zones"] if lay == ref_layer and n != "GND"]
        power = [s for s in self.data["segments"] if s[5] == ref_layer and s[6] != "GND"]
        result = {}
        for seg in self.data["segments"]:
            x1, y1, x2, y2, w, lay, net = seg
            if lay != layer or net == "GND":
                continue
            length = math.hypot(x2 - x1, y2 - y1)
            steps = max(1, int(length / 0.1))
            over = 0
            for i in range(steps + 1):
                p = (x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps)
                if any(inside(p, pts) for n, pts in islands if n != net):
                    over += 1
            gap = sum(s[4] for s in power if s[6] != net and segments_cross(seg, s))
            crossings = sum(1 for s in power if s[6] != net and segments_cross(seg, s))
            if over or crossings:
                entry = result.setdefault(net, [0.0, 0, 0.0])
                entry[0] += length * over / (steps + 1)
                entry[1] += crossings
                entry[2] += gap
        return result



# ------------------------------------------------------------------------------------------
# Part 1: converters

def converter_runs(board, fast):
    """Per converter: the loop geometry, the ngspice cases and their line spectra."""
    sources = {}
    for name, conv in CONVERTERS.items():
        loop = board.hot_loop(conv["loop"], conv["caps"])
        if name.startswith("boost U6") and run_heat.BOOST.get("loop_area_model"):
            loop["area"] = run_heat.BOOST["loop_area_model"]
        fsw, d = conv["fsw"], conv["d"]
        n_max = int(6.5e9 / fsw)
        freqs = fsw * np.arange(1, n_max + 1)
        src = dict(conv=conv, loop=loop, freqs=freqs, cases={}, sw_area=board.zone_area(conv["sw_net"]))
        src["i_closed"] = trapezoid_lines(conv["idc"], d, EDGE_NOMINAL, fsw, freqs)
        src["v_closed"] = trapezoid_lines(conv["vbank"], d, EDGE_NOMINAL, fsw, freqs)
        coss = conv["coss"]
        cases = [("nominal", dict(tedge=EDGE_NOMINAL, coss=coss))]
        cases += [("edge %.0f ns" % (e * 1e9), dict(tedge=e, coss=coss)) for e in EDGE_CASES
                  if e != EDGE_NOMINAL]
        cases += [("Coss %.0f pF" % (c * 1e12), dict(tedge=EDGE_NOMINAL, coss=c)) for c in (coss / 2, coss * 2)]
        if conv["snubber"]:
            lsnub = microstrip(0.3e-3)[1] * board.copper_path(conv["snubber"]) + 2 * ESL["0805"]
            cases.append(("snubber R39 2.2 Ω, C36 1 nF", dict(tedge=EDGE_NOMINAL, coss=coss,
                                                               rsnub=2.2, csnub=1e-9, lsnub=lsnub)))
            src["lsnub"] = lsnub
        if not fast:
            params = []
            for _, extra in cases:
                p = dict(dir=conv["dir"], mg=conv["mg"], mb=conv["mb"], sg=conv["sg"], sb=conv["sb"],
                         vbank=conv["vbank"], idc=conv["idc"], ton=d / fsw, ron_main=conv["ron_main"],
                         ron_sync=conv["ron_sync"], lloop=loop["l_total"], cbank=conv["cbank"], esr=conv["esr"],
                         esl=conv["esl"], ic_sw=conv["vbank"] if conv["dir"] > 0 else 0.0,
                         ic_il=conv["idc"] if conv["dir"] > 0 else 0.0)
                p.update(extra)
                params.append(p)
            results = BENCH.simulate_all("switch_edge", params)
            for (label, extra), rows in zip(cases, results):
                arr = np.array(rows)
                # drop the settling of the initial conditions before the first edge
                arr = arr[arr[:, 0] >= 100e-9 - 30e-9]
                t, v, i, vb = arr[:, 0], arr[:, 1], arr[:, 2], arr[:, 3]
                v_lines = period_lines(t, v, 10e-12, fsw, freqs)
                i_lines = period_lines(t, i, 10e-12, fsw, freqs)
                src["cases"][label] = dict(v=v_lines, i=i_lines, ring=ring_of(t, v, i, vb, conv), params=extra)
        sources[name] = src
    return sources


def ring_of(t, v, i, vb, conv):
    """Ring of the loop current after the first edge (frequency from its zero crossings, peak
    relative to the settled value) and the extremes of the switch node and the bank pin."""
    t1 = 100e-9
    after = (t > t1) & (t < t1 + 60e-9)
    swing = i[after] - i[after][-1]
    crossings = np.where(np.diff(np.sign(swing)) != 0)[0]
    f_ring = None
    if len(crossings) >= 3:
        f_ring = 1 / float(np.median(np.diff(t[after][crossings])[:6]) * 2)
    return dict(f=f_ring, i_peak=float(np.max(np.abs(swing))), v_max=float(v.max()), v_min=float(v.min()),
                vb_max=float(vb.max()), vb_min=float(vb.min()))


def report_sources(report, board, sources, fast):
    report.add("## Converter hot loops and edges (ngspice, emi/switch_edge.cir)")
    report.add()
    rows = []
    for name, src in sources.items():
        loop, conv = src["loop"], src["conv"]
        runs = ", ".join("%s %.1f mm" % (net, run * 1e3) for net, run, _ in loop["runs"])
        rows.append([name, conv["point"], "%.0f kHz" % (conv["fsw"] / 1e3), "%.2f" % conv["d"],
                     "%.2f A" % conv["idc"], "%.1f mm²" % (loop["area"] * 1e6), runs,
                     "%.2f nH" % (loop["l_total"] * 1e9), "%.0f mm²" % (src["sw_area"] * 1e6)])
    report.add("Hot loop through the pad centres (%s). The loop inductance is the copper runs as "
               "microstrip over the plane %.2f mm below, plus %.1f nH capacitor ESL per 0805, "
               "%.1f nH for the package and %.1f nH per via to the plane. Operating points are the "
               "highest loop current and duty of each converter:"
               % (", ".join("%s: %s" % (n, " ".join("%s.%s" % p for p in s["conv"]["loop"]))
                            for n, s in sources.items()), H_REF * 1e3, ESL["0805"] * 1e9, L_PKG * 1e9,
                  L_VIA * 1e9))
    report.add()
    report.table(["Converter", "Point", "fSW", "Duty", "Loop current", "Loop area", "Copper runs",
                  "Loop L", "Switch-node copper"], rows)
    report.add("The boost uses the 1.5 A safety point to bound loop current. Sustained load is 0.8 A and expected "
               "peak is 1.0 A; edge resonance does not scale reliably with load current.")
    report.add()
    if fast:
        report.add("ngspice skipped (--fast): spectra are the closed-form trapezoids with %.0f ns edges."
                   % (EDGE_NOMINAL * 1e9))
        report.add()
        return
    rows = []
    for name, src in sources.items():
        for label, case in src["cases"].items():
            ring = case["ring"]
            rows.append([name.split(" (")[0], label, "%.0f MHz" % (ring["f"] / 1e6) if ring["f"] else "none",
                         "%.2f A" % ring["i_peak"], "%.2f V / %.2f V" % (ring["v_max"], ring["v_min"]),
                         "%.2f V / %.2f V" % (ring["vb_max"], ring["vb_min"])])
    report.add("One period at 10 ps per case. Ring frequency from the zero crossings of the loop current "
               "after the main switch turns on, ring current its peak relative to the settled value. Edge is "
               "the gate ramp (10 A of current slew per ramp), Coss the output capacitance of each switch "
               "(boost %.0f pF, buck %.0f pF assumed); both are swept:"
               % (CONVERTERS[BOOST_KEY]["coss"] * 1e12, COSS["buck"] * 1e12))
    report.add()
    report.table(["Converter", "Case", "Ring", "Ring current", "Switch node max / min", "Bank pin max / min"], rows)
    # closed form against the bench at the low harmonics
    rows = []
    for name, src in sources.items():
        nominal = src["cases"]["nominal"]
        for n in (1, 2, 5, 10):
            rows.append([name.split(" (")[0], "%d (%s)" % (n, mhz(src["freqs"][n - 1])),
                         "%.3f A" % src["i_closed"][n - 1], "%.3f A" % nominal["i"][n - 1],
                         "%.3f V" % src["v_closed"][n - 1], "%.3f V" % nominal["v"][n - 1]])
    report.add("Cross-check of the bench lines against the closed-form trapezoid at the low harmonics "
               "(the bench includes dead time and the diode drops, so a few percent differ):")
    report.add()
    report.table(["Converter", "Harmonic", "Loop current, closed form", "bench", "Switch node, closed form",
                  "bench"], rows)
    for name, src in sources.items():
        nominal = src["cases"]["nominal"]
        for n in (1, 2):
            ratio = nominal["i"][n - 1] / src["i_closed"][n - 1]
            report.check(0.7 < ratio < 1.3, "%s loop-current harmonic %d: bench %.3f A against closed form "
                         "%.3f A" % (name.split(" (")[0], n, nominal["i"][n - 1], src["i_closed"][n - 1]))
    report.add()


# ------------------------------------------------------------------------------------------
# Part 2: digital signals

def digital_sources(board):
    sources = []
    for net, t_drv, edges, swing, r_series, c_extra, note in DIGITAL:
        length, r, l, c, vias = board.net_parasitics(net)
        c_load = c + c_extra + C_PIN
        t_rc = 2.2 * (r_series + R_OUT) * c_load
        t_edge = math.hypot(t_drv, t_rc)
        z0 = microstrip(0.25e-3)[0]
        sources.append(dict(net=net, length=length, c=c, c_load=c_load, t_edge=t_edge, edges=edges,
                            swing=swing, z0=z0, note=note, vias=vias, l=l, r=r))
    return sources


def digital_level(src, f, bandwidth, detector="peak"):
    """Equivalent line amplitude of the loop current in one receiver bandwidth. Peak detector:
    dense edge trains as coherent lines at the edge rate, sparse ones as impulses. Mean: the
    power of a random edge train, PSD 2 fe |I(f)|² over the bandwidth."""
    v = src["swing"] / (2 * math.pi * f) * abs(sinc(f * src["t_edge"]))
    lam = C0 / f
    short = src["length"] < lam / 10
    i = v * (2 * math.pi * f * src["c_load"] if short else 1 / src["z0"])
    if detector == "peak":
        rate = max(src["edges"], 1.05 * bandwidth)
    else:
        rate = math.sqrt(src["edges"] * bandwidth)
    return 2 * i * rate, (src["length"] * H_REF * (0.5 if short else 1.0))


def report_digital(report, sources):
    report.add("## Digital signals on the carrier")
    report.add()
    rows = []
    for s in sources:
        knee = 1 / (math.pi * s["t_edge"])
        rows.append([s["net"], "%.0f mm" % (s["length"] * 1e3), "%.1f pF" % (s["c_load"] * 1e12),
                     "%.1f ns" % (s["t_edge"] * 1e9), "%.0f MHz" % (knee / 1e6),
                     "%.0f /s" % s["edges"], s["note"]])
    report.add("Edge time at the far end: driver edge (assumed: ESP32-S3 and RP2040 %.0f ns, MAX-M10S %.0f ns, "
               "TCA9534 %.0f ns, %.0f Ω output) combined with the RC of the series resistor and the track "
               "plus pin capacitance from the board. The knee 1/(π tr) is where the spectrum starts to fall "
               "at 40 dB per decade. %s"
               % (T_ESP * 1e9, T_GNSS * 1e9, T_U13 * 1e9, R_OUT, NOT_MODELLED))
    report.add()
    report.table(["Net", "Track", "Load C", "Edge", "Knee", "Edges", "Note"], rows)


# ------------------------------------------------------------------------------------------
# Part 3: radiated emissions

def radiated(report, board, sources, digital, fast):
    report.add("## Radiated emission at 3 m against CISPR 32 class B")
    report.add()
    report.add("Each loop as a small magnetic dipole (Ott, E = 2.63e-14 f² A I / r including the ground-plane "
               "reflection of the test site). Converter loop currents are the bench lines at n·fSW; the "
               "inductor leakage field uses the triangle ripple current with %.0f %% leakage of %d turns of "
               "%.0f mm². Lines closer than the receiver bandwidth (120 kHz below 1 GHz, 1 MHz above) add as "
               "power. Limits: 40 dBµV/m to 230 MHz, 47 dBµV/m to 1 GHz, average 50 dBµV/m above. Margin is "
               "limit minus level, target %.0f dB:" % (INDUCTOR_LEAK * 100, INDUCTOR_TURNS, INDUCTOR_AREA * 1e6,
                                                        MARGIN))
    report.add()
    rows, worst = [], []
    for name, src in sources.items():
        conv, loop, freqs = src["conv"], src["loop"], src["freqs"]
        cases = src["cases"] if not fast else {"closed form": dict(i=src["i_closed"])}
        for label, case in cases.items():
            fields = [loop_field(f, loop["area"], i) for f, i in zip(freqs, case["i"])]
            per_band, lines = band_margins(freqs, fields, conv["fsw"])
            cells = ["%.0f dB at %s" % (per_band[b][0], mhz(per_band[b][1])) if b in per_band else "-"
                     for b in RADIATED[:3]]
            rows.append([name.split(" (")[0] + " hot loop", label] + cells)
            worst.append((lines[0][0], name, label, lines[0]))
        # inductor leakage: triangle ripple, lines fall with 1/n²
        ripple = conv["ripple"]
        i_tri = np.array([ripple * 2 / (math.pi ** 2 * n * n * conv["d"] * (1 - conv["d"]))
                          * abs(math.sin(math.pi * n * conv["d"])) for n in range(1, len(freqs) + 1)])
        fields = [loop_field(f, INDUCTOR_TURNS * INDUCTOR_AREA * INDUCTOR_LEAK, i) for f, i in zip(freqs, i_tri)]
        per_band, lines = band_margins(freqs, fields, conv["fsw"])
        rows.append([conv["inductor"] + " leakage", "%.2f A ripple" % ripple]
                    + ["%.0f dB at %s" % (per_band[b][0], mhz(per_band[b][1])) if b in per_band else "-"
                       for b in RADIATED[:3]])
    for s in digital:
        freqs = np.geomspace(30e6, 1e9, 400)
        fields = []
        for f in freqs:
            level, area = digital_level(s, f, rbw_at(f))
            fields.append(loop_field(f, area, level))
        per_band, lines = band_margins(freqs, fields, 1e12)
        rows.append([s["net"], "%.1f ns edge" % (s["t_edge"] * 1e9)]
                    + ["%.0f dB at %s" % (per_band[b][0], mhz(per_band[b][1])) if b in per_band else "-"
                       for b in RADIATED[:3]])
        worst.append((lines[0][0], s["net"], "nominal", lines[0]))
    report.table(["Source", "Case", "30 to 230 MHz", "230 MHz to 1 GHz", "1 to 3 GHz"], rows)
    worst.sort()
    m, n, label, line = min(w for w in worst if w[2] in ("nominal", "closed form"))
    report.check(m >= MARGIN, "smallest radiated margin as modelled %.0f dB: %s, %.0f dBµV/m at %s against "
                 "%.0f dBµV/m" % (m, n, line[2], mhz(line[1]), line[3]))
    m, n, label, line = worst[0]
    if label not in ("nominal", "closed form"):
        report.add("- Sensitivity: %.0f dB margin for %s with %s (%.0f dBµV/m at %s)."
                   % (m, n, label, line[2], mhz(line[1])))
    report.add("- Digital tracks run over the solid In1 ground; their loop is track length times the "
               "%.2f mm dielectric, with the displacement current of the track capacitance below λ/10 and "
               "V/Z0 above. Impulsive sources (PPS, UART) are read with the peak detector, which overstates "
               "the quasi-peak by 20 to 40 dB." % (H_REF * 1e3))
    report.add("- Not in this estimate: common-mode currents on the antenna coax and the USB cable, the "
               "modules' own emissions (ESP32-S3 clocks and Wi-Fi, RP2040, CC1312) and the enclosure.")
    report.add()


# ------------------------------------------------------------------------------------------
# Part 4: receiver bands

def receiver_bands(report, board, sources, digital, fast):
    report.add("## Converter and digital lines in the receiver bands")
    report.add()
    rows, failures, boost_failures = [], [], []
    mounts = {m: board.mounts[m] for _, _, _, _, m in RECEIVERS}
    for rx_name, f0, bw, nf, mount in RECEIVERS:
        floor = dbm(KT * bw) + nf
        threshold = floor - MARGIN
        for name, src in sources.items():
            conv, loop, freqs = src["conv"], src["loop"], src["freqs"]
            centre = board.position(conv["inductor"])
            r = math.dist(centre, mounts[mount]) * 1e-3 + ANTENNA_OFFSET
            cases = src["cases"] if not fast else {"closed form": dict(i=src["i_closed"])}
            for label, case in cases.items():
                if label not in ("nominal", "closed form", "edge 2 ns", "snubber R39 2.2 Ω, C36 1 nF"):
                    continue
                sel = np.array([spread_overlap(f, conv.get("spread", 0.0), f0, bw, conv["fsw"]) > 0
                                for f in freqs])
                power = converter_received_power(src, case["i"], f0, bw, r)
                level = dbm(power)
                rows.append([rx_name, name.split(" (")[0] + ", " + label, "%.0f mm" % (r * 1e3),
                             "%d" % int(sel.sum()), "%.0f dBm" % level, "%.0f dBm" % threshold,
                             "%.0f dB" % (threshold - level)])
                if label in ("nominal", "closed form") and level > threshold:
                    failures.append((rx_name, name, level, threshold))
                if name.startswith("boost U6") and label in ("nominal", "closed form", "edge 2 ns") \
                        and level > threshold:
                    boost_failures.append((rx_name, name, label, level, threshold))
        worst_digital = None
        for s in digital:
            r = math.dist(board.net_centre(s["net"]), mounts[mount]) * 1e-3 + ANTENNA_OFFSET
            level, area = digital_level(s, f0, bw, "mean")
            e = loop_field(f0, area, level, r=r, ground=False)
            p = dbm(received_power(e, f0) / 2)
            if worst_digital is None or p > worst_digital[1]:
                worst_digital = (s["net"], p, r)
        rows.append([rx_name, "worst digital net: " + worst_digital[0], "%.0f mm" % (worst_digital[2] * 1e3), "-",
                     "%.0f dBm" % worst_digital[1], "%.0f dBm" % threshold, "%.0f dB" % (threshold - worst_digital[1])])
    report.add("Field of each hot loop at the antenna (free space with the near-field term; distance from "
               "the inductor centre to the mount plus %.0f mm to the antenna on the enclosure wall), received "
               "by a dipole of gain %.2f with its far-field aperture. Lines within the receiver bandwidth add "
               "as power; where the line spacing exceeds the bandwidth the nearest line counts. Triangle "
               "frequency dithering is integrated as uniform dwell across each harmonic's specified spread. Threshold: "
               "thermal noise floor kTB plus the noise figure, less %.0f dB (ADS-B 4 MHz, NF 5 dB; FLARM "
               "250 kHz, NF 5 dB; GNSS 2 MHz, NF 2 dB):" % (ANTENNA_OFFSET * 1e3, ANT_GAIN, MARGIN))
    report.add()
    report.table(["Receiver", "Source", "Distance", "Lines", "In-band power", "Threshold", "Margin"], rows)
    report.check(not failures, "converter lines in every receiver band below the threshold"
                 if not failures else "; ".join("%s: %s %.0f dBm above %.0f dBm" % (r, n.split(" (")[0], lv, th)
                                                for r, n, lv, th in failures))
    report.check(not boost_failures, "selected boost nominal and 2 ns edge cases below every receiver threshold"
                 if not boost_failures else "; ".join("%s: %s %s %.0f dBm above %.0f dBm"
                                                      % (r, n.split(" (")[0], label, lv, th)
                                                      for r, n, label, lv, th in boost_failures))
    report.add("- The ADS-B receiver's data sheet sensitivity is −85 dBm; the threshold here protects the noise "
               "floor, which is stricter. GNSS tolerates narrowband interference better than this threshold "
               "suggests, because despreading averages it. Digital nets use the mean power of a random edge "
               "train in the bandwidth.")
    report.add("- Bench item: the ADSBee reports its noise floor (trigger level); compare it with the boost and "
               "the charger running against both off, and with the snubber fitted.")
    report.add()


SPECTRUM = os.path.join(BENCHES, "spectrum_flarm_max")  # spectrum_flarm_<charger>, bind()
FLARM_CHANNELS = (868.2e6, 868.4e6)
SPECTRUM_COLORS = {"boost U6": "#d9534f", "buck U4": "#337ab7", "digital": "#777777"}


def flarm_spectrum(board, sources, digital):
    """Received power at the FLARM mount per converter line (nominal bench case) and of the worst
    digital net, as a wide view and a zoom on the channels."""
    _, f0, bw, nf, mount = next(r for r in RECEIVERS if r[0].startswith("FLARM"))
    ant = board.mounts[mount]
    floor = dbm(KT * bw) + nf
    combs = {}
    for name, src in sources.items():
        conv, loop, freqs = src["conv"], src["loop"], src["freqs"]
        r = math.dist(board.position(conv["inductor"]), ant) * 1e-3 + ANTENNA_OFFSET
        p = [dbm(received_power(loop_field(f, loop["area"], i, r=r, ground=False), f) / 2)
             for f, i in zip(freqs, src["cases"]["nominal"]["i"])]
        combs[name.split(" (")[0]] = (freqs, np.array(p), conv["fsw"])
    grid = np.logspace(6, math.log10(2e9), 400)
    worst = np.full_like(grid, -300.0)
    for s in digital:
        r = math.dist(board.net_centre(s["net"]), ant) * 1e-3 + ANTENNA_OFFSET
        for k, f in enumerate(grid):
            level, area = digital_level(s, f, bw, "mean")
            worst[k] = max(worst[k], dbm(received_power(loop_field(f, area, level, r=r, ground=False), f) / 2))

    fig = simlib.Figure(900, 900)
    left, right, top, height, ymin, ymax = 70, 880, 40, 340, -170, -40

    def axes(y0, title, fx, flo, fhi, logx):
        x = (lambda f: left + math.log10(f / flo) / math.log10(fhi / flo) * (right - left)) if logx else \
            (lambda f: left + (f - flo) / (fhi - flo) * (right - left))
        yy = lambda v: y0 + (ymax - min(max(v, ymin), ymax)) / (ymax - ymin) * height
        fig.text(left, y0 - 12, title, 13, bold=True)
        for v in range(ymin, ymax + 1, 10):
            fig.line([(left, yy(v)), (right, yy(v))], "#eeeeee")
            fig.text(left - 5, yy(v) + 4, simlib.minus("%d" % v), 10, anchor="end")
        for f, label in fx:
            fig.line([(x(f), y0), (x(f), y0 + height)], "#eeeeee")
            fig.text(x(f), y0 + height + 14, label, 10, anchor="middle")
        fig.text(right, y0 + height + 30, "MHz", 10, anchor="end")
        fig.text(22, y0 + height / 2, "dBm in %.0f kHz at %s" % (bw / 1e3, mount), 10, anchor="middle", rotate=True)
        return x, yy

    def levels(x, yy, y0, at_left=False):
        for v, label, color in ((floor, "receiver floor %d dBm" % round(floor), "#444444"),
                                (floor - MARGIN, "threshold %d dBm" % round(floor - MARGIN), "#e08a00")):
            fig.line([(left, yy(v)), (right, yy(v))], color, dash=(5, 3))
            fig.text(left + 4 if at_left else right - 4, yy(v) - 4, simlib.minus(label), 10, color,
                     anchor="start" if at_left else "end")
        fig.rect(left, y0, right - left, height, stroke="#999999")

    x, yy = axes(top, "Converter lines at the FLARM antenna, 1 MHz to 2 GHz",
                 [(f, "%g" % (f / 1e6)) for f in (1e6, 3e6, 10e6, 30e6, 100e6, 300e6, 1e9, 2e9)], 1e6, 2e9, True)
    for f, label, dy in ((f0, "FLARM", 12), (1090e6, "ADS-B", 26), (1575.42e6, "GNSS", 12)):
        fig.line([(x(f), top), (x(f), top + height)], "#a8d8a8", 2)
        fig.text(x(f) - 3, top + dy, label, 10, "#3c8c3c", anchor="end")
    for name, (freqs, p, _) in combs.items():
        color = SPECTRUM_COLORS.get(name, "#d9534f" if name.startswith("boost U6") else "#337ab7")
        fig.line([(x(f), yy(v)) for f, v in zip(freqs, p) if 1e6 <= f <= 2e9], color, 0.8)
    fig.line([(x(f), yy(v)) for f, v in zip(grid, worst)], SPECTRUM_COLORS["digital"], 1.5)
    levels(x, yy, top, at_left=True)

    zlo, zhi, y0 = 864e6, 872e6, top + height + 80
    x, yy = axes(y0, "Zoom on the FLARM channels at nominal fSW. The lines move with fSW tolerance and, for U6, with VIN.",
                 [(f, "%g" % (f / 1e6)) for f in np.arange(zlo, zhi + 1, 1e6)], zlo, zhi, False)
    for ch in FLARM_CHANNELS:
        fig.rect(x(ch - bw / 2), y0, x(ch + bw / 2) - x(ch - bw / 2), height, fill="#dff0df")
    fig.text(x(sum(FLARM_CHANNELS) / 2), y0 + 12, "868.2 / 868.4", 10, "#3c8c3c", anchor="middle")
    for name, (freqs, p, _) in combs.items():
        color = SPECTRUM_COLORS.get(name, "#d9534f" if name.startswith("boost U6") else "#337ab7")
        for f, v in zip(freqs, p):
            if zlo <= f <= zhi:
                fig.line([(x(f), yy(ymin)), (x(f), yy(v))], color, 2)
                fig.circle(x(f), yy(v), 3, color)
    levels(x, yy, y0)

    legend = [(SPECTRUM_COLORS.get(n, "#d9534f" if n.startswith("boost U6") else "#337ab7"),
               "%s, lines every %.0f kHz" % (n, c[2] / 1e3)) for n, c in combs.items()]
    legend.append((SPECTRUM_COLORS["digital"], "worst digital net, mean in %.0f kHz" % (bw / 1e3)))
    for k, (color, label) in enumerate(legend):
        fig.rect(left + k * 260, 872, 14, 4, fill=color)
        fig.text(left + k * 260 + 20, 878, label, 11)
    return fig


# ------------------------------------------------------------------------------------------
# Part 5: conducted emission on USB

def conducted(report, board, fast):
    report.add("## Conducted emission on the USB input (ngspice, emi/usb_lisn.cir)")
    report.add()
    if fast:
        report.add("Skipped (--fast).")
        report.add()
        return
    length, r_tr, l_tr, _, vias = board.net_parasitics("USB_VBUS")
    conv = CONVERTERS[BUCK_KEY]
    fb = lambda l: dict(r_fb=FB1[0], l_fb=l, rp_fb=FB1[2], r_in=run_power.CHG["r_in"])
    cases = [("FB1 %.1f µH, %.0f ns edges" % (FB1[1] * 1e6, EDGE_NOMINAL * 1e9), dict(tedge=EDGE_NOMINAL, **fb(FB1[1]))),
             ("FB1 %.1f µH, 2 ns edges" % (FB1[1] * 1e6), dict(tedge=2e-9, **fb(FB1[1])))]
    cases += [("FB1 %.1f µH (DC bias)" % (l * 1e6), dict(tedge=EDGE_NOMINAL, **fb(l))) for l in FB1_L_BIAS]
    cases += [("FB1 fitted as 0 Ω", dict(tedge=EDGE_NOMINAL)),
              ("before FB1: 0 Ω, C1 1 µF", dict(tedge=EDGE_NOMINAL, c1=0.6e-6)),
              ("C33 missing (BYP 6 µF)", dict(tedge=EDGE_NOMINAL, cbyp=6e-6, esl_byp=0.5e-9, **fb(FB1[1]))),
              ("0 Ω, adapter 470 µF / 30 mΩ across the LISN", dict(tedge=EDGE_NOMINAL, c_adp=470e-6, esr_adp=30e-3))]
    designed = [c[0] for c in cases[:2 + len(FB1_L_BIAS)]]
    params = [dict(fsw=conv["fsw"], d=conv["d"], ipk=conv["idc"], r_tr=r_tr, l_tr=l_tr, r_cab=USB_CABLE[0],
                   l_cab=USB_CABLE[1], **extra) for _, extra in cases]
    results = BENCH.simulate_all("usb_lisn", params)
    harmonics = conv["fsw"] * np.arange(1, int(30e6 / conv["fsw"]) + 1)
    rows, worst = [], []
    for (label, _), rows_ in zip(cases, results):
        arr = np.array(rows_)
        t = arr[:, 0]
        v_m = windowed_lines(t, arr[:, 1], 1e-9, harmonics)
        v_byp = windowed_lines(t, arr[:, 2], 1e-9, harmonics)
        margins = [(conducted_limit(f) - dbuv(v), f, dbuv(v), dbuv(b)) for f, v, b in zip(harmonics, v_m, v_byp)]
        margins.sort()
        m, f, level, byp = margins[0]
        rows.append([label, "%.1f mV" % (1e3 * v_byp[0]), "%.1f dBµV at %s" % (dbuv(v_m[0]), mhz(harmonics[0])),
                     "%.1f dBµV at %s" % (level, f and mhz(f)), "%.0f dBµV" % conducted_limit(f), "%.0f dB" % m])
        worst.append((m, label, f, level))
    report.add("Buck input current %.1f A at duty %.2f and %.0f kHz from BYP; USB_VBUS copper %.0f mm, %.1f mΩ, "
               "%.0f nH with %d vias from the board; USB cable %.1f Ω, %.1f µH; LISN 50 µH / 50 Ω. Steady state, "
               "last 100 µs, Hann window, lines at the harmonics. The quasi-peak limit applies to the mains port; "
               "an adapter's own filter is not in the model, so this is the worst case:"
               % (conv["idc"], conv["d"], conv["fsw"] / 1e3, length * 1e3, r_tr * 1e3, l_tr * 1e9, vias,
                  USB_CABLE[0], USB_CABLE[1] * 1e6))
    report.add()
    report.table(["Case", "BYP ripple (fSW)", "LISN at fSW", "Worst line", "Limit (QP)", "Margin"], rows)
    m, label, f, level = min(w for w in worst if w[1] in designed)
    report.check(m >= MARGIN, "conducted margin with FB1, worst inductance %.0f dB (%s, %.0f dBµV at %s "
                 "against the LISN)" % (m, label, level, mhz(f)))
    report.add("- FB1's inductance at %.2g MHz and up to 3 A is an estimate; the rows with %s show how the margin "
               "follows it. Murata's DC-bias curve for BLM31KN471SZ1L decides; it is not on file."
               % (conv["fsw"] / 1e6, " and ".join("%.1f µH" % (l * 1e6) for l in FB1_L_BIAS)))
    report.add("- The LISN is the CISPR 32 method for a DC power port and the worst case for the mains port. "
               "A real adapter's output capacitor absorbs most of the cable current (last row); its mains-side "
               "filter is designed for its own switching frequency, not for %.2g MHz. Average limits are "
               "10 dB lower; steady lines read the same on both detectors. Common-mode current through the "
               "adapter's parasitic capacitance is not modelled." % (conv["fsw"] / 1e6))
    report.add()


# ------------------------------------------------------------------------------------------
# Part 6: return paths

def return_paths(report, board):
    report.add("## Return paths under signal tracks")
    report.add()
    rows, total = [], 0.0
    for layer, ref in (("F", "In1"), ("B", "In2")):
        gaps = board.return_path_gaps(layer, ref)
        for net, (over, crossings, gap) in sorted(gaps.items(), key=lambda kv: -kv[1][0] - kv[1][2]):
            rows.append([layer + ".Cu", net, "%.1f mm" % over, "%d" % crossings, "%.1f mm" % gap])
            if net in [d[0] for d in DIGITAL]:
                total += over + gap
    in1 = [n for n, lay, _ in board.data["zones"] if lay == "In1" and n != "GND"]
    report.add("Top tracks reference In1, bottom tracks In2, both %.2f mm away. Listed: track length over "
               "non-GND zone copper of the reference layer and crossings of its power tracks (the gap is the "
               "power track width). In1 carries %s."
               % (H_REF * 1e3, "only GND" if not in1 else "also " + ", ".join(sorted(set(in1)))))
    report.add()
    if rows:
        report.table(["Layer", "Net", "Over power copper", "Track crossings", "Gap"], rows)
    else:
        report.add("No signal track runs over power copper of its reference layer.")
        report.add()
    report.check(total < 5.0, "digital tracks over return-path gaps: %.1f mm in total, below 5 mm" % total)
    report.add()


# ------------------------------------------------------------------------------------------
# Part 7: rails and harness

def rails(report, board, fast):
    report.add("## Rail ripple at the module sockets and the battery harness loop")
    report.add()
    rows = []
    for net, dest in (("RAIL_5V", "U11/U12 inputs"), ("ESP_5V", "J7.1"), ("ADS_5V", "J10.1"), ("VSYS", "L2, J3"),
                      ("VBAT", "J3")):
        length, r, l, c, vias = board.net_parasitics(net)
        rows.append([net, dest, "%.0f mm" % (length * 1e3), "%.1f mΩ" % (r * 1e3), "%.0f nH" % (l * 1e9),
                     "%.0f pF" % (c * 1e12), "%d" % vias])
    report.add("Track parasitics from the board (all segments of the net, a bound for branching nets; "
               "%.1f nH and %.1f mΩ per via):" % (L_VIA * 1e9, R_VIA * 1e3))
    report.add()
    report.table(["Net", "To", "Length", "R", "L", "C", "Vias"], rows)
    if fast:
        report.add("Benches skipped (--fast).")
        report.add()
        return
    boost = CONVERTERS[BOOST_KEY]
    buck = CONVERTERS[BUCK_KEY]
    runs = [("boost_ripple", dict(vin=BOOST_VIN, iout=BOOST_IOUT, fsw=boost["fsw"], cout=boost["cbank"],
                                  c24=run_power.C24 * run_power.C24_BIAS,
                                  ron_ls=boost["ron_main"], ron_hs=boost["ron_sync"], coss=boost["coss"],
                                  ron_u7=run_heat.BOOST["load_switch_ron"] or 0.1e-3,
                                  eff=run_heat.BOOST["emi_efficiency"])),
            ("buck_ripple", dict(vsys=BUCK_VSYS, iload=BUCK_ILOAD, **run_power.buck_bench_params()))]
    results = [run_power.BENCH.simulate(bench, **p) for bench, p in runs]
    # socket ripple: RAIL_5V lines through the load switch and copper into the module capacitance
    arr = np.array(results[0])
    t = arr[:, 0]
    keep = t >= t[-1] - 40e-6
    f_lines = boost["fsw"] * np.arange(1, 141)
    rail = windowed_lines(t[keep], arr[keep, 2], 1e-9, f_lines)
    vsys_boost = windowed_lines(t[keep], arr[keep, 3], 1e-9, f_lines)
    _, r_rail, l_rail, _, _ = board.net_parasitics("RAIL_5V")
    _, r_esp, l_esp, _, _ = board.net_parasitics("ESP_5V")
    rows = []
    for n in (1, 2, 3):
        f = f_lines[n - 1]
        z_path = complex(r_rail + r_esp + run_power.ID_RON[0], 2 * math.pi * f * (l_rail + l_esp))
        z_cap = complex(0, -1 / (2 * math.pi * f * C_MODULE))
        v_socket = rail[n - 1] * abs(z_cap / (z_path + z_cap))
        rows.append(["%d (%s)" % (n, mhz(f)), "%.1f mV" % (1e3 * rail[n - 1]),
                     "%.0f mA" % (1e3 * rail[n - 1] / abs(z_path + z_cap)), "%.1f mV" % (1e3 * v_socket)])
    report.add("Boost at %s: RAIL_5V lines from power/boost_ripple.cir, driven through the LM73100 (%.0f mΩ) "
               "and the RAIL_5V and ESP_5V copper into %.0f µF module input capacitance:"
               % (boost["point"], run_power.ID_RON[0] * 1e3, C_MODULE * 1e6))
    report.add()
    report.table(["Harmonic", "RAIL_5V", "Current into the module", "At the socket"], rows)
    # battery harness loop: VSYS lines drive current through the pack leads
    rows, worst = [], None
    arr_b = np.array(results[1])
    tb = arr_b[:, 0]
    keep_b = tb >= tb[-1] - 40e-6
    f_buck = buck["fsw"] * np.arange(1, 77)
    vsys_buck = windowed_lines(tb[keep_b], arr_b[keep_b, 1], 1e-9, f_buck)
    area = HARNESS["length"] * HARNESS["spacing"]
    for label, freqs, lines in (("boost, battery", f_lines, vsys_boost), ("buck, charging", f_buck, vsys_buck)):
        best = None
        for f, v in zip(freqs, lines):
            if f < 30e6 or f > 100e6:
                continue
            i = v / abs(complex(HARNESS["r"], 2 * math.pi * f * HARNESS["l"]))
            e = loop_field(f, area, i)
            level = dbuv(merged(e, freqs[0], rbw_at(f)))
            margin = radiated_limit(f) - level
            if best is None or margin < best[0]:
                best = (margin, f, v, i, level)
        rows.append([label, "%.1f mV" % (1e3 * lines[0]), mhz(best[1]), "%.1f µV" % (1e6 * best[2]),
                     "%.1f µA" % (1e6 * best[3]), "%.0f dBµV/m" % best[4], "%.0f dB" % best[0]])
        if worst is None or best[0] < worst[0]:
            worst = best
    report.add("VSYS ripple lines (benches, raw node, valid to about 100 MHz at their time step) drive the "
               "pack leads: %.0f mm untwisted pair %.0f mm apart (%.0f mm² loop), %.0f mΩ and %.0f nH in the "
               "path. Worst line between 30 and 100 MHz against the radiated limit:"
               % (HARNESS["length"] * 1e3, HARNESS["spacing"] * 1e3, area * 1e6, HARNESS["r"] * 1e3,
                  HARNESS["l"] * 1e9))
    report.add()
    report.table(["Source", "VSYS ripple (fSW)", "Worst line", "VSYS line", "Lead current", "Field at 3 m",
                  "Margin"], rows)
    report.check(worst[0] >= MARGIN, "battery harness loop margin %.0f dB at %s" % (worst[0], mhz(worst[1])))
    report.add("- The carrier's own VSYS copper adds %.0f mm over the plane, a far smaller loop. Twisting the "
               "pack leads removes most of the harness area." % (board.net_parasitics("VSYS")[0] * 1e3))
    report.add()


# ------------------------------------------------------------------------------------------

bind()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--write", action="store_true", help="store the report in emi/results_<charger>.md")
    parser.add_argument("--fast", action="store_true", help="skip the ngspice benches")
    parser.add_argument("--boost", choices=boosts.PROFILES, default=boosts.DEFAULT,
                        help="boost converter to model without changing the board")
    run_heat.project.add_argument(parser)
    args = parser.parse_args()
    if args.project:
        run_heat.select_project(args.project)
    run_heat.select_boost(args.boost)
    bind()
    version = "closed form only" if args.fast else simlib.require_ngspice()
    board = Board()
    report = simlib.Report()
    report.add("# EMI estimate, simulation results%s" %
               ("" if run_heat.BOOST_NAME == boosts.DEFAULT else ", %s candidate" % run_heat.BOOST["part"]))
    report.add()
    report.add("Generated by `hardware/sim/run_emi.py` with %s. Board geometry from `carrier.kicad_pcb`. "
               "See `hardware/sim/README.md` for what is and is not modelled. These are order-of-magnitude "
               "pre-compliance estimates, not measurements." % version)
    report.add()
    if run_heat.BOOST["layout_note"]:
        report.add("Candidate assumption: %s PFM timing is unpublished and is not represented by the fixed-frequency model."
                   % run_heat.BOOST["layout_note"])
        report.add()
    sources = converter_runs(board, args.fast)
    report_sources(report, board, sources, args.fast)
    digital = digital_sources(board)
    report_digital(report, digital)
    report.add()
    radiated(report, board, sources, digital, args.fast)
    receiver_bands(report, board, sources, digital, args.fast)
    conducted(report, board, args.fast)
    return_paths(report, board)
    rails(report, board, args.fast)
    if args.write and not args.fast:
        fig = flarm_spectrum(board, sources, digital)
        fig.svg(SPECTRUM + ".svg")
        fig.pdf(SPECTRUM + ".pdf")
    return report.finish(RESULTS if args.write else None)


if __name__ == "__main__":
    sys.exit(main())
