#!/usr/bin/env python3
"""Thermal charge cut-off verification for the carrier, simulated with ngspice.

Runs the benches in hardware/sim/thermal, evaluates them against the charger's thermistor
thresholds and the EN/ON thresholds of the boost and load switch, and prints a Markdown
report. The exit code is 1 if a check fails.

    python3 hardware/sim/run_thermal.py            # print the report
    python3 hardware/sim/run_thermal.py --write    # also store thermal/results_<charger>.md
    python3 hardware/sim/run_thermal.py --project carrier-max-charger

What is simulated (see thermal/carrier_thermal.inc and carrier_thermal_ti.inc):
  1. The thermistor divider over pack temperature: MAX77757 PVL - R8 - THM - NTC, or BQ25798
     REGN - RT1 - TS - RT2 || NTC, giving the ratio the charger compares against its thresholds.
  2. The thermostat chain VSYS - R43 - thermostat - THERM_OK - Q2 - CHG_DIS and
     THERM_OK - slider - R13 - PWR_EN, as DC states and as a transient when the contact opens
     and closes. CHG_DIS drives Q6 onto THM (MAX77757) or the CE pin (BQ25798).
The charger itself is not simulated (no SPICE model exists). Its reaction is taken from the
data sheet thresholds, so the results verify the network around the chip, not the chip.

Requires ngspice on the PATH (brew install ngspice). Only the standard library is used.
"""

import argparse
import itertools
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "..", "tools"))
import project  # noqa: E402
import simlib  # noqa: E402

BENCHES = os.path.join(HERE, "thermal")

# Per charger: thresholds as a fraction of the bias rail (min, typ, max), what each one does, the
# nominal transitions documented in docs/power.md, the bias resistors and the bench names.
# MAX77757 (THM/PVL, data sheet Electrical Characteristics, 1 % hysteresis): cold and cool act on a
# rising ratio (temperature falling), warm and hot on a falling ratio (temperature rising).
# BQ25798 (TS/REGN, SLUSDV2C Electrical Characteristics and REG18 defaults): T1 cold 73.3 %, T2 cool
# 68.4 % (10 C option), T3 warm 44.8 % (45 C option), T5 hot 34.2 %, about 1.3 points of hysteresis.
# The T2/T3 spreads are the ±0.5 point of the specified rows; T1/T5 are specified.
CHARGERS = {
    "max": dict(part="MAX77757", rail="PVL", pin="THM", include="carrier_thermal.inc", sweep="jeita_sweep",
                trip="thermostat_trip", thresholds={"cold": (0.725, 0.74, 0.755), "cool": (0.635, 0.65, 0.665),
                                                    "warm": (0.31, 0.325, 0.34), "hot": (0.215, 0.23, 0.245)},
                disable=(0.045, 0.06, 0.075), hysteresis=0.01,
                effect={"cold": "charging stops below", "cool": "half current below",
                        "warm": "4.05 V termination above", "hot": "charging stops above"},
                documented={"cold": -2.9, "cool": 4.6, "warm": 31.4, "hot": 42.1},
                bias={"r8": 154e3}, pull_down="Q6 2N7002 onto THM"),
    "ti": dict(part="BQ25798", rail="REGN", pin="TS", include="carrier_thermal_ti.inc", sweep="ts_sweep_ti",
               trip="thermostat_trip_ti", thresholds={"cold": (0.724, 0.733, 0.742), "cool": (0.679, 0.684, 0.689),
                                                      "warm": (0.443, 0.448, 0.453), "hot": (0.337, 0.342, 0.347)},
               disable=None, hysteresis=0.013,
               effect={"cold": "charging stops below", "cool": "20 % current below (JEITA_ISETC default)",
                       "warm": "VREG - 400 mV above (JEITA_VSET default)", "hot": "charging stops above"},
               documented={"cold": 2.7, "cool": 8.5, "warm": 31.8, "hot": 42.6},
               bias={"rt1": 82e3, "rt2": 768e3}, pull_down="the CE pin"),
}
CE_VIH, CE_VIL = 1.3, 0.4  # BQ25798 CE logic thresholds


