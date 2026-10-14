#!/usr/bin/env python3
"""Power converter verification for the carrier: TPS61088 boost loop, switching ripple of
the boost and of the MAX77757 buck, and switch-node coupling into sensitive copper.

    python3 hardware/sim/run_power.py            # print the report
    python3 hardware/sim/run_power.py --write    # also store power/results.md
    python3 hardware/sim/run_power.py --fast     # skip the ngspice ripple benches

Three parts:
  1. Loop gain of the TPS61088 with the fitted compensation (R18, C21, C22), from the
     small-signal model in the data sheet (SLVSCM8D, Equations 13 to 20): crossover,
     phase margin and gain margin over input voltage, load and output capacitance.
     Closed form, no simulator. The MAX77757 buck loop is internal and undocumented.
  2. Switching ripple from ngspice benches of the two power stages (power/*.cir) with
     the real inductors and derated capacitors.
  3. Distances between the switch-node copper and the sensitive nets on the board,
     read from hardware/carrier-max-charger/carrier.kicad_pcb, with an estimate of the capacitive
     coupling. Magnetic coupling from the shielded inductors is bounded analytically.

Requires ngspice for part 2. Only the standard library is used.
"""

import argparse
import itertools
import json
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import chargers  # noqa: E402
import project  # noqa: E402
import simlib  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
BENCHES = os.path.join(HERE, "power")
RESULTS = os.path.join(BENCHES, "results_max.md")  # results_<charger>.md, bind_charger

# --- TPS61088, data sheet SLVSCM8D
VREF = 1.204
GM_EA = 190e-6  # error amplifier transconductance
R_EA = 10e6  # error amplifier output resistance, not specified: assumed
RSENSE = 0.08  # equivalent current sense resistor
C_FREQ, T_DELAY = 23e-12, 89e-9  # frequency setting
ILIM_MIN_DROP = 1.3  # worst-case current limit is 1.3 A below the equation

# --- boost_switch sheet
R14, R15 = 240e3, 75e3
VOUT = VREF * (1 + R14 / R15)
R16, R17 = 220e3, 160e3  # frequency setting (about 720 kHz) and current limit
R18, C21, C22 = 6.8e3, 10e-9, 100e-12  # compensation: R5, C5, C8 in the data sheet
C21_TOL = 0.10  # X7R
L2, L2_DCR, L2_ISAT = 2.2e-6, 15.5e-3, 10.0  # CKST0603-2.2uH/M
L2_TOL = 0.20  # suffix M, CENKER CKST series data sheet
EFFICIENCY = 0.9
# Output capacitance seen by the loop. C18, C19, C20: Murata GRM21BZ71A226ME15L, 22 uF +-20 %
# X7R 10 V 0805 (datasheets/murata-grm21bz71a226.pdf): DC bias -52 % at 5 V. The -10 % at
# 85 C and the -8 % at small AC signal against the 0.5 Vrms of the measurement are X5R
# figures, kept as the conservative side (Murata's AC curve shows -5 % at 0.1 Vrms). C24 10 uF
# +-10 % X7R 25 V (CL21B106KAYQNNE) sits behind the load switch; its DC bias loss at 5 V is
# assumed -40 % (no curve on file). The modules add their own input capacitance, which is unknown.
C_MAIN, C_MAIN_TOL, C_MAIN_BIAS = 3 * 22e-6, 0.20, 0.48
C24, C24_TOL, C24_BIAS = 10e-6, 0.10, 0.60
C_TEMP, C_AC = 0.90, 0.92
C_MODULES = 40e-6
C_INPUT = 100e-9  # C39/C38, each module-feed input bypass
# LM73100 SNOSDC0A: typical and maximum over -40 to 125 C, as in run_heat.py.
ID_NAME = "LM73100"
ID_RON = (28.4e-3, 44.85e-3)
ID_VFWD = (16.4e-3, 28.4e-3)
ID_CORNERS = {name: dict(id_ron=ron, id_vfwd=vfwd)
              for name, ron, vfwd in zip(("typical", "maximum"), ID_RON, ID_VFWD)}
with open(os.path.join(HERE, "../../specs/power_params.json")) as source:
    _params = json.load(source)
_ads = next(load for load in _params["loads_5v"] if load["name"].startswith("ADSBee"))
_gnss = _params["gnss"]
_ads_ma = _ads["wifi_on_ma"] + sum(_gnss[k] for k in
                                     ("module_tracking_ma", "breakout_led_ma", "active_antenna_ma"))
_esp_ma = sum(load["ma"] for load in _params["loads_5v"] if load["name"].startswith("Display"))
ADS_SHARE = _ads_ma / (_ads_ma + _esp_ma)  # same Wi-Fi-on load split as run_heat.py



def cout_case(tol, temp=1.0, ac=1.0, modules=0.0):
    """Effective output capacitance for a tolerance sign (-1, 0, +1) and derating factors."""
    return (C_MAIN * (1 + tol * C_MAIN_TOL) * C_MAIN_BIAS
            + C24 * (1 + tol * C24_TOL) * C24_BIAS) * temp * ac + modules


COUT_VALUES = (("minimum", cout_case(-1, C_TEMP, C_AC)), ("nominal", cout_case(0)),
               ("maximum", cout_case(1)), ("maximum plus modules", cout_case(1, modules=C_MODULES)))
COUT_CASES = {"%s (%.0f µF)" % (label, c / 1e-6): c for label, c in COUT_VALUES}
COUT_ESR = 1e-3
L_CASES = {"L −20 %": L2 * (1 - L2_TOL), "L nominal": L2, "L +20 %": L2 * (1 + L2_TOL)}
C21_CASES = {"C21 −10 %": C21 * (1 - C21_TOL), "C21 nominal": C21, "C21 +10 %": C21 * (1 + C21_TOL)}
VIN_CASES = (3.1, 3.7, 4.4)
IOUT_CASES = (0.05, 0.2, 0.8, 1.0, 1.5)
IOUT_DESIGN = 1.0  # loads above this are short-term headroom, not design points
PM_MIN, GM_MIN = 45.0, 10.0

# --- charger buck (chargers.py, bound by bind_charger; run_heat.select_project calls it)
BUCK_FSW = 1.3e6
CHG = None


def bind_charger(tag=None):
    """Charger buck constants and the battery path copper of the project's board."""
    global BUCK_FSW, CHG, R_B2S, R_BOARD, R_SWITCH_BOARD, R_BOARD_SOURCE, RESULTS
    global ID_NAME, ID_RON, ID_VFWD, ID_CORNERS
    tag = tag or project.charger(project.resolve())
    ID_NAME = "TPS259474L" if tag == "ti" else "LM73100"
    ID_RON = (28.2e-3, 45e-3) if tag == "ti" else (28.4e-3, 44.85e-3)
    # TPS25947 specifies no maximum VFWD; retain 28.4 mV as a modelling assumption.
    ID_VFWD = (16.9e-3, 28.4e-3) if tag == "ti" else (16.4e-3, 28.4e-3)
    ID_CORNERS = {name: dict(id_ron=ron, id_vfwd=vfwd)
                  for name, ron, vfwd in zip(("typical", "maximum"), ID_RON, ID_VFWD)}
    CHG = chargers.CHARGERS[tag]
    RESULTS = os.path.join(BENCHES, "results_%s.md" % tag)
    BUCK_FSW = CHG["fsw"]
    R_B2S = CHG["r_batfet"]
    path = os.path.join(HERE, "heat", tag, "calibration.json")
    if os.path.exists(path):
        with open(path) as f:
            cal = json.load(f)
        R_BOARD = sum(cal["r_board"].values())
        R_BOARD_SOURCE = "heat/%s/calibration.json%s" % (tag, ", unrouted nets assumed" if cal["unrouted"] else "")
    else:
        R_BOARD = 1.3e-3 + 4.6e-3 + 6.2e-3  # MAX77757 board, heat/max/results.md
        R_BOARD_SOURCE = "literal, no calibration file"
    R_SWITCH_BOARD = R_B2S + R_BOARD + CHG.get("ship_fet", {}).get("r_on", (0.0,))[0]

BENCH = simlib.Bench(BENCHES)


def fsw_boost(vin):
    return 1 / (R16 * C_FREQ / 4 + T_DELAY * VOUT / vin)


def ilim_boost():
    """Peak switch current limit in forced PWM mode, typical and worst-case minimum."""
    typical = 1190000 / R17 - 1.6
    return typical, typical - ILIM_MIN_DROP


