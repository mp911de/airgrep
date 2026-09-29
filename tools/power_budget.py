#!/usr/bin/env python3
"""Power budget, runtime and charge-time calculation for the Flybee carrier.

Reads specs/power_params.json, prints the results as Markdown and replaces the
generated block in docs/specs.md (between the BEGIN/END GENERATED markers).

    python3 tools/power_budget.py            # print and update docs/specs.md
    python3 tools/power_budget.py --print    # print only

Model, per scenario:
  1. Sum the 5 V loads (modules, display, carrier LEDs) plus GNSS power drawn
     through the ADSBee 3.3 V output (treated as a linear regulator).
  2. Add the load-switch loss, divide by the boost efficiency at that output
     current (interpolated from the TI curve in the parameter file).
  3. Add battery-side quiescent currents and the I^2 R loss of the battery path
     (connector, fuse, charger BATFET), solved for the battery current.
  4. Runtime = rated capacity x usable fraction x nominal voltage / battery power.
Charging: with the device off the full charge current applies (limited by the
input current limit and charger efficiency); with the device on, the system
power is taken from the charger first and only the remainder charges the cell.
Only the standard library is used.
"""

import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PARAMS = os.path.join(ROOT, "specs", "power_params.json")
SPECS_DOC = os.path.join(ROOT, "docs", "specs.md")
BEGIN = "<!-- BEGIN GENERATED: tools/power_budget.py -->"
END = "<!-- END GENERATED: tools/power_budget.py -->"


def interpolate(curve, x):
    """Linear interpolation in a list of [x, y] points, clamped at the ends."""
    if x <= curve[0][0]:
        return curve[0][1]
    for (x0, y0), (x1, y1) in zip(curve, curve[1:]):
        if x <= x1:
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
    return curve[-1][1]


def gnss_ma(p, fix):
    g = p["gnss"]
    module = g["module_tracking_ma"] if fix else g["module_acquisition_ma"]
    return module + g["breakout_led_ma"] + g["active_antenna_ma"]


def rail_loads(p, wifi, fix, brightness):
    """Return [(name, mA at 5 V)] for one scenario."""
    rows = []
    for load in p["loads_5v"]:
        ma = load["ma"]
        if wifi and "wifi_on_ma" in load:
            ma = load["wifi_on_ma"]
        if load.get("scales_with_brightness"):
            ma *= brightness
        rows.append((load["name"], ma))
    rows.append(("GNSS: MAX-M10S, breakout LED, active antenna (" + ("tracking" if fix else "acquisition, no fix") + ")", gnss_ma(p, fix)))
    return rows


def battery_power(p, i5_ma):
    """Battery power (W) and battery current (A) for a 5 V rail current in mA."""
    v5 = p["rail_5v_v"]
    i5 = i5_ma / 1000.0
    p_out = v5 * i5 + i5 * i5 * p["load_switch_ron_ohm"]
    eff = interpolate(p["boost_efficiency_curve"], i5)
    p_boost_in = p_out / eff
    vb = p["battery_nominal_v"]
    iq = p["battery_side_quiescent_ma"] / 1000.0
    ib = (p_boost_in / vb) + iq
    for _ in range(20):  # solve for the I^2 R loss of the battery path
        p_batt = p_boost_in + iq * vb + ib * ib * p["path_resistance_ohm"]
        ib = p_batt / vb
    return p_batt, ib, eff


def scenario_table(p):
    out = []
    for brightness in p["brightness_levels"]:
        for wifi in (False, True):
            for fix in (True, False):
                rows = rail_loads(p, wifi, fix, brightness)
                i5 = sum(ma for _, ma in rows)
                p_batt, ib, eff = battery_power(p, i5)
                out.append({"brightness": brightness, "wifi": wifi, "fix": fix, "i5_ma": i5,
                            "p5_w": p["rail_5v_v"] * i5 / 1000.0, "eff": eff,
                            "p_batt_w": p_batt, "i_batt_a": ib})
    return out


def runtime_h(p, battery, p_batt_w, aged=False):
    cap = battery["capacity_mah"] / 1000.0 * p["usable_fraction"]
    if aged:
        cap *= p["aged_capacity_fraction"]
    return cap * p["battery_nominal_v"] / p_batt_w


def charge_hours(p, battery, system_w):
    """Charge time from empty and the charge current, for each source."""
    c = p["charging"]
    cap = battery["capacity_mah"] / 1000.0
    res = []
    for src in c["sources"]:
        p_avail = c["input_v"] * src["input_limit_a"] * c["charger_efficiency"]
        p_for_cell = p_avail - system_w
        i_chg = min(c["charge_current_a"], max(0.0, p_for_cell / p["battery_nominal_v"]))
        if i_chg <= 0.05:
            res.append((src, i_chg, None))
            continue
        t = cap * c["cc_fraction"] / i_chg + cap * c["cv_tail_hours_per_ah"]
        res.append((src, i_chg, t))
    return res


def fmt_h(h):
    if h is None:
        return "does not charge"
    hours = int(h)
    minutes = int(round((h - hours) * 60))
    if minutes == 60:
        hours, minutes = hours + 1, 0
    return f"{hours} h {minutes:02d} min"


def label(s):
    return f"Wi-Fi {'on' if s['wifi'] else 'off'}, GNSS {'fix' if s['fix'] else 'no fix'}"