def select(tag=None):
    global TAG, CHG, RESULTS, THRESHOLDS, CHARGER_DISABLE, HYSTERESIS, EFFECT, DOCUMENTED, BIAS
    TAG = tag or project.charger(project.resolve())
    CHG = CHARGERS[TAG]
    RESULTS = os.path.join(BENCHES, "results_%s.md" % TAG)
    THRESHOLDS, CHARGER_DISABLE, HYSTERESIS = CHG["thresholds"], CHG["disable"], CHG["hysteresis"]
    EFFECT, DOCUMENTED, BIAS = CHG["effect"], CHG["documented"], CHG["bias"]


select()
PACK_CHARGE_MIN, PACK_CHARGE_MAX = 0.0, 45.0

# Tolerances
NTC_DT = 0.2  # ENTC-EB-100K4262-02 interchangeability, K, 0 to 70 deg C
NTC_BTOL = 0.005  # B25/85 tolerance
BIAS_TOL = 0.01  # bias resistors, 1 %
TI_BIAS_TCR = 100e-6  # R59 specified; R60 assumed pending manufacturer qualification
TI_BIAS_TEMP = (-40.0, 125.0)  # board temperature, independent of pack temperature
R8 = 154e3  # MAX77757 bias resistor; the design-check binding reads this literal
THM_LEAK_MAX, THM_LEAK_TYP = 1e-6, 0.1e-6  # MAX77757 THM input leakage; assumed the same for the BQ25798 TS pin
Q6_IDSS_MAX, Q6_IDSS_TYP = 1e-6, 10e-9  # 2N7002, 25 deg C
Q2_IDSS_25, Q2_IDSS_55 = 1e-6, 5e-6  # AO3401A maximum at 25 and 55 deg C
Q2_VTH = (0.5, 0.9, 1.3)
Q6_VTH = (1.0, 2.0, 2.5)

# Enable thresholds on PWR_EN: TPS61088 EN 0.4 / 1.2 V, TPS22975 ON 0.5 / 1.2 V
EN_LOW, EN_HIGH = 0.4, 1.2
T_OPEN, T_CLOSE = 10e-3, 600e-3  # thermostat events in thermostat_trip.cir

BENCH = simlib.Bench(BENCHES)
simulate, simulate_all = BENCH.simulate, BENCH.simulate_all
crossing, value_at, deg, pct = simlib.crossing, simlib.value_at, simlib.deg, simlib.pct


def span(values):
    if any(v is None for v in values):
        return "not reached"
    return "%s to %s" % (deg(min(values)), deg(max(values)))


def ms_span(values):
    low, high = ("%.1f" % (1e3 * v) for v in (min(values), max(values)))
    if float(high) < 0.1:
        return "under 0.1 ms"
    return "%s ms" % low if low == high else "%s to %s ms" % (low, high)


def transitions(rows, pick=1):
    """Temperature of each JEITA transition for threshold min (0), typ (1) or max (2)."""
    return {name: crossing(rows, 1, limits[pick]) for name, limits in THRESHOLDS.items()}


def bias_corners(drift=False):
    """Independent resistor extremes; optional TI temperature-drift screening."""
    names = sorted(BIAS)
    delta = max(abs(t - 25.0) for t in TI_BIAS_TEMP) if drift else 0.0
    return [{n: BIAS[n] * (1 + s * BIAS_TOL) * (1 + s * TI_BIAS_TCR * delta)
             for n, s in zip(names, signs)}
            for signs in itertools.product((-1, 1), repeat=len(names))]


def leak_params(ithm, idss):
    return {"ithm": ithm, "q6_idss": idss} if TAG == "max" else {"ithm": ithm}


def part_corners(leakages=((0.0, 0.0),), drift=False):
    """All extreme combinations of thermistor offset, B value, bias resistors and leakage."""
    return [dict(bias, ntc_dt=d * NTC_DT, ntc_btol=b * NTC_BTOL, **leak_params(ithm, idss))
            for d, b, bias, (ithm, idss) in itertools.product((-1, 1), (-1, 1), bias_corners(drift), leakages)]