class BoostLoop:
    """Loop gain T(f) = Gps(f) * Gc(f) from data sheet Equations 13 to 17."""

    def __init__(self, vin, iout, cout, l=L2, c21=C21):
        self.d = 1 - EFFICIENCY * vin / VOUT
        self.ro = VOUT / iout
        self.cout = cout
        self.f_p = 2 / (2 * math.pi * self.ro * cout)
        self.f_esrz = 1 / (2 * math.pi * COUT_ESR * cout)
        self.f_rhpz = self.ro * (1 - self.d) ** 2 / (2 * math.pi * l)
        self.f_comp1 = 1 / (2 * math.pi * R_EA * c21)
        self.f_comz = 1 / (2 * math.pi * R18 * c21)
        self.f_comp2 = 1 / (2 * math.pi * R18 * C22)
        self.gps0 = self.ro * (1 - self.d) / (2 * RSENSE)
        self.gc0 = GM_EA * R_EA * VREF / VOUT
        self.fsw = fsw_boost(vin)

    def gain_db(self, f):
        s = 1j * 2 * math.pi * f
        gps = self.gps0 * (1 + s / (2 * math.pi * self.f_esrz)) * (1 - s / (2 * math.pi * self.f_rhpz)) \
            / (1 + s / (2 * math.pi * self.f_p))
        gc = self.gc0 * (1 + s / (2 * math.pi * self.f_comz)) \
            / ((1 + s / (2 * math.pi * self.f_comp1)) * (1 + s / (2 * math.pi * self.f_comp2)))
        return 20 * math.log10(abs(gps * gc))

    def phase_deg(self, f):
        lead = math.atan(f / self.f_esrz) + math.atan(f / self.f_comz)
        lag = (math.atan(f / self.f_rhpz) + math.atan(f / self.f_p)
               + math.atan(f / self.f_comp1) + math.atan(f / self.f_comp2))
        return math.degrees(lead - lag)

    def crossover_limit(self):
        """Highest crossover the data sheet allows: the lower of fSW/10 and RHPZ/5."""
        return min(self.fsw / 10, self.f_rhpz / 5)

    def crossover(self):
        return self._solve(self.gain_db, 0.0)

    def phase_margin(self, fc):
        return 180 + self.phase_deg(fc)

    def gain_margin(self):
        f180 = self._solve(self.phase_deg, -180.0)
        return None if f180 is None else -self.gain_db(f180)

    @staticmethod
    def _solve(func, level, lo=1.0, hi=1e7):
        """First downward crossing of func through level on a log grid, then bisection."""
        points = [lo * (hi / lo) ** (i / 600) for i in range(601)]
        for a, b in zip(points, points[1:]):
            if func(a) > level >= func(b):
                for _ in range(60):
                    mid = math.sqrt(a * b)
                    if func(mid) > level:
                        a = mid
                    else:
                        b = mid
                return math.sqrt(a * b)
        return None

    def recommended(self, fc):
        """R5, C5, C8 from data sheet Equations 18 to 20 for a crossover fc."""
        r5 = 2 * math.pi * VOUT * RSENSE * fc * self.cout / ((1 - self.d) * VREF * GM_EA)
        return r5, self.ro * self.cout / (2 * r5), COUT_ESR * self.cout / r5


def khz(f):
    return "none" if f is None else "%.1f kHz" % (f / 1e3)


def db(gain):
    """Gain margin text; None means the phase never reaches −180° below 10 MHz."""
    return "> 40 dB" if gain is None else "%.0f dB" % gain


def boost_loop(report):
    report.add("## TPS61088 boost loop")
    report.add()
    report.add("Small-signal model from the data sheet: constant off-time, peak current "
               "control, Rsense 0.08 Ω, GEA 190 µA/V. Fitted compensation R18 %.1f kΩ, "
               "C21 %.0f nF, C22 %.0f pF, with L2 %.1f µH. VOUT %.2f V, switching frequency "
               "%.2f to %.2f MHz over 3.1 to 4.4 V (R16 %.0f kΩ). The error amplifier output "
               "resistance is not specified. %.0f MΩ is assumed; it sets only the DC gain."
               % (R18 / 1e3, C21 / 1e-9, C22 / 1e-12, L2 / 1e-6, VOUT, fsw_boost(3.1) / 1e6,
                  fsw_boost(4.4) / 1e6, R16 / 1e3, R_EA / 1e6))
    report.add()
    report.add("MODE is tied to GND (forced PWM), so the converter stays in continuous conduction "
               "at every load. The CCM model therefore applies down to the %.2f A light-load case. Not in "
               "the model: the current-loop sampling poles at fSW/2 (a few degrees at these "
               "crossovers), slope compensation and the error amplifier bandwidth."
               % IOUT_CASES[0])
    report.add()
    report.add("Nominal parts (L2 %.1f µH, C21 %.0f nF, output capacitance %.0f µF):"
               % (L2 / 1e-6, C21 / 1e-9, COUT_VALUES[1][1] / 1e-6))
    report.add()
    table = []
    for vin, iout in itertools.product(VIN_CASES, IOUT_CASES):
        loop = BoostLoop(vin, iout, COUT_VALUES[1][1])
        fc = loop.crossover()
        table.append(["%.1f V" % vin, "%.2f A" % iout, khz(fc), khz(loop.crossover_limit()),
                      khz(loop.f_rhpz), "%.0f°" % loop.phase_margin(fc), db(loop.gain_margin())])
    report.table(["VIN", "IOUT", "Crossover", "Limit (fSW/10, RHPZ/5)", "RHP zero", "Phase margin",
                  "Gain margin"], table)
    report.add()

    corners = list(itertools.product(COUT_CASES.items(), L_CASES.items(), C21_CASES.items()))
    report.add("Worst case over %d part corners per operating point: output capacitance %s; %s; %s. "
               "The corner in brackets is where the worst value occurs:"
               % (len(corners), ", ".join(COUT_CASES), ", ".join(L_CASES), ", ".join(C21_CASES)))
    report.add()
    table = []
    worst_pm, worst_gm, worst_ratio, worst_head = (999.0, ""), (999.0, ""), (0.0, ""), (0.0, "")
    for vin, iout in itertools.product(VIN_CASES, IOUT_CASES):
        pm_min, gm_min, ratio_max, fc_lo, fc_hi = (999.0, ""), (999.0, ""), (0.0, ""), 1e9, 0.0
        for (clabel, cout), (llabel, l), (c21label, c21) in corners:
            loop = BoostLoop(vin, iout, cout, l, c21)
            fc = loop.crossover()
            pm = loop.phase_margin(fc)
            gm = loop.gain_margin()
            ratio = fc / loop.crossover_limit()
            corner = "%s, %s, %s" % (clabel, llabel, c21label)
            pm_min = min(pm_min, (pm, corner))
            gm_min = min(gm_min, (gm if gm is not None else 999.0, corner))
            ratio_max = max(ratio_max, (ratio, corner))
            fc_lo, fc_hi = min(fc_lo, fc), max(fc_hi, fc)
        point = " at %.1f V, %.2f A, %s" % (vin, iout, "%s")
        worst_pm = min(worst_pm, (pm_min[0], point % pm_min[1]))
        worst_gm = min(worst_gm, (gm_min[0], point % gm_min[1]))
        if iout <= IOUT_DESIGN:
            worst_ratio = max(worst_ratio, (ratio_max[0], point % ratio_max[1]))
        else:
            worst_head = max(worst_head, (ratio_max[0], point % ratio_max[1]))
        table.append(["%.1f V" % vin, "%.2f A" % iout, "%s to %s" % (khz(fc_lo), khz(fc_hi)),
                      "%.0f° (%s)" % pm_min, "%s (%s)" % (db(None if gm_min[0] == 999.0 else gm_min[0]), gm_min[1]),
                      "%.2f (%s)" % ratio_max])
    report.table(["VIN", "IOUT", "Crossover", "Lowest phase margin", "Lowest gain margin",
                  "Highest crossover / limit"], table)
    report.check(worst_pm[0] >= PM_MIN, "phase margin at least %.0f° in every corner (lowest %.0f°%s)"
                 % (PM_MIN, worst_pm[0], worst_pm[1]))
    report.check(worst_gm[0] >= GM_MIN, "gain margin at least %.0f dB in every corner (lowest %.0f dB%s)"
                 % (GM_MIN, worst_gm[0], worst_gm[1]))
    report.check(worst_ratio[0] <= 1.0, "crossover below fSW/10 and RHPZ/5 in every corner up to "
                 "%.1f A (highest %.2f of the limit%s)" % (IOUT_DESIGN, worst_ratio[0], worst_ratio[1]))
    report.add("- Above %.1f A (short-term headroom, not a design point): crossover up to %.2f of the "
               "RHPZ/5 guideline%s. The margins above already include these corners."
               % (IOUT_DESIGN, worst_head[0], worst_head[1]))
    report.add()

    table = []
    for label, cout in list(COUT_CASES.items())[:2]:
        for vin, iout in ((3.1, 1.5), (3.1, 1.0), (3.7, 0.8)):
            loop = BoostLoop(vin, iout, cout)
            fc = loop.crossover_limit()
            r5, c5, c8 = loop.recommended(fc)
            table.append([label, "%.1f V / %.1f A" % (vin, iout), khz(fc), "%.1f kΩ" % (r5 / 1e3),
                          "%.1f nF" % (c5 / 1e-9), "%.0f pF" % (c8 / 1e-12)])
    report.add("Values the data sheet procedure (Equations 18 to 20) would give for the "
               "highest allowed crossover, against the fitted %.1f kΩ / %.0f nF / %.0f pF:"
               % (R18 / 1e3, C21 / 1e-9, C22 / 1e-12))
    report.add()
    report.table(["Output capacitance", "Operating point", "Target crossover", "R5 (R18)",
                  "C5 (C21)", "C8 (C22)"], table)
    loop = BoostLoop(3.7, 0.8, COUT_VALUES[1][1])
    step = 0.8 / (2 * math.pi * loop.crossover() * loop.cout)
    report.add("- Load step estimate at 3.7 V, nominal capacitance: a 0.8 A step moves the output "
               "by about ΔI / (2π · fc · COUT) = %.0f mV (%.1f %%) before the loop recovers. "
               "This is an estimate; the transient bench below simulates the steps."
               % (1e3 * step, 100 * step / VOUT))
    typical, minimum = ilim_boost()
    peak = VOUT * 1.5 / (3.1 * EFFICIENCY) + 3.1 * (1 - 3.1 / VOUT) / ((1 - L2_TOL) * L2 * fsw_boost(3.1)) / 2
    report.check(peak < minimum,
                 "peak inductor current %.2f A at 3.1 V / 1.5 A (L −%.0f %%) stays below the "
                 "worst-case current limit %.2f A (R17 %.0f kΩ, typical %.2f A) and the %.0f A "
                 "saturation current" % (peak, 100 * L2_TOL, minimum, R17 / 1e3, typical, L2_ISAT))
    report.add()