def render(p):
    lines = []
    scen = scenario_table(p)
    bats = p["batteries"]
    ref = next(s for s in scen if s["brightness"] == p["brightness_levels"][0] and not s["wifi"] and s["fix"])

    lines.append("### Usage matrix and runtime")
    lines.append("")
    lines.append(f"Runtime from a full charge to supervisor cut-off, {int(p['usable_fraction']*100)} % of rated capacity usable; the aged column assumes {int(p['aged_capacity_fraction']*100)} % remaining capacity. Display Wi-Fi off in all cases (the display talks to the ADSBee over UART).")
    lines.append("")
    for brightness in p["brightness_levels"]:
        lines.append(f"**Backlight {int(brightness*100)} %**")
        lines.append("")
        head = "| Scenario | 5 V rail | Boost eff. | Battery power | " + " | ".join(f"{b['capacity_mah']} mAh (new / aged)" for b in bats) + " |"
        lines.append(head)
        lines.append("|---|---|---|---|" + "---|" * len(bats))
        for s in scen:
            if s["brightness"] != brightness:
                continue
            rts = " | ".join(f"{runtime_h(p, b, s['p_batt_w']):.1f} h / {runtime_h(p, b, s['p_batt_w'], aged=True):.1f} h" for b in bats)
            lines.append(f"| {label(s)} | {s['i5_ma']:.0f} mA, {s['p5_w']:.2f} W | {s['eff']*100:.0f} % | {s['p_batt_w']:.2f} W ({s['i_batt_a']*1000:.0f} mA at {p['battery_nominal_v']} V) | {rts} |")
        lines.append("")

    lines.append("### Component breakdown (backlight 100 %, Wi-Fi off, GNSS fix)")
    lines.append("")
    lines.append("| Load on the 5 V rail | Current | Source |")
    lines.append("|---|---|---|")
    srcs = {l["name"]: l["source"] for l in p["loads_5v"]}
    for name, ma in rail_loads(p, False, True, p["brightness_levels"][0]):
        src = srcs.get(name, p["gnss"]["_module"] + "; " + p["gnss"]["_active_antenna_ma"])
        lines.append(f"| {name} | {ma:.1f} mA | {src} |")
    lines.append(f"| **Total at {p['rail_5v_v']} V** | **{ref['i5_ma']:.0f} mA** | |")
    lines.append("")
    lines.append(f"Wi-Fi on raises the ADSBee from {p['loads_5v'][0]['ma']} mA to {p['loads_5v'][0]['wifi_on_ma']} mA. No GNSS fix raises the GNSS draw by {p['gnss']['module_acquisition_ma']-p['gnss']['module_tracking_ma']:.1f} mA (continuous acquisition), which is below 1 % of the total.")
    lines.append("")

    off_ua = sum(p["off_state_ua"].values())
    lines.append("### Standby (POWER slider off, no USB)")
    lines.append("")
    lines.append("| Source | Current |")
    lines.append("|---|---|")
    for k, v in p["off_state_ua"].items():
        lines.append(f"| {k} | {v:g} µA |")
    lines.append(f"| **Total** | **{off_ua:.0f} µA** |")
    lines.append("")
    for b in bats:
        months = b["capacity_mah"] / (off_ua / 1000.0) / 24 / 30.4
        pct = off_ua / 1000.0 * 24 * 30.4 / b["capacity_mah"] * 100
        lines.append(f"- {b['capacity_mah']} mAh: {pct:.1f} % per month from the circuit; a full pack would last about {months/12:.0f} years (cell self-discharge dominates).")
    lines.append("")

    typ = next(s for s in scen if s["brightness"] == p["brightness_levels"][-1] and not s["wifi"] and s["fix"])
    lines.append("### Charging time from empty")
    lines.append("")
    lines.append(f"Charge current is limited to {p['charging']['charge_current_a']} A (IFAST) and by the source's input limit. \"Device on\" uses the typical scenario (Wi-Fi off, GNSS fix, backlight {int(p['brightness_levels'][-1]*100)} %, {typ['p_batt_w']:.2f} W); the charger powers the system first and only the rest charges the cell. Charge temperature window 0 to 45 °C (NTC, MAX77757 JEITA thresholds; see docs/power.md).")
    lines.append("")
    head = "| Source | Input limit | " + " | ".join(f"{b['capacity_mah']} mAh off / on" for b in bats) + " |"
    lines.append(head)
    lines.append("|---|---|" + "---|" * len(bats))
    for i, src in enumerate(p["charging"]["sources"]):
        cells = []
        for b in bats:
            off = charge_hours(p, b, 0.0)[i]
            on = charge_hours(p, b, typ["p_batt_w"])[i]
            cells.append(f"{fmt_h(off[2])} ({off[1]:.2f} A) / {fmt_h(on[2])} ({on[1]:.2f} A)")
        lines.append(f"| {src['name']} | {src['input_limit_a']:.2f} A | " + " | ".join(cells) + " |")
    lines.append("")
    lines.append("With Wi-Fi on the system needs more than a PC port can supply: the battery then supplements the input and slowly discharges.")
    lines.append("")
    lines.append(f"Parameters: `specs/power_params.json`. Regenerate with `python3 tools/power_budget.py`.")
    return "\n".join(lines)


def update_doc(text):
    with open(SPECS_DOC) as f:
        doc = f.read()
    if BEGIN not in doc or END not in doc:
        sys.exit(f"markers not found in {SPECS_DOC}")
    head, rest = doc.split(BEGIN, 1)
    _, tail = rest.split(END, 1)
    with open(SPECS_DOC, "w") as f:
        f.write(head + BEGIN + "\n" + text + "\n" + END + tail)


def main():
    with open(PARAMS) as f:
        p = json.load(f)
    text = render(p)
    print(text)
    if "--print" not in sys.argv:
        update_doc(text)
        print(f"\nupdated {os.path.relpath(SPECS_DOC, ROOT)}", file=sys.stderr)


if __name__ == "__main__":
    main()