def transition_ranges(runs):
    """Lowest and highest temperature of each transition over runs and threshold limits."""
    found = {name: [] for name in THRESHOLDS}
    for rows in simulate_all(CHG["sweep"], runs):
        for pick in (0, 2):
            for name, t in transitions(rows, pick).items():
                found[name].append(t)
    return found


def fet_models(report):
    report.add("## FET models against their data sheet points")
    report.add()
    table = []
    ok = True
    for q2, q6 in zip(Q2_VTH, Q6_VTH):
        rows = simulate("fet_check", q2_vth=q2, q6_vth=q6)
        for name, col, target in (("Q2 AO3401A", 2, q2), ("Q6 2N7002", 1, q6)):
            v250 = crossing(rows, col, 250e-6)
            swing = crossing(rows, col, 10e-9) - crossing(rows, col, 1e-9)
            ok = ok and abs(v250 - target) < 0.01 and 0.085 < swing < 0.095
            table.append([name, "%.2f V" % target, "%.3f V" % v250, "%.0f mV/decade" % (1e3 * swing)])
    report.table(["FET", "VGS(th) data sheet", "Model gate voltage at 250 µA", "Subthreshold swing"],
                 sorted(table))
    report.check(ok, "models carry 250 µA within 10 mV of the data sheet threshold, "
                     "swing 90 mV/decade (assumed, not a data sheet value)")
    report.add()