def peak_to_peak(rows, col, start):
    values = [row[col] for row in simlib.window(rows, start)]
    return max(values) - min(values), sum(values) / len(values), max(values)


def boost_ripple(report):
    report.add("## Boost switching ripple (ngspice, power/boost_ripple.cir)")
    report.add()
    cases = list(itertools.product(VIN_CASES, (0.2, 0.8, 1.5)))
    runs = [{"vin": vin, "iout": iout, "fsw": fsw_boost(vin), "cout": C_MAIN * C_MAIN_BIAS,
             "c24": C24 * C24_BIAS} for vin, iout in cases]
    results = BENCH.simulate_all("boost_ripple", runs)
    minimum = ilim_boost()[1]
    table = []
    worst_ripple, worst_peak = 0.0, 0.0
    for (vin, iout), rows in zip(cases, results):
        # columns: t, BOOST_5V, RAIL_5V, VSYS, I(L2), BOOST_SW, BOOST_5V filtered, RAIL_5V filtered
        start = rows[-1][0] - 10e-6
        out_pp, out_mean, _ = peak_to_peak(rows, 6, start)
        rail_pp, _, _ = peak_to_peak(rows, 7, start)
        in_pp, _, _ = peak_to_peak(rows, 3, start)
        il_pp, _, il_peak = peak_to_peak(rows, 4, start)
        raw_pp, _, _ = peak_to_peak(rows, 1, start)
        worst_ripple = max(worst_ripple, out_pp)
        worst_peak = max(worst_peak, il_peak)
        table.append(["%.1f V" % vin, "%.1f A" % iout, "%.2f V" % out_mean,
                      "%.0f mV" % (1e3 * out_pp), "%.0f mV" % (1e3 * rail_pp),
                      "%.1f mV" % (1e3 * in_pp), "%.2f A / %.2f A" % (il_pp, il_peak),
                      "%.1f V" % raw_pp])
    report.add("Steady state, last 10 µs of each run. Ripple is read through a 20 MHz "
               "bandwidth limit as on an oscilloscope. The last column is the unfiltered swing "
               "at the capacitor node in the model. It is ringing near 1 GHz of the assumed 0.3 nH "
               "copper loop with the switch capacitance, damped only by the assumed 0.5 Ω. Its "
               "real amplitude depends on the layout and the IC, so it is a bench measurement, "
               "not a design value. It falls into the 1090 MHz band of the ADS-B receiver:")
    report.add()
    report.table(["VIN", "IOUT", "VOUT mean", "BOOST_5V ripple", "RAIL_5V ripple",
                  "VSYS ripple", "L2 ripple / peak", "Model edge ringing"], table)
    report.check(worst_ripple < 0.1, "output ripple at most %.0f mV peak to peak (20 MHz), "
                 "below 100 mV" % (1e3 * worst_ripple))
    report.check(worst_peak < minimum,
                 "highest simulated inductor peak %.2f A below the worst-case current limit %.2f A"
                 % (worst_peak, minimum))
    report.add()


def buck_ripple(report):
    report.add("## Charger buck switching ripple on VSYS (ngspice, power/buck_ripple.cir)")
    report.add()
    cases = list(itertools.product((3.5, 4.2), (1.5, 3.0)))
    runs = [dict(vsys=vsys, iload=iload, **buck_bench_params()) for vsys, iload in cases]
    results = BENCH.simulate_all("buck_ripple", runs)
    table = []
    worst = 0.0
    for (vsys, iload), rows in zip(cases, results):
        # columns: t, VSYS, I(L1), CHG_SW, VSYS filtered
        start = rows[-1][0] - 10e-6
        pp, _, _ = peak_to_peak(rows, 4, start)
        il_pp, _, il_peak = peak_to_peak(rows, 2, start)
        worst = max(worst, pp)
        table.append(["%.1f V" % vsys, "%.1f A" % iload, "%.1f mV" % (1e3 * pp),
                      "%.2f A / %.2f A" % (il_pp, il_peak)])
    report.add("5 V input, %.2g MHz, L1 %.1f µH (%s), VSYS capacitors 4 x 22 µF derated to 49 µF. "
               "The %s loop is internal and not modelled, so only the ripple is meaningful:"
               % (BUCK_FSW / 1e6, CHG["l1"]["l"] * 1e6, CHG["l1"]["label"], CHG["part"]))
    report.add()
    report.table(["VSYS", "Load", "VSYS ripple (20 MHz)", "L1 ripple / peak"], table)
    report.check(worst < 0.02, "VSYS ripple at most %.1f mV peak to peak, below 20 mV" % (1e3 * worst))
    report.add()


# --- start-up, load steps and shutdown (power/boost_transient.cir)
C17, ISS = 47e-9, 5e-6  # soft start: C17 and the data sheet charging current
T_SS = C17 * VREF / ISS
C23_CT, U7_RISE = 10e-9, 16.98e-3  # TPS22975 CT capacitor and its Table 1 rise time (10 nF, 5 V)
# Module terminals: the ADSBee, the display and the GNSS board regulate 5 V down with LDOs.
# Their input windows are not specified beyond "5 V"; USB tolerance is applied: 4.75 to
# 5.25 V for the design loads, 4.5 V allowed during the 1.5 A headroom pulses, and 5.5 V
# as the hard limit (the 6 V class LDO inputs behind the terminals).
MODULE_VMIN, MODULE_VMIN_PULSE, MODULE_VMAX, MODULE_VABS = 4.75, 4.5, 5.25, 5.5
SETTLE_BAND = 0.01 * VOUT
COUT_TRANSIENT = {"nominal": (C_MAIN * C_MAIN_BIAS, C24 * C24_BIAS),
                  "minimum": (C_MAIN * (1 - C_MAIN_TOL) * C_MAIN_BIAS * C_TEMP * C_AC,
                              C24 * (1 - C24_TOL) * C24_BIAS * C_TEMP * C_AC)}
T_STEP, T_BACK = 25e-3, 35e-3


def model_params(vin, cout="nominal", ilim=None):
    """Parameters of the averaged boost, load switch and module model shared by the
    transient and the undervoltage bench."""
    params = {"fsw": fsw_boost(vin), "lval": L2, "r18": R18, "c21": C21, "c22": C22, "css": C17,
              "iss": ISS, "ct_rise": U7_RISE, "cmod": C_MODULES,
              "cinput": C_INPUT, "ads_share": ADS_SHARE, **ID_CORNERS["maximum"],
              "ilim": ilim_boost()[0] if ilim is None else ilim}
    params["cout"], params["c24"] = COUT_TRANSIENT[cout]
    return params


