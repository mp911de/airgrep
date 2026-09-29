#!/usr/bin/env python3
"""Thermal charge cut-off verification for the carrier, simulated with ngspice.

Runs the benches in hardware/sim/thermal, evaluates them against the MAX77757 THM
thresholds and the EN/ON thresholds of the boost and load switch, and prints a
Markdown report. The exit code is 1 if a check fails.

    python3 hardware/sim/run_thermal.py            # print the report
    python3 hardware/sim/run_thermal.py --write    # also store thermal/results.md

What is simulated (see thermal/carrier_thermal.inc for the netlist):
  1. The thermistor divider PVL - R8 - THM - NTC over pack temperature, giving
     the THM/PVL ratio the charger compares against its JEITA thresholds.
  2. The thermostat chain VSYS - R43 - thermostat - THERM_OK - Q2 - CHG_DIS - Q6 - THM
     and THERM_OK - slider - R13 - PWR_EN, as DC states and as a transient when the
     contact opens and closes.
The MAX77757 itself is not simulated (no SPICE model exists). Its reaction is taken
from the data sheet thresholds, so the results verify the network around the chip,
not the chip.

Requires ngspice on the PATH (brew install ngspice). Only the standard library is used.
"""

import argparse
import itertools
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import simlib  # noqa: E402

BENCHES = os.path.join(HERE, "thermal")
RESULTS = os.path.join(BENCHES, "results.md")

# MAX77757 THM thresholds as a fraction of PVL: (min, typ, max), 1 % hysteresis.
# Data sheet, Electrical Characteristics. Cold and cool act on a rising ratio
# (temperature falling), warm and hot on a falling ratio (temperature rising).
THRESHOLDS = {
    "cold": (0.725, 0.74, 0.755),
    "cool": (0.635, 0.65, 0.665),
    "warm": (0.31, 0.325, 0.34),
    "hot": (0.215, 0.23, 0.245),
}
CHARGER_DISABLE = (0.045, 0.06, 0.075)
HYSTERESIS = 0.01
EFFECT = {
    "cold": "charging stops below",
    "cool": "half current below",
    "warm": "4.05 V termination above",
    "hot": "charging stops above",
}

# Nominal transitions stated in docs/power.md. The nominal run must reproduce them.
DOCUMENTED = {"cold": -2.9, "cool": 4.6, "warm": 31.4, "hot": 42.1}
PACK_CHARGE_MIN, PACK_CHARGE_MAX = 0.0, 45.0

# Tolerances
NTC_DT = 0.2  # ENTC-EB-100K4262-02 interchangeability, K, 0 to 70 deg C
NTC_BTOL = 0.005  # B25/85 tolerance
BIAS_TOL = 0.01  # R8, 1 %
R8 = 154e3
THM_LEAK_MAX, THM_LEAK_TYP = 1e-6, 0.1e-6  # MAX77757 THM input leakage
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


def part_corners(leakages=((0.0, 0.0),)):
    """All extreme combinations of thermistor offset, B value, bias resistor and leakage."""
    return [{"r8": R8 * (1 + s * BIAS_TOL), "ntc_dt": d * NTC_DT, "ntc_btol": b * NTC_BTOL,
             "ithm": ithm, "q6_idss": idss}
            for d, b, s, (ithm, idss) in itertools.product((-1, 1), (-1, 1), (-1, 1), leakages)]