def jeita_window(report, mc_runs, seed):
    report.add("## JEITA window from the thermistor divider")
    report.add()
    nominal = simulate(CHG["sweep"])
    typ = transitions(nominal)
    table = []
    for name, limits in THRESHOLDS.items():
        rising = name in ("cold", "cool")
        release = crossing(nominal, 1, limits[1] - HYSTERESIS if rising else limits[1] + HYSTERESIS)
        table.append([name + " (" + EFFECT[name] + ")", pct(limits[1]), deg(typ[name]) + " °C",
                      deg(release) + " °C", deg(DOCUMENTED[name]) + " °C"])
    report.add("Nominal parts, typical thresholds, no leakage:")
    report.add()
    report.table(["Transition", "%s/%s" % (CHG["pin"], CHG["rail"]), "Acts at", "Releases at", "docs/power.md"], table)
    report.check(all(abs(typ[n] - DOCUMENTED[n]) <= 0.1 for n in THRESHOLDS),
                 "nominal transitions match docs/power.md within 0.1 °C")
    report.add()

    no_leak = transition_ranges(part_corners())
    typical = transition_ranges(part_corners(((-THM_LEAK_TYP, 0.0), (THM_LEAK_TYP, Q6_IDSS_TYP))))
    # THM pin leakage in either direction; Q6 off-state leakage only pulls THM down
    maximum = transition_ranges(part_corners(((-THM_LEAK_MAX, 0.0), (THM_LEAK_MAX, Q6_IDSS_MAX))))
    report.add("Worst case in °C. Parts and thresholds: thermistor ±0.2 K and B ±0.5 %%, bias "
               "resistor%s ±1 %%, charger thresholds at their data sheet minimum and maximum. "
               "Leakage: %s pin 0.1 µA typical and 1 µA maximum%s."
               % ("s" if len(BIAS) > 1 else "", CHG["pin"],
                  ", Q6 off-state 10 nA typical and 1 µA maximum" if TAG == "max" else " (assumed, not specified)"))
    report.add()
    report.table(["Transition", "Nominal", "Parts and thresholds", "plus typical leakage",
                  "plus maximum leakage"],
                 [[name, deg(typ[name]), span(no_leak[name]), span(no_leak[name] + typical[name]),
                   span(no_leak[name] + maximum[name])] for name in THRESHOLDS])
    hot = no_leak["hot"] + typical["hot"]
    cold = no_leak["cold"] + typical["cold"]
    report.check(max(hot) <= PACK_CHARGE_MAX,
                 "hot cut-off at most %s °C with typical leakage, within the pack's %s °C charge rating"
                 % (deg(max(hot)), deg(PACK_CHARGE_MAX)))
    report.add("- Cold cut-off: %s to %s °C with typical leakage against the pack's %s °C rating. "
               "Accepted, the window width follows from the thermistor's B value (docs/power.md)."
               % (deg(min(cold)), deg(max(cold)), deg(PACK_CHARGE_MIN)))
    report.add(("- With leakage at the data sheet maximum: " if TAG == "max" else
                "- With assumed ±1 µA TS leakage: ") + "charging up to %s °C and down to %s °C."
               % (deg(max(no_leak["hot"] + maximum["hot"])), deg(min(no_leak["cold"] + maximum["cold"]))))
    report.add()

    if TAG == "ti":
        drift = transition_ranges(part_corners(((-THM_LEAK_TYP, 0.0), (THM_LEAK_TYP, 0.0)), drift=True))
        report.add("Temperature-drift screening: independent ±100 ppm/°C for R59 and R60 over "
                   "−40 to +125 °C board temperature, referenced to 25 °C, plus part/threshold "
                   "corners and assumed ±0.1 µA TS leakage. R59 specifies this TCR; R60's TCR "
                   "is assumed pending manufacturer qualification. Board temperature is independent "
                   "of pack temperature. These bounds are conditional, not guaranteed pack limits.")
        report.add()
        report.table(["Transition", "Including resistor drift, °C"],
                     [[name, span(drift[name])] for name in THRESHOLDS])
        report.add()

    currents = (-THM_LEAK_MAX, -THM_LEAK_TYP, THM_LEAK_TYP, THM_LEAK_MAX, THM_LEAK_MAX + Q6_IDSS_MAX)
    table = []
    for current, rows in zip(currents, simulate_all(CHG["sweep"], [{"ithm": i} for i in currents])):
        found = transitions(rows)
        table.append([simlib.minus("%+.1f µA" % (1e6 * current))]
                     + [deg(found[name]) for name in THRESHOLDS])
    report.add("Leakage alone, nominal parts and typical thresholds, °C. Positive current flows out "
               "of the %s node to ground (%s pin%s), negative current into it. The "
               "%s pin is %s at 0.1 µA typical, 1 µA maximum:"
               % (CHG["pin"], CHG["pin"], " or Q6 drain" if TAG == "max" else "", CHG["pin"],
                  "specified" if TAG == "max" else "assumed"))
    report.add()
    report.table(["%s node leakage" % CHG["pin"]] + list(THRESHOLDS), table)

    if mc_runs:
        monte_carlo(report, mc_runs, seed)


def monte_carlo(report, count, seed):
    rng = random.Random(seed)
    runs, limits = [], []
    for _ in range(count):
        run = {"ntc_dt": rng.uniform(-NTC_DT, NTC_DT), "ntc_btol": rng.uniform(-NTC_BTOL, NTC_BTOL),
               **leak_params(rng.uniform(-THM_LEAK_TYP, THM_LEAK_TYP), Q6_IDSS_TYP)}
        run.update({n: v * (1 + rng.uniform(-BIAS_TOL, BIAS_TOL)) for n, v in BIAS.items()})
        runs.append(run)
        limits.append({n: rng.uniform(lo, hi) for n, (lo, _, hi) in THRESHOLDS.items()})
    found = {name: [] for name in THRESHOLDS}
    for rows, limit in zip(simulate_all(CHG["sweep"], runs), limits):
        for name in THRESHOLDS:
            found[name].append(crossing(rows, 1, limit[name]))
    table = []
    for name, temps in found.items():
        temps.sort()
        quantile = lambda q: deg(temps[min(count - 1, int(q * count))])
        table.append([name, deg(temps[0]), quantile(0.01), quantile(0.5), quantile(0.99), deg(temps[-1])])
    report.add("Monte Carlo, %d units, seed %d, °C. Every tolerance is uniform over its full range: "
               "thermistor ±0.2 K and B ±0.5 %%, bias ±1 %%, thresholds between minimum and "
               "maximum. %s leakage is within the typical ±0.1 µA%s."
               % (count, seed, CHG["pin"], ", Q6 leakage is the typical 10 nA" if TAG == "max" else ""))
    report.add()
    report.table(["Transition", "Lowest", "1 %", "Median", "99 %", "Highest"], table)
    above = sum(t > PACK_CHARGE_MAX for t in found["hot"]) / count
    below = sum(t < PACK_CHARGE_MIN for t in found["cold"]) / count
    report.add("- Units that still charge above %s °C: %s. Units that still charge below %s °C: %s."
               % (deg(PACK_CHARGE_MAX), pct(above, 0), deg(PACK_CHARGE_MIN), pct(below, 0)))
    report.add()