def transient_params(vin, i0, i1=None, t1=T_STEP, t2=T_BACK, toff=1.0, tstop=45e-3,
                     cout="nominal", ilim=None):
    return dict(model_params(vin, cout, ilim), vin=vin, i0=i0, i1=i0 if i1 is None else i1,
                t1=t1, t2=t2, toff=toff, tstop=tstop)


def extremes(rows, col, start, end=None):
    values = [row[col] for row in simlib.window(rows, start, end)]
    return min(values), max(values)


def settle_time(rows, col, t_event, t_end):
    """Time after t_event until the column stays within ±1 % of its value at t_end."""
    final = simlib.value_at(rows, t_end, col)
    last = t_event
    for row in simlib.window(rows, t_event, t_end):
        if abs(row[col] - final) > SETTLE_BAND:
            last = row[0]
    return last - t_event


def ripple_half(vin):
    return vin * (1 - vin / VOUT) / (L2 * fsw_boost(vin)) / 2


def ms(t):
    return "none" if t is None else "%.1f ms" % (t * 1e3)


def boost_transient(report):
    for diode in ID_CORNERS:
        boost_transient_corner(report, diode)


def boost_transient_corner(report, diode):
    # columns: t, BOOST_5V, RAIL_5V, MODULE, iL average, COMP, SS, load, VSYS, U7 gate
    B5V, RAIL, MOD, IL, COMP, SS, LOAD, VSYS, GATE, ADS, ESP, MODMAX = range(1, 13)
    typical, minimum = ilim_boost()
    report.add("## Start-up, load steps and shutdown (%s diode corner)" % diode)
    report.add()
    report.add("Averaged closed-loop model of the TPS61088: peak current command from COMP with "
               "the data sheet clamps, error amplifier 190 µA/V against the soft-start ramp, and the "
               "fitted R18 %.1f kΩ / C21 %.0f nF / C22 %.0f pF. The TPS22975 is a slew-controlled "
               "switch (C23 %.0f nF on CT: %.0f ms rise per data sheet Table 1, %.0f mΩ, 230 Ω "
               "discharge when off). U11/U12 feed separate ADSBee and display branches. "
               "The modules are a programmed current above 3 V behind "
               "%.0f µF of total input capacitance and 20 mΩ of connector per branch. Output capacitance is %.0f µF "
               "(C18 to C20 derated) plus %.0f µF (C24), or %.0f + %.0f µF in the minimum corner. "
               "PWR_EN enables U6 and U7 together. Soft start: C17 %.0f nF at %.0f µA, %.1f ms to "
               "the 1.204 V reference. Switching ripple, the sampling poles and the inductor "
               "energy at shutdown are not in this model."
               % (R18 / 1e3, C21 / 1e-9, C22 / 1e-12, C23_CT / 1e-9, U7_RISE * 1e3, 16, C_MODULES / 1e-6,
                  COUT_TRANSIENT["nominal"][0] / 1e-6, COUT_TRANSIENT["nominal"][1] / 1e-6,
                  COUT_TRANSIENT["minimum"][0] / 1e-6, COUT_TRANSIENT["minimum"][1] / 1e-6,
                  C17 / 1e-9, ISS / 1e-6, T_SS * 1e3))
    report.add()

    if CHG is chargers.CHARGERS["ti"]:
        report.add("TPS259474L: the maximum corner uses an assumed 28.4 mV forward drop, not a guaranteed datasheet maximum.")
    report.add("%s %s corner: regulated forward drop %.1f mV, on-resistance %.2f mΩ; "
               "quasistatic reverse blocking with less than 0.6 mV of numerical diode drop. ADSBee (including GNSS) takes %.1f %% of the load "
               "and input capacitance, the display the rest, using the heat model's Wi-Fi-on split. "
               "C39/C38 add 100 nF each at the diode inputs. Minimum and maximum terminal "
               "figures are envelopes across both branches. The module capacitances and load "
               "split remain assumptions; diode switching delays, UVLO, startup slew, circuit-breaker operation and parasitics are not modelled."
               % (ID_NAME, diode, ID_CORNERS[diode]["id_vfwd"] * 1e3,
                  ID_CORNERS[diode]["id_ron"] * 1e3, ADS_SHARE * 100))
    report.add()

    # --- start-up
    cases = list(itertools.product(VIN_CASES, (0.1, 0.8)))
    runs = [transient_params(vin, load, t1=1.0, tstop=30e-3) for vin, load in cases]
    results = BENCH.simulate_all("boost_transient", [dict(run, **ID_CORNERS[diode]) for run in runs])
    table, worst_over, worst_peak, worst_dip, slowest = [], 0.0, 0.0, 0.0, 0.0
    for (vin, load), rows in zip(cases, results):
        t_reg = simlib.crossing(rows, B5V, 0.99 * VOUT)
        t_mod = simlib.crossing(rows, MOD, MODULE_VMIN)
        over = extremes(rows, B5V, 0)[1] - VOUT
        peak = extremes(rows, IL, 0)[1] + ripple_half(vin)
        # dip after the module terminal reached its window: largest drop below the running maximum
        dip = 0.0
        for col in (ADS, ESP):
            running = 0.0
            for row in simlib.window(rows, t_mod or 1.0):
                running = max(running, row[col])
                dip = max(dip, running - row[col])
        vsys_min = extremes(rows, VSYS, 0)[0]
        worst_over, worst_peak = max(worst_over, over), max(worst_peak, peak)
        worst_dip, slowest = max(worst_dip, dip), max(slowest, t_mod or 1.0)
        table.append(["%.1f V" % vin, "%.1f A" % load, ms(t_reg), ms(t_mod), "%+.0f mV" % (1e3 * over),
                      "%.0f mV" % (1e3 * dip), "%.2f A" % peak, "%.2f V" % vsys_min])
    report.add("Start-up from PWR_EN, the modules drawing their load as soon as the terminal passes 3 V:")
    report.add()
    report.table(["VIN", "Load", "BOOST_5V in regulation", "Module terminal at %.2f V" % MODULE_VMIN,
                  "BOOST_5V overshoot", "Terminal dip afterwards", "Peak inductor current", "VSYS minimum"],
                 table)
    report.check(worst_over < MODULE_VMAX - VOUT, "start-up overshoot at most %.0f mV, terminal below %.2f V"
                 % (1e3 * worst_over, MODULE_VMAX))
    report.check(worst_peak < minimum, "start-up peak inductor current %.2f A below the worst-case "
                 "current limit %.2f A" % (worst_peak, minimum))
    report.check(worst_dip < 0.05, "module terminal rises without a dip larger than 50 mV once above "
                 "%.2f V (largest %.0f mV)" % (MODULE_VMIN, 1e3 * worst_dip))
    report.add("- The module terminal follows the TPS22975 slew (about %.0f ms), not the %.1f ms boost "
               "soft start. The terminal is ready %s after PWR_EN at the latest." % (U7_RISE * 1e3, T_SS * 1e3, ms(slowest)))
    report.add()

    # --- load steps
    cases = [(3.1, "nominal", 0.1, 0.8), (3.1, "minimum", 0.1, 0.8), (3.7, "nominal", 0.1, 0.8),
             (4.4, "nominal", 0.1, 0.8), (3.1, "nominal", 0.8, 1.0), (3.1, "minimum", 0.8, 1.0),
             (3.7, "nominal", 0.8, 1.0)]
    runs = [transient_params(vin, i0, i1, cout=cout) for vin, cout, i0, i1 in cases]
    results = BENCH.simulate_all("boost_transient", [dict(run, **ID_CORNERS[diode]) for run in runs])
    table, worst_min, worst_max, worst_peak, slowest = [], 9.0, 0.0, 0.0, 0.0
    for (vin, cout, i0, i1), rows in zip(cases, results):
        v_min = extremes(rows, MOD, T_STEP, T_BACK)[0]
        v_max = extremes(rows, MODMAX, T_BACK)[1]
        peak = extremes(rows, IL, T_STEP)[1] + ripple_half(vin)
        up = max(settle_time(rows, col, T_STEP, T_BACK) for col in (ADS, ESP))
        down = max(settle_time(rows, col, T_BACK, 45e-3) for col in (ADS, ESP))
        worst_min, worst_max = min(worst_min, v_min), max(worst_max, v_max)
        worst_peak, slowest = max(worst_peak, peak), max(slowest, up, down)
        table.append(["%.1f V" % vin, cout, "%.1f to %.1f to %.1f A" % (i0, i1, i0), "%.3f V" % v_min,
                      "%.0f mV" % (1e3 * (v_min - MODULE_VMIN)), ms(up), "%.3f V" % v_max, ms(down), "%.2f A" % peak])
    report.add("Load steps at the module terminal (10 µs edges), from steady state, 10 ms apart:")
    report.add()
    report.table(["VIN", "Capacitance", "Step", "Terminal minimum", "Margin to 4.75 V", "Settled (±1 %)", "Terminal maximum",
                  "Settled (±1 %)", "Peak inductor current"], table)
    report.check(worst_min >= MODULE_VMIN, "terminal stays above %.2f V through the steps between "
                 "0.1 and 0.8 A and between 0.8 and 1 A (lowest %.3f V, margin %.0f mV)"
                 % (MODULE_VMIN, worst_min, 1e3 * (worst_min - MODULE_VMIN)))
    report.check(worst_max <= MODULE_VABS, "terminal stays below %.1f V on load release (highest %.2f V; "
                 "%s the %.2f V USB window by %.0f mV)"
                 % (MODULE_VABS, worst_max, "above" if worst_max > MODULE_VMAX else "inside", MODULE_VMAX,
                    1e3 * abs(worst_max - MODULE_VMAX)))
    report.check(worst_peak < minimum, "peak inductor current %.2f A during the steps below the worst-case "
                 "current limit %.2f A" % (worst_peak, minimum))
    report.check(slowest < 5e-3, "settled within ±1 %% in at most %s" % ms(slowest))
    report.add()

    # --- 1.5 A pulses from the nominal load
    cases = [(3.1, "nominal", 1e-3, typical), (3.1, "nominal", 10e-3, typical),
             (3.1, "minimum", 10e-3, typical), (3.1, "minimum", 10e-3, minimum),
             (3.7, "nominal", 10e-3, typical), (4.4, "nominal", 10e-3, typical)]
    runs = [transient_params(vin, 0.8, 1.5, t2=T_STEP + width, cout=cout, ilim=ilim)
            for vin, cout, width, ilim in cases]
    results = BENCH.simulate_all("boost_transient", [dict(run, **ID_CORNERS[diode]) for run in runs])
    table, worst_min, worst_max, worst_peak, limited = [], 9.0, 0.0, 0.0, []
    for (vin, cout, width, ilim), rows in zip(cases, results):
        t_end = T_STEP + width
        v_min = extremes(rows, MOD, T_STEP, t_end)[0]
        v_end = simlib.value_at(rows, t_end, MOD)
        v_max = extremes(rows, MODMAX, t_end)[1]
        peak = extremes(rows, IL, T_STEP)[1] + ripple_half(vin)
        clamp = extremes(rows, COMP, T_STEP, t_end)[1] >= 1.4 + ilim * RSENSE - 1e-3
        vsys_min = extremes(rows, VSYS, T_STEP, t_end)[0]
        down = max(settle_time(rows, col, t_end, 45e-3) for col in (ADS, ESP))
        if clamp:
            limited.append("%.1f V, %s, %.0f ms, limit %.2f A" % (vin, cout, width * 1e3, ilim))
        worst_min, worst_max, worst_peak = min(worst_min, v_min), max(worst_max, v_max), max(worst_peak, peak)
        table.append(["%.1f V" % vin, cout, "%.0f ms" % (width * 1e3), "%.2f A" % ilim, "%.3f V" % v_min,
                      "%.2f V" % v_end, "%.2f A%s" % (peak, " (limited)" if clamp else ""), "%.2f V" % vsys_min,
                      "%.2f V" % v_max, ms(down)])
    report.add("1.5 A pulses from 0.8 A (short-term headroom), with the typical and the worst-case "
               "current limit:")
    report.add()
    report.table(["VIN", "Capacitance", "Pulse", "Current limit", "Terminal minimum", "Terminal at pulse end",
                  "Peak inductor current", "VSYS minimum", "Maximum after release", "Settled (±1 %)"], table)
    report.check(worst_min >= MODULE_VMIN_PULSE, "terminal stays above %.2f V during the 1.5 A pulses "
                 "(lowest %.3f V)" % (MODULE_VMIN_PULSE, worst_min))
    report.check(worst_max <= MODULE_VABS, "terminal stays below %.1f V after the pulses (highest %.2f V)"
                 % (MODULE_VABS, worst_max))
    report.check(not limited, "the current limit does not engage during the pulses"
                 + ("" if not limited else "; engaged at: " + "; ".join(limited)))
    report.add()

    # --- shutdown
    cases = [(3.7, 0.8), (3.7, 0.1)]
    runs = [transient_params(vin, load, t1=1.0, toff=30e-3, tstop=45e-3) for vin, load in cases]
    results = BENCH.simulate_all("boost_transient", [dict(run, **ID_CORNERS[diode]) for run in runs])
    table, slowest, lowest = [], 0.0, 0.0
    for (vin, load), rows in zip(cases, results):
        t_half = simlib.crossing(rows, MODMAX, VOUT / 2, after=30e-3)
        t_low = simlib.crossing(rows, MODMAX, 0.5, after=30e-3)
        v_min = extremes(rows, MOD, 30e-3)[0]
        boost_end = rows[-1][B5V]
        slowest, lowest = max(slowest, (t_low or 1.0) - 30e-3), min(lowest, v_min)
        table.append(["%.1f V" % vin, "%.1f A" % load, ms(t_half and t_half - 30e-3),
                      ms(t_low and t_low - 30e-3), "%.2f V" % v_min, "%.2f V" % boost_end])
    report.add("Shutdown: PWR_EN low at 30 ms, U7 opens and discharges the rail through 230 Ω, U6 stops:")
    report.add()
    report.table(["VIN", "Load before", "Terminal below %.2f V" % (VOUT / 2), "Terminal below 0.5 V",
                  "Terminal minimum", "BOOST_5V 15 ms later"], table)
    report.check(slowest < 50e-3, "module terminal below 0.5 V within %s of PWR_EN falling" % ms(slowest))
    report.check(lowest > -0.05, "no negative excursion at the module terminal")
    report.add("- BOOST_5V stays charged behind the open switch; only the 315 kΩ divider discharges "
               "it, over seconds. U11/U12 block the module capacitors from U7 discharge; "
               "the programmed module loads discharge them. Not modelled: the inductor energy "
               "dumped into COUT when switching stops. It is about L I² / 2 = %.1f µJ at 0.8 A load "
               "(1.5 A in the inductor), which raises %.0f µF by under %.0f mV."
               % (0.5 * L2 * 1.5 ** 2 * 1e6, COUT_TRANSIENT["nominal"][0] / 1e-6,
                  1e3 * 0.5 * L2 * 1.5 ** 2 / (COUT_TRANSIENT["nominal"][0] * VOUT)))
    report.add()