def transition_ranges(runs):
    """Lowest and highest temperature of each transition over runs and threshold limits."""
    found = {name: [] for name in THRESHOLDS}
    for rows in simulate_all("jeita_sweep", runs):
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
    nominal = simulate("jeita_sweep")
    typ = transitions(nominal)
    table = []
    for name, limits in THRESHOLDS.items():
        rising = name in ("cold", "cool")
        release = crossing(nominal, 1, limits[1] - HYSTERESIS if rising else limits[1] + HYSTERESIS)
        table.append([name + " (" + EFFECT[name] + ")", pct(limits[1]), deg(typ[name]) + " °C",
                      deg(release) + " °C", deg(DOCUMENTED[name]) + " °C"])
    report.add("Nominal parts, typical thresholds, no leakage:")
    report.add()
    report.table(["Transition", "THM/PVL", "Acts at", "Releases at", "docs/power.md"], table)
    report.check(all(abs(typ[n] - DOCUMENTED[n]) <= 0.1 for n in THRESHOLDS),
                 "nominal transitions match docs/power.md within 0.1 °C")
    report.add()

    no_leak = transition_ranges(part_corners())
    typical = transition_ranges(part_corners(((-THM_LEAK_TYP, 0.0), (THM_LEAK_TYP, Q6_IDSS_TYP))))
    # THM pin leakage in either direction; Q6 off-state leakage only pulls THM down
    maximum = transition_ranges(part_corners(((-THM_LEAK_MAX, 0.0), (THM_LEAK_MAX, Q6_IDSS_MAX))))
    report.add("Worst case in °C. Parts and thresholds: thermistor ±0.2 K and B ±0.5 %, bias "
               "resistor ±1 %, charger thresholds at their data sheet minimum and maximum. "
               "Leakage: THM pin 0.1 µA typical and 1 µA maximum, Q6 off-state 10 nA typical "
               "and 1 µA maximum.")
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
    report.add("- With leakage at the data sheet maximum: charging up to %s °C and down to %s °C."
               % (deg(max(no_leak["hot"] + maximum["hot"])), deg(min(no_leak["cold"] + maximum["cold"]))))
    report.add()

    currents = (-THM_LEAK_MAX, -THM_LEAK_TYP, THM_LEAK_TYP, THM_LEAK_MAX, THM_LEAK_MAX + Q6_IDSS_MAX)
    table = []
    for current, rows in zip(currents, simulate_all("jeita_sweep", [{"ithm": i} for i in currents])):
        found = transitions(rows)
        table.append([simlib.minus("%+.1f µA" % (1e6 * current))]
                     + [deg(found[name]) for name in THRESHOLDS])
    report.add("Leakage alone, nominal parts and typical thresholds, °C. Positive current flows out "
               "of the THM node to ground (THM pin or Q6 drain), negative current into it. The "
               "THM pin is specified at 0.1 µA typical, 1 µA maximum:")
    report.add()
    report.table(["THM node leakage"] + list(THRESHOLDS), table)

    if mc_runs:
        monte_carlo(report, mc_runs, seed)


def monte_carlo(report, count, seed):
    rng = random.Random(seed)
    runs, limits = [], []
    for _ in range(count):
        runs.append({
            "ntc_dt": rng.uniform(-NTC_DT, NTC_DT),
            "ntc_btol": rng.uniform(-NTC_BTOL, NTC_BTOL),
            "r8": R8 * (1 + rng.uniform(-BIAS_TOL, BIAS_TOL)),
            "ithm": rng.uniform(-THM_LEAK_TYP, THM_LEAK_TYP),
            "q6_idss": Q6_IDSS_TYP,
        })
        limits.append({n: rng.uniform(lo, hi) for n, (lo, _, hi) in THRESHOLDS.items()})
    found = {name: [] for name in THRESHOLDS}
    for rows, limit in zip(simulate_all("jeita_sweep", runs), limits):
        for name in THRESHOLDS:
            found[name].append(crossing(rows, 1, limit[name]))
    table = []
    for name, temps in found.items():
        temps.sort()
        quantile = lambda q: deg(temps[min(count - 1, int(q * count))])
        table.append([name, deg(temps[0]), quantile(0.01), quantile(0.5), quantile(0.99), deg(temps[-1])])
    report.add("Monte Carlo, %d units, seed %d, °C. Every tolerance is uniform over its full range: "
               "thermistor ±0.2 K and B ±0.5 %%, bias ±1 %%, thresholds between minimum and "
               "maximum. THM leakage is within the typical ±0.1 µA, Q6 leakage is the typical 10 nA."
               % (count, seed))
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
    leak = {"ithm": THM_LEAK_MAX, "q6_idss": Q6_IDSS_MAX}
    unplugged = min(row[1] for row in simulate("jeita_sweep", ntc_r25=1e12, **leak))
    shorted = max(row[1] for row in simulate("jeita_sweep", ntc_r25=1.0))
    report.check(unplugged > THRESHOLDS["cold"][2],
                 "unplugged: THM/PVL = %s with 2 µA leakage pulling down, above the highest cold "
                 "threshold (%s), charging suspended" % (pct(unplugged), pct(THRESHOLDS["cold"][2])))
    report.check(shorted < CHARGER_DISABLE[0],
                 "shorted: THM/PVL = %s, below the lowest disable threshold (%s), charger disabled"
                 % (pct(shorted, 2), pct(CHARGER_DISABLE[0])))
    report.add()