def sensor_faults(report):
    report.add("## Thermistor unplugged or shorted")
    report.add()
    ratio = "%s/%s" % (CHG["pin"], CHG["rail"])
    leak = leak_params(THM_LEAK_MAX, Q6_IDSS_MAX)
    unplugged = min(row[1] for row in simulate(CHG["sweep"], ntc_r25=1e12, **leak))
    shorted = max(row[1] for row in simulate(CHG["sweep"], ntc_r25=1.0))
    report.check(unplugged > THRESHOLDS["cold"][2],
                 "unplugged: %s = %s with %s leakage pulling down, above the highest cold "
                 "threshold (%s), charging suspended" % (ratio, pct(unplugged), "2 µA" if TAG == "max" else "1 µA",
                                                        pct(THRESHOLDS["cold"][2])))
    if CHARGER_DISABLE:
        report.check(shorted < CHARGER_DISABLE[0],
                     "shorted: %s = %s, below the lowest disable threshold (%s), charger disabled"
                     % (ratio, pct(shorted, 2), pct(CHARGER_DISABLE[0])))
    else:
        report.check(shorted < THRESHOLDS["hot"][0],
                     "shorted: %s = %s, below the lowest hot threshold (%s), charging suspended"
                     % (ratio, pct(shorted, 2), pct(THRESHOLDS["hot"][0])))
    report.add()


def q2_leakage_ce(report):
    """BQ25798: Q2 off-state leakage raises the CE pin through R12; it must stay below VIL."""
    report.add("## Thermostat closed: Q2 leakage into R12 and the CE pin")
    report.add()
    report.add("With the thermostat closed Q2 is off and only R12 (100 kΩ) holds CE (CHG_DIS) low. The "
               "AO3401A data sheet allows 1 µA at 25 °C and 5 µA at 55 °C at VDS = −30 V; at the 3 to 4.4 V "
               "here it is normally far lower. CE must stay below VIL %.1f V, charging is disabled above "
               "VIH %.1f V." % (CE_VIL, CE_VIH))
    report.add()
    cases = [0.1e-6, 0.5e-6, Q2_IDSS_25, Q2_IDSS_55]
    results = simulate_all(CHG["sweep"], [{"q2_idss": i} for i in cases])
    table, worst = [], 0.0
    for idss, rows in zip(cases, results):
        v = value_at(rows, 25.0, 2)
        worst = max(worst, v) if idss <= Q2_IDSS_25 else worst
        table.append(["%.1f µA" % (1e6 * idss), "%.3f V" % v, "below VIL" if v < CE_VIL else
                      ("undefined" if v < CE_VIH else "charging disabled")])
    report.table(["Q2 IDSS", "CE (CHG_DIS)", "State"], table)
    report.check(worst < CE_VIL, "CE at most %.3f V with Q2 leakage at the 25 °C data sheet maximum, below VIL %.1f V; "
                                 "the 55 °C maximum (5 µA, %.1f V) would disable charging" % (worst, CE_VIL, 5e-6 * 100e3))
    report.add()