# --- undervoltage cut-off, rebound and restart (power/undervoltage.cir)
VIT = 2.9  # TLV840CADL29, VIT-
VHYS_CASES = {"2.5 % (minimum)": 0.025 * VIT, "5 % (typical)": 0.05 * VIT, "7 % (maximum)": 0.07 * VIT}
TD_CT_OPEN, TP_SUPERVISOR = 40e-6, 30e-6  # reset delay with CT open, propagation delay
VIN_UVLO = 2.4  # TPS61088 VIN UVLO falling, minimum
R_B2S = 20e-3  # charger BATT to SYS switch, bind_charger
# Board copper J3 to F1, F1 to U4 BATT, U4 SYS to L2 from heat/<charger>/calibration.json (run_heat.py
# solves the routed tracks and copper areas). test_heat.py checks them against the heat model.
R_BOARD = 1.3e-3 + 4.6e-3 + 6.2e-3
R_SWITCH_BOARD = R_B2S + R_BOARD
R_BOARD_SOURCE = "literal"


def buck_bench_params():
    """power/buck_ripple.cir parameters of the project's charger."""
    return dict(fsw=CHG["fsw"], lval=CHG["l1"]["l"], dcr=CHG["l1"]["dcr"], ron_hs=CHG["r_hs"], ron_ls=CHG["r_ls"],
                r_series=CHG["r_series"])
# Pack: <= 50 mOhm AC at 1 kHz and 50 % capacity (data sheet); the DC value near the end of
# discharge with the protection FETs is assumed 2 to 3 times that. Harness: two JST-PH
# contacts (20 mOhm maximum each), AWG 24 lead 2 x 10 cm (17 mOhm) and F1 (11 mOhm, Littelfuse
# 0466005.NRHF): 68 mOhm with the contacts at their maximum.
R_PATH_CASES = {"pack 50 + harness 60 mΩ": (50e-3, 60e-3), "pack 100 + harness 60 mΩ": (100e-3, 60e-3),
                "pack 150 + harness 100 mΩ": (150e-3, 100e-3)}