def q2_leakage(report):
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
    results = simulate_all("jeita_sweep",
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
                                   (Q6_VTH[0], Q6_VTH[2]), (0.0, 40.0)))
    results = simulate_all("thermostat_trip",
                           [{"vsys": v, "q2_vth": q2, "q6_vth": q6, "tntc": t} for v, q2, q6, t in cases])
    m = {key: [] for key in ("en_closed", "ratio_open", "en_open", "disabled", "en_high", "en_low",
                             "charge_back", "en_back")}
    before, settled = T_OPEN - 1e-3, T_CLOSE - 10e-3
    for rows in results:
        # columns: time, THERM_OK, CHG_DIS, THM/PVL, PWR_EN
        m["en_closed"].append(value_at(rows, before, 4))
        m["ratio_open"].append(value_at(rows, settled, 3))
        m["en_open"].append(value_at(rows, settled, 4))
        m["disabled"].append(crossing(rows, 3, CHARGER_DISABLE[0], T_OPEN) - T_OPEN)
        m["en_high"].append(crossing(rows, 4, EN_HIGH, T_OPEN) - T_OPEN)
        m["en_low"].append(crossing(rows, 4, EN_LOW, T_OPEN) - T_OPEN)
        restored = value_at(rows, before, 3) - 0.005
        m["charge_back"].append(crossing(rows, 3, restored, T_CLOSE) - T_CLOSE)
        m["en_back"].append(crossing(rows, 4, EN_HIGH, T_CLOSE) - T_CLOSE)
    report.add("%d corners: VSYS 3.0 / 3.5 / 4.4 V, Q2 and Q6 at minimum and maximum VGS(th), "
               "thermistor at 0 and 40 °C, slider on." % len(cases))
    report.add()
    report.check(max(m["ratio_open"]) < CHARGER_DISABLE[0],
                 "open: THM/PVL at most %s, below the lowest disable threshold (%s), charger disabled"
                 % (pct(max(m["ratio_open"]), 3), pct(CHARGER_DISABLE[0])))
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
        ["THM below %s (charger disabled)" % pct(CHARGER_DISABLE[0]), ms_span(m["disabled"])],
        ["PWR_EN below %.1f V (loads may switch off)" % EN_HIGH, ms_span(m["en_high"])],
        ["PWR_EN below %.1f V (loads off)" % EN_LOW, ms_span(m["en_low"])],
        ["PWR_EN between %.1f and %.1f V (either IC may be on or off)" % (EN_HIGH, EN_LOW),
         ms_span(dwell)],
    ])
    report.table(["After the contact closes", "Delay"], [
        ["THM back within 0.5 % of PVL (charging resumes)", ms_span(m["charge_back"])],
        ["PWR_EN above %.1f V (loads on)" % EN_HIGH, ms_span(m["en_back"])],
    ])


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--write", action="store_true", help="store the report in thermal/results.md")
    parser.add_argument("--mc", type=int, default=400, metavar="N", help="Monte Carlo units (0 = skip)")
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    version = simlib.require_ngspice()

    report = simlib.Report()
    report.add("# Thermal charge cut-off, simulation results")
    report.add()
    report.add("Generated by `hardware/sim/run_thermal.py` with %s. The MAX77757 is represented by "
               "its data sheet THM thresholds, see `hardware/sim/README.md`."
               % version)
    report.add()
    fet_models(report)
    jeita_window(report, args.mc, args.seed)
    sensor_faults(report)
    q2_leakage(report)
    thermostat_trip(report)
    sys.exit(report.finish(RESULTS if args.write else None))


if __name__ == "__main__":
    main()