def q2_leakage(report):
    if TAG != "max":
        return q2_leakage_ce(report)
    report.add("## Thermostat closed: Q2 leakage into R12")
    report.add()
    report.add("With the thermostat closed Q2 is off and only R12 holds the Q6 gate (CHG_DIS) low. "
               "Q2 off-state leakage raises that gate. The AO3401A data sheet allows 1 µA at 25 °C "
               "and 5 µA at 55 °C at VDS = −30 V. At the 3 to 4.4 V present here it is normally "
               "far lower. The 1000 kΩ rows show why R12 is not larger. Typical charger "
               "thresholds, nominal thermistor:")
    report.add()
    reference = value_at(simulate("jeita_sweep"), 25.0, 1)
    # R12 is 100 kOhm on the board; the 1 MOhm rows show why it is not larger
    cases = [(idss, 100e3, vth) for vth in (Q6_VTH[0], Q6_VTH[1])
             for idss in (0.1e-6, 0.5e-6, Q2_IDSS_25, Q2_IDSS_55)]
    cases += [(idss, 1e6, Q6_VTH[0]) for idss in (0.5e-6, Q2_IDSS_25, Q2_IDSS_55)]
    results = simulate_all(CHG["sweep"],
                           [{"q2_idss": i, "r12": r, "q6_vth": v} for i, r, v in cases])
    table, shifts = [], {}
    for (idss, r12, vth), rows in zip(cases, results):
        shift = value_at(rows, 25.0, 1) - reference
        shifts[(idss, r12, vth)] = shift
        found = transitions(rows)
        window = ("blocked at every temperature" if found["hot"] is None or found["cold"] is None
                  else "%s to %s °C" % (deg(found["cold"]), deg(found["hot"])))
        table.append(["%.1f µA" % (1e6 * idss), "%.0f kΩ" % (r12 / 1e3), "%.1f V" % vth,
                      "%.2f V" % value_at(rows, 25.0, 2), pct(shift, 2), window])
    report.table(["Q2 IDSS", "R12", "Q6 VGS(th)", "CHG_DIS", "THM/PVL shift at 25 °C",
                  "Charging window"], table)
    for idss, label in ((Q2_IDSS_25, "1 µA, 25 °C"), (Q2_IDSS_55, "5 µA, 55 °C")):
        shift = shifts[(idss, 100e3, Q6_VTH[0])]
        report.check(abs(shift) < 0.005,
                     "Q2 leakage at the data sheet maximum (%s) with Q6 at minimum VGS(th) moves "
                     "THM/PVL by %s (limit 0.5 %% of PVL, about 0.5 °C)" % (label, pct(shift, 2)))
    report.add()