UV_LOADS = {"Wi-Fi off": 0.23, "nominal": 0.8, "peak": 1.0}
UV_RAMP = (20e-3, 420e-3, 520e-3, 920e-3)  # discharge ramp start, end; restore ramp start, end
UV_VOC = (3.30, 2.95, 3.30)
OCV_SLOPE = 1.0  # V per Ah near 3.0 V open-circuit voltage, assumed for the duration estimate


def uv_params(load, rpath, vhys, voc=UV_VOC, ramp=UV_RAMP, burst=None, tstop=1.0, **extra):
    i1, t1, t2 = burst or (load, 10.0, 10.0)
    return dict(model_params(3.0), i0=load, i1=i1, t1=t1, t2=t2, rbat=rpath[0], rharn=rpath[1],
                rb2s=R_SWITCH_BOARD, vit=VIT, vhys=vhys, td=TD_CT_OPEN, tp=TP_SUPERVISOR,
                vin_uvlo=VIN_UVLO, voc0=voc[0], voc1=voc[1], voc2=voc[2], ta=ramp[0], tb=ramp[1],
                tc=ramp[2], tdn=ramp[3], tstop=tstop, **extra)


def sys_current(load, vsys):
    """Battery-path current for a 5 V load at a SYS voltage."""
    return load * VOUT / (EFFICIENCY * vsys)


def pwr_en_edges(rows, col, after=5e-3):
    """Falling and rising crossings of PWR_EN through 1.2 V, as two lists of times. The default
    start skips the slider closing at 2 ms."""
    edges = [(b[0], b[col] > 1.2) for a, b in zip(rows, rows[1:])
             if (a[col] - 1.2) * (b[col] - 1.2) < 0 and b[0] > after]
    return [t for t, up in edges if not up], [t for t, up in edges if up]


def undervoltage(report):
    # columns: t, VOC, pack terminal, VSYS, PWR_EN, MODULE, iL, load, BOOST_5V, supervisor sense
    VOC, BATTJ, VSYS, PWREN, MOD, IL, LOAD, B5V, SENSE = range(1, 10)
    report.add("## Undervoltage cut-off, rebound and restart (ngspice, power/undervoltage.cir)")
    report.add("U11/U12 use the maximum diode-drop corner and the same separate module branches "
               "as the transient bench. Module readiness requires both terminals above 4.75 V.")
    report.add()
    report.add("The battery is an open-circuit voltage behind the pack resistance and the harness. "
               "Then follow the " + CHG["part"] + " BATT-to-SYS switch (%.0f mΩ) and the board copper (%.0f mΩ "
               "from the routed tracks, J3 to L2). The TLV840 on VSYS (VIT− %.1f V, "
               "hysteresis %s, %.0f µs propagation, %.0f µs reset delay with CT open) pulls PWR_EN "
               "low against R13 100 kΩ. R13 is fed from VSYS through R43 1 kΩ, the closed thermostat "
               "and the closed slider. The boost, load switch and modules are those of the transient "
               "bench. The open-circuit voltage ramps from %.2f to %.2f V in %.0f ms, holds, then "
               "ramps back to %.2f V (restore). The ramp stands for a discharge; it is fast only "
               "against the electrical time constants. The thermal bench holds the supervisor "
               "released; this one does not."
               % (R_B2S * 1e3, R_BOARD * 1e3, VIT, ", ".join("%.0f mV" % (1e3 * v) for v in VHYS_CASES.values()),
                  TP_SUPERVISOR * 1e6, TD_CT_OPEN * 1e6, UV_VOC[0], UV_VOC[1],
                  (UV_RAMP[1] - UV_RAMP[0]) * 1e3, UV_VOC[2]))
    report.add()

    # --- what the numbers say before simulating
    table = []
    for lname, load in UV_LOADS.items():
        for rname, (rpack, rharn) in R_PATH_CASES.items():
            drop = sys_current(load, VIT) * (rpack + rharn + R_SWITCH_BOARD)
            verdict = ["chatter" if drop > vhys else "clean" for vhys in VHYS_CASES.values()]
            table.append([lname + " %.2f A" % load, rname, "%.2f A" % sys_current(load, VIT),
                          "%.0f mV" % (1e3 * drop)] + verdict)
    report.add("Voltage the load drops across the path at the trip point against the hysteresis. "
               "When the drop exceeds the hysteresis, cutting the load lifts VSYS above the release "
               "threshold and the supervisor restarts the system:")
    report.add()
    report.table(["Load", "Path resistance", "Path current at %.1f V" % VIT, "Drop I·R"]
                 + ["Hysteresis " + name for name in VHYS_CASES], table)
    report.add()

    # --- simulated discharge and restore
    cases = [("nominal", "pack 100 + harness 60 mΩ", "5 % (typical)"),
             ("nominal", "pack 150 + harness 100 mΩ", "2.5 % (minimum)"),
             ("nominal", "pack 50 + harness 60 mΩ", "7 % (maximum)"),
             ("Wi-Fi off", "pack 100 + harness 60 mΩ", "5 % (typical)"),
             ("Wi-Fi off", "pack 150 + harness 100 mΩ", "2.5 % (minimum)"),
             ("peak", "pack 100 + harness 60 mΩ", "5 % (typical)")]
    runs = [uv_params(UV_LOADS[l], R_PATH_CASES[r], VHYS_CASES[h]) for l, r, h in cases]
    results = BENCH.simulate_all("undervoltage", runs)
    table, chatter, uvlo_hit = [], [], []
    for (lname, rname, hname), rows in zip(cases, results):
        falls, rises = pwr_en_edges(rows, PWREN)
        f_dis = [t for t in falls if t < UV_RAMP[2]]
        band = ("%.3f to %.3f V" % (simlib.value_at(rows, f_dis[0], VOC), simlib.value_at(rows, f_dis[-1], VOC))
                if f_dis else "no trip")
        period = (f_dis[-1] - f_dis[0]) / (len(f_dis) - 1) if len(f_dis) > 1 else None
        # on-time fraction while cycling: PWR_EN high between a rise and the next fall
        on_time = sum(min([f for f in falls if f > r] or [rows[-1][0]]) - r for r in rises
                      if r < UV_RAMP[2] and f_dis and f_dis[0] <= r <= f_dis[-1])
        fraction = on_time / (f_dis[-1] - f_dis[0]) if len(f_dis) > 1 else None
        vsys_min = extremes(rows, VSYS, 0)[0]
        if len(f_dis) > 1:
            chatter.append("%s load, %s, hysteresis %s: %d restarts" % (lname, rname, hname, len(f_dis) - 1))
        if vsys_min < VIN_UVLO:
            uvlo_hit.append("%s, %s" % (lname, rname))
        table.append([lname, rname, hname, str(len(f_dis)), band, ms(period), "none" if fraction is None
                      else "%.0f %%" % (100 * fraction), str(len(falls) - len(f_dis)), "%.2f V" % vsys_min])
    report.add("Simulated discharge and restore ramps, PWR_EN counted through 1.2 V:")
    report.add()
    report.table(["Load", "Path resistance", "Hysteresis", "Trips while discharging",
                  "Open-circuit voltage band of the trips", "Cycle", "On-time in the band",
                  "Trips while restoring", "VSYS minimum"], table)
    report.add("- Without the latch: "
               + ("no cycling in any case." if not chatter else "cycling at " + "; ".join(chatter) + "."))
    report.check(not uvlo_hit, "the supervisor cuts the load before VSYS reaches the boost UVLO (%.1f V)"
                 % VIN_UVLO + ("" if not uvlo_hit else "; UVLO reached at: " + "; ".join(uvlo_hit)))
    ref = results[0]
    f_dis = [t for t in pwr_en_edges(ref, PWREN)[0] if t < UV_RAMP[2]]
    if len(f_dis) > 1:
        width = simlib.value_at(ref, f_dis[0], VOC) - simlib.value_at(ref, f_dis[-1], VOC)
        i_avg = sys_current(UV_LOADS["nominal"], VIT) * 0.5
        report.add("- Duration estimate for the nominal case: the trips span %.0f mV of open-circuit "
                   "voltage. The pack is assumed to lose about %.1f V per Ah near 3 V, and the average "
                   "path current while cycling is roughly %.2f A. That gives about %.0f minutes of "
                   "restarts every %s at the end of every discharge."
                   % (1e3 * width, OCV_SLOPE, i_avg, 60 * width / (OCV_SLOPE * i_avg),
                      ms((f_dis[-1] - f_dis[0]) / (len(f_dis) - 1))))
    report.add()

    # --- load burst just above the band
    rpath = R_PATH_CASES["pack 100 + harness 60 mΩ"]
    band_top = VIT + sys_current(UV_LOADS["nominal"], VIT) * sum(rpath + (R_SWITCH_BOARD,))
    cases = [(band_top + 0.03, 10e-3), (band_top + 0.10, 10e-3), (band_top + 0.03, 1e-3)]
    runs = [uv_params(UV_LOADS["nominal"], rpath, VHYS_CASES["5 % (typical)"], voc=(v, v, v),
                      burst=(1.5, 100e-3, 100e-3 + width), tstop=300e-3) for v, width in cases]
    results = BENCH.simulate_all("undervoltage", runs)
    table, worst_restarts, longest = [], 0, 0.0
    for (voc, width), rows in zip(cases, results):
        falls, rises = pwr_en_edges(rows, PWREN, after=90e-3)
        outage = (rises[-1] - falls[0]) if falls and rises and rises[-1] > falls[0] else 0.0
        ready = simlib.crossing(rows, MOD, MODULE_VMIN, after=100e-3 + width) if falls else None
        worst_restarts, longest = max(worst_restarts, len(falls)), max(longest, outage)
        table.append(["%.2f V" % voc, "%.0f ms" % (width * 1e3), "%.2f V" % extremes(rows, VSYS, 100e-3)[0],
                      str(len(falls)), ms(outage) if falls else "none",
                      ms(ready - 100e-3) if ready else "not interrupted"])
    report.add("1.5 A burst from the nominal load with the open-circuit voltage held just above the "
               "band (band top %.2f V for the typical path), typical hysteresis:" % band_top)
    report.add()
    report.table(["Open-circuit voltage", "Burst", "VSYS minimum", "Trips", "PWR_EN low for",
                  "Module terminal back at %.2f V after burst start" % MODULE_VMIN], table)
    report.check(worst_restarts <= 1, "a load burst near cut-off causes at most one shutdown and restart "
                 "(most: %d)" % worst_restarts)
    report.add()

    # --- what stops the cycling
    runs = [uv_params(UV_LOADS["nominal"], R_PATH_CASES["pack 150 + harness 100 mΩ"], vhys,
                      voc=(3.6, UV_VOC[1], 3.6)) for vhys in (0.3, 0.4, 0.5)]
    results = BENCH.simulate_all("undervoltage", runs)
    table = []
    for vhys, rows in zip((0.3, 0.4, 0.5), results):
        falls = pwr_en_edges(rows, PWREN)[0]
        discharging = sum(t < UV_RAMP[2] for t in falls)
        table.append(["%.0f mV" % (1e3 * vhys), str(discharging), str(len(falls) - discharging)])
    report.add("The same discharge and restore with a larger hysteresis (a comparator with external "
               "hysteresis, or a latch, in place of the fixed 5 %), nominal load, worst-case path:")
    report.add()
    report.table(["Hysteresis", "Trips while discharging", "Trips while restoring"], table)
    report.add()

    # --- the latch: Q7 (AO3401A) sets LATCH from the delayed hold supply when PWR_EN drops,
    # Q8 (2N7002) holds PWR_EN low; cleared by the slider (or USB via Q9, not in this bench)
    cases = [("nominal", "pack 100 + harness 60 mΩ", "5 % (typical)"),
             ("nominal", "pack 150 + harness 100 mΩ", "2.5 % (minimum)"),
             ("peak", "pack 100 + harness 60 mΩ", "5 % (typical)")]
    runs = [uv_params(UV_LOADS[l], R_PATH_CASES[r], VHYS_CASES[h], latch=1) for l, r, h in cases]
    burst = uv_params(UV_LOADS["nominal"], rpath, VHYS_CASES["5 % (typical)"],
                      voc=(band_top + 0.03,) * 3, burst=(1.5, 100e-3, 110e-3), tstop=300e-3, latch=1)
    # Q8 at its maximum VGS(th): LATCH is set from VSYS through R43 (thermostat feed), R40 and Q7 into
    # R41 while R13 and R10 also load THERM_OK, so the series resistor R43 eats into this margin
    q8max = uv_params(UV_LOADS["nominal"], R_PATH_CASES["pack 150 + harness 100 mΩ"],
                      VHYS_CASES["2.5 % (minimum)"], latch=1, q8_vth=2.5)
    results = BENCH.simulate_all("undervoltage", runs + [burst, q8max])
    cases += [("nominal, 1.5 A burst at %.2f V" % (band_top + 0.03), "pack 100 + harness 60 mΩ",
               "5 % (typical)"),
              ("nominal, Q8 VGS(th) 2.5 V (max)", "pack 150 + harness 100 mΩ", "2.5 % (minimum)")]
    table, most_trips, restarts = [], 0, 0
    for (l, r, h), rows in zip(cases, results):
        falls, rises = pwr_en_edges(rows, PWREN)
        most_trips, restarts = max(most_trips, len(falls)), restarts + len(rises)
        table.append([l, r, h, str(len(falls)), str(len(rises)),
                      "%.3f V" % simlib.value_at(rows, falls[0], VOC) if falls else "no trip",
                      "%.2f V" % rows[-1][PWREN]])
    report.add("With the latch fitted (Q7 AO3401A from the R40/C37 delayed hold supply, Q8 2N7002 on "
               "PWR_EN, R41 100 kΩ bleed), same discharge and restore, then the burst case and Q8 at "
               "its maximum threshold:")
    report.add()
    report.table(["Load", "Path resistance", "Hysteresis", "Trips", "Restarts", "Open-circuit voltage at the trip",
                  "PWR_EN at the end (voltage restored)"], table)
    report.check(most_trips <= 1 and restarts == 0, "with the latch: at most one shutdown per discharge and no "
                 "restart until the slider is cycled (trips %d, restarts %d)" % (most_trips, restarts))
    report.add("- The cycling is set by the drop I·R across pack, harness and switch against the fixed "
               "5 % hysteresis. The reset delay (CT, 6.2 ms per 10 nF) only stretches the off time of "
               "each cycle. Every trip drops the rail through the 230 Ω discharge, so each cycle is a "
               "module reboot. The pack's own protection cuts at 2.5 V ± 0.1 V, below all of this.")
    report.add()


AGGRESSORS = {"BOOST_SW": VOUT, "CHG_SW": 5.0, "CHG_SW1": 5.0, "CHG_SW2": 5.0}  # switch-node net and its edge swing
VICTIMS = ("BOOST_FB", "BOOST_COMP", "BOOST_SS", "BOOST_ILIM", "BAT_TS", "CHG_PVL", "CHG_IFAST",
           "CHG_VDD", "CHG_PROG", "CHG_ILIM", "CHG_REGN", "THERM_OK", "CHG_DIS", "I2C_SDA", "I2C_SCL", "GNSS_PPS")
NODE_CAP = 5e-12  # pin plus trace capacitance of a high-impedance node, assumed


def read_pcb():
    """Copper of the board: tracks and pads as (x1, y1, x2, y2, width, layer, net)."""
    with open(project.pcb(project.resolve())) as pcb:
        text = pcb.read()
    copper = []
    for m in re.finditer(r"\(segment\n\t\t\(start ([-\d.]+) ([-\d.]+)\)\n\t\t\(end ([-\d.]+) "
                         r"([-\d.]+)\)\n\t\t\(width ([\d.]+)\)\n\t\t\(layer \"([^\"]+)\"\)\n"
                         r"\t\t\(net \"([^\"]*)\"\)", text):
        copper.append(tuple(float(v) for v in m.groups()[:5]) + (m.group(6), m.group(7)))
    positions = {}
    for fm in re.finditer(r"\n\t\(footprint \"[^\"]+\"\n(?:\t\t.*\n)*?\t\t\(at ([-\d.]+) ([-\d.]+)"
                          r"(?: ([-\d.]+))?\)\n", text):
        block = text[fm.start():text.find("\n\t)\n", fm.start())]
        ref = re.search(r"\(property \"Reference\" \"([^\"]+)\"", block).group(1)
        fx, fy, rot = float(fm.group(1)), float(fm.group(2)), math.radians(float(fm.group(3) or 0))
        positions[ref] = (fx, fy)
        for pm in re.finditer(r"\(pad \"[^\"]*\" \w+ \w+\n\t\t\t\(at ([-\d.]+) ([-\d.]+)(?: [-\d.]+)?\)"
                              r"\n\t\t\t\(size ([\d.]+) ([\d.]+)\)\n(?:\t\t\t.*\n)*?\t\t\t"
                              r"\(net \"([^\"]*)\"\)", block):
            px, py = float(pm.group(1)), float(pm.group(2))
            x = fx + px * math.cos(rot) + py * math.sin(rot)
            y = fy - px * math.sin(rot) + py * math.cos(rot)
            size = max(float(pm.group(3)), float(pm.group(4)))
            copper.append((x, y, x, y, size, "pad", pm.group(5)))
    # pours: the edges of every filled polygon as zero-width segments, so the distance to a pour is
    # the distance to its outline
    sys.modules.setdefault("run_power", sys.modules[__name__])  # run_heat imports run_power: keep one copy when run as a script
    import run_heat
    for net, layer, pts in run_heat.read_board(project.pcb(project.resolve()))["zones"]:
        if layer != "F":
            continue  # victims are top-layer tracks; a pour on another layer is not a coplanar neighbour
        for (x1, y1), (x2, y2) in zip(pts, pts[1:] + pts[:1]):
            copper.append((x1, y1, x2, y2, 0.0, "zone F", net))
    return copper, positions