def thermostat_trip(report):
    report.add("## Thermostat opens and closes")
    report.add()
    cases = list(itertools.product((3.0, 3.5, 4.4), (Q2_VTH[0], Q2_VTH[2]),
                                   (Q6_VTH[0], Q6_VTH[2]) if TAG == "max" else (Q6_VTH[1],), (0.0, 40.0)))
    results = simulate_all(CHG["trip"],
                           [{"vsys": v, "q2_vth": q2, "q6_vth": q6, "tntc": t} for v, q2, q6, t in cases])
    m = {key: [] for key in ("en_closed", "ratio_open", "en_open", "disabled", "en_high", "en_low",
                             "charge_back", "en_back")}
    before, settled = T_OPEN - 1e-3, T_CLOSE - 10e-3
    for rows in results:
        # columns: time, THERM_OK, CHG_DIS, THM/PVL, PWR_EN
        m["en_closed"].append(value_at(rows, before, 4))
        m["ratio_open"].append(value_at(rows, settled, 3))
        m["en_open"].append(value_at(rows, settled, 4))
        if TAG == "max":
            m["disabled"].append(crossing(rows, 3, CHARGER_DISABLE[0], T_OPEN) - T_OPEN)
        else:
            m["disabled"].append(crossing(rows, 2, CE_VIH, T_OPEN) - T_OPEN)
        m["en_high"].append(crossing(rows, 4, EN_HIGH, T_OPEN) - T_OPEN)
        m["en_low"].append(crossing(rows, 4, EN_LOW, T_OPEN) - T_OPEN)
        if TAG == "max":
            restored = value_at(rows, before, 3) - 0.005
            m["charge_back"].append(crossing(rows, 3, restored, T_CLOSE) - T_CLOSE)
        else:
            m["charge_back"].append(crossing(rows, 2, CE_VIL, T_CLOSE) - T_CLOSE)
        m["en_back"].append(crossing(rows, 4, EN_HIGH, T_CLOSE) - T_CLOSE)
    report.add("%d corners: VSYS 3.0 / 3.5 / 4.4 V, Q2%s at minimum and maximum VGS(th), "
               "thermistor at 0 and 40 °C, slider on." % (len(cases), " and Q6" if TAG == "max" else ""))
    report.add()
    if TAG == "max":
        report.check(max(m["ratio_open"]) < CHARGER_DISABLE[0],
                     "open: THM/PVL at most %s, below the lowest disable threshold (%s), charger disabled"
                     % (pct(max(m["ratio_open"]), 3), pct(CHARGER_DISABLE[0])))
    else:
        ce_open = [value_at(rows, settled, 2) for rows in results]
        report.check(min(ce_open) > CE_VIH, "open: CE at least %.2f V, above VIH %.1f V, charging disabled"
                     % (min(ce_open), CE_VIH))
    report.check(max(m["en_open"]) < EN_LOW,
                 "open: PWR_EN at most %.3f V, below %.1f V, boost and load switch off"
                 % (max(m["en_open"]), EN_LOW))
    report.check(min(m["en_closed"]) > EN_HIGH,
                 "closed: PWR_EN at least %.2f V (R43 and R13 against the 800 kΩ EN pull-down), "
                 "above %.1f V"
                 % (min(m["en_closed"]), EN_HIGH))
    report.add()
    dwell = [low - high for low, high in zip(m["en_low"], m["en_high"])]
    report.table(["After the contact opens", "Delay"], [
        ["THM below %s (charger disabled)" % pct(CHARGER_DISABLE[0]) if TAG == "max"
         else "CE above %.1f V (charging disabled)" % CE_VIH, ms_span(m["disabled"])],
        ["PWR_EN below %.1f V (loads may switch off)" % EN_HIGH, ms_span(m["en_high"])],
        ["PWR_EN below %.1f V (loads off)" % EN_LOW, ms_span(m["en_low"])],
        ["PWR_EN between %.1f and %.1f V (either IC may be on or off)" % (EN_HIGH, EN_LOW),
         ms_span(dwell)],
    ])
    report.table(["After the contact closes", "Delay"], [
        ["THM back within 0.5 % of PVL (charging resumes)" if TAG == "max"
         else "CE below %.1f V (charging resumes)" % CE_VIL, ms_span(m["charge_back"])],
        ["PWR_EN above %.1f V (loads on)" % EN_HIGH, ms_span(m["en_back"])],
    ])


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--write", action="store_true", help="store the report in thermal/results_<charger>.md")
    parser.add_argument("--mc", type=int, default=400, metavar="N", help="Monte Carlo units (0 = skip)")
    parser.add_argument("--seed", type=int, default=1)
    project.add_argument(parser)
    args = parser.parse_args()
    if args.project:
        select(project.charger(project.select(args.project)))
    version = simlib.require_ngspice()

    report = simlib.Report()
    report.add("# Thermal charge cut-off, simulation results")
    report.add()
    report.add("Generated by `hardware/sim/run_thermal.py` with %s for the %s board (charger %s). The charger "
               "is represented by its data sheet %s thresholds, see `hardware/sim/README.md`."
               % (version, os.path.basename(project.resolve()), CHG["part"], CHG["pin"]))
    report.add()
    fet_models(report)
    jeita_window(report, args.mc, args.seed)
    sensor_faults(report)
    q2_leakage(report)
    thermostat_trip(report)
    sys.exit(report.finish(RESULTS if args.write else None))


if __name__ == "__main__":
    main()