def distance(a, b):
    """Edge-to-edge distance between two copper items."""
    def point_to_segment(px, py, x1, y1, x2, y2):
        dx, dy = x2 - x1, y2 - y1
        length2 = dx * dx + dy * dy
        t = 0 if length2 == 0 else max(0, min(1, ((px - x1) * dx + (py - y1) * dy) / length2))
        return math.hypot(px - (x1 + t * dx), py - (y1 + t * dy))
    centre = min(point_to_segment(a[0], a[1], *b[:4]), point_to_segment(a[2], a[3], *b[:4]),
                 point_to_segment(b[0], b[1], *a[:4]), point_to_segment(b[2], b[3], *a[:4]))
    return centre - (a[4] + b[4]) / 2


def coplanar_capacitance(gap, width, length, er=4.5):
    """Mutual capacitance of two coplanar strips (conformal mapping, K(k')/K(k))."""
    def elliptic_k(k):
        a, b = 1.0, math.sqrt(1 - k * k)
        while abs(a - b) > 1e-12:
            a, b = (a + b) / 2, math.sqrt(a * b)
        return math.pi / (2 * a)
    k = gap / (gap + 2 * width)
    eps = 8.854e-12 * (er + 1) / 2
    return eps * elliptic_k(math.sqrt(1 - k * k)) / elliptic_k(k) * length


def layout_coupling(report):
    report.add("## Switch-node coupling on the board")
    report.add()
    copper, positions = read_pcb()
    by_net = {}
    for item in copper:
        by_net.setdefault(item[6], []).append(item)
    table = []
    worst_fb = 0.0
    for aggressor, swing in AGGRESSORS.items():
        for victim in VICTIMS:
            best = min((distance(a, b) for a in by_net.get(aggressor, [])
                        for b in by_net.get(victim, [])), default=None)
            if best is None or best > 5:
                continue
            # length of victim track within 0.6 mm of the aggressor copper
            run = 0.0
            for b in by_net[victim]:
                if b[5] == "pad" or b[5].startswith("zone"):
                    continue
                length = math.hypot(b[2] - b[0], b[3] - b[1])
                steps = max(2, int(length / 0.05))
                near = 0
                for i in range(steps + 1):
                    px, py = b[0] + (b[2] - b[0]) * i / steps, b[1] + (b[3] - b[1]) * i / steps
                    if min(distance((px, py, px, py, b[4], "", ""), a) for a in by_net[aggressor]) < 0.6:
                        near += 1
                run += length * near / (steps + 1)
            cm = coplanar_capacitance(max(best, 0.1) * 1e-3, 0.2e-3, max(run, 0.2) * 1e-3)
            spike = swing * cm / (cm + NODE_CAP)
            if victim == "BOOST_FB":
                worst_fb = max(worst_fb, spike)
            table.append([aggressor, victim, "%.2f mm" % best, "%.1f mm" % run,
                          "%.3f pF" % (cm * 1e12), "%.0f mV" % (1e3 * spike)])
    report.add("For each sensitive net: the edge-to-edge distance from the switch-node copper (pads "
               "and tracks) and the length of victim track within 0.6 mm. Then the coplanar coupling "
               "capacitance for that length (0.2 mm minimum) and the spike it injects into a "
               "%.0f pF node at a full switch-node edge:" % (NODE_CAP * 1e12))
    report.add()
    report.table(["Aggressor", "Victim", "Gap", "Parallel run", "Coupling", "Injected spike"],
                 table)
    report.check(worst_fb < 0.012,
                 "spike into BOOST_FB at most %.1f mV, below 1 %% of the 1.204 V reference"
                 % (1e3 * worst_fb))
    report.add()

    l1, l2 = positions["L1"], positions["L2"]
    table = []
    for ref, name in (("J2", "thermistor connector"), ("J4", "thermostat connector"),
                      ("R8", "THM bias"), ("U4", "charger"), ("R14", "boost feedback divider"),
                      ("U6", "boost"), ("U8", "gauge"), ("J10", "ADSBee socket"),
                      ("J9", "display socket"), ("J1", "USB-C")):
        if ref in positions:
            table.append([ref + " " + name, "%.0f mm" % math.dist(l1, positions[ref]),
                          "%.0f mm" % math.dist(l2, positions[ref])])
    report.add("Centre distances from the two switching inductors (both magnetically shielded, "
               "molded):")
    report.add()
    report.table(["Part", "from L1 (charger, %.1f µH)" % (CHG["l1"]["l"] * 1e6), "from L2 (boost, 2.2 µH)"], table)
    # magnetic bound: dipole of the inductor ripple current (5 turns of 10 mm², 10 % leakage for a
    # shielded core), axial field at the real centre distance, EMF in a victim loop
    vin = 3.1
    d_boost = 1 - vin / VOUT
    sources = {"L1": (0.5, BUCK_FSW), "L2": (vin * d_boost / (L2 * fsw_boost(vin)), fsw_boost(vin))}
    turns, area = 5, 10e-6

    def pickup(src, ref, loop_mm2):
        ripple, fsw = sources[src]
        r = max(math.dist(positions[src], positions[ref]), 3.0) * 1e-3
        b_field = 4e-7 * math.pi * ripple * turns * area * 0.1 / (2 * math.pi * r ** 3)
        return loop_mm2 * 1e-6 * b_field * 2 * fsw, r * 1e3

    table, worst_thm, worst_fb = [], 0.0, 0.0
    for ref, loop_mm2, what in (("R8", 25, "THM bias network, 25 mm² loop"),
                                ("J2", 500, "thermistor lead pair at J2, untwisted, 500 mm² loop"),
                                ("R14", 10, "boost feedback divider, 10 mm² loop"),
                                ("L2", turns * area * 1e6, "L2 winding (from L1)"),
                                ("L1", turns * area * 1e6, "L1 winding (from L2)")):
        for src in ("L1", "L2"):
            if src == ref or ref not in positions:
                continue
            volts, r_mm = pickup(src, ref, loop_mm2)
            table.append([what, src, "%.0f mm" % r_mm, "%.3f mV" % (1e3 * volts)])
            if ref in ("R8", "J2"):
                worst_thm = max(worst_thm, volts)
            if ref == "R14":
                worst_fb = max(worst_fb, volts)
    report.add("Magnetic pickup bound at the real centre distances (dipole field of the ripple "
               "current, 5 turns of 10 mm², 10 %% leakage of the shielded core; L1 0.5 A at %.1f MHz, "
               "L2 %.2f A at %.0f kHz and %.1f V in):"
               % (BUCK_FSW / 1e6, sources["L2"][0], sources["L2"][1] / 1e3, vin))
    report.add()
    report.table(["Victim", "Source", "Distance", "EMF peak"], table)
    report.check(worst_thm < 2.7e-3, "magnetic pickup into the THM network at most %.2f mV, below 10 %% "
                 "of the 27 mV threshold spacing" % (1e3 * worst_thm))
    report.check(worst_fb < 12e-3, "magnetic pickup into the boost feedback divider at most %.2f mV, "
                 "below 1 %% of the 1.204 V reference" % (1e3 * worst_fb))
    report.add("- L1 and L2: each winding picks up an EMF from the other, which adds to its own ripple. "
               "The two switching frequencies differ, so it shows as a beat on VSYS. The bound is for "
               "the axial direction, the worst case.")
    report.add()


bind_charger()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--write", action="store_true", help="store the report in power/results_<charger>.md")
    parser.add_argument("--fast", action="store_true", help="skip the ngspice benches")
    project.add_argument(parser)
    args = parser.parse_args()
    if args.project:
        project.select(args.project)
        bind_charger()
    version = simlib.require_ngspice()

    report = simlib.Report()
    report.add("# Power converters, simulation results")
    report.add()
    report.add("Generated by `hardware/sim/run_power.py` with %s. See `hardware/sim/README.md` "
               "for what is and is not modelled." % version)
    report.add()
    boost_loop(report)
    if not args.fast:
        boost_ripple(report)
        buck_ripple(report)
        boost_transient(report)
        undervoltage(report)
    layout_coupling(report)
    sys.exit(report.finish(RESULTS if args.write else None))


if __name__ == "__main__":
    main()
