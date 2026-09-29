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

This is a budget estimate. The simulated losses on the routed board are in
hardware/sim/heat/results.md. Only the standard library is used.
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


def rail_loads(p, wifi, fix, brightness):
    """Return [(name, mA at 5 V, source)] for one scenario."""
    rows = []
    for load in p["loads_5v"]:
        ma = load["wifi_on_ma"] if wifi and "wifi_on_ma" in load else load["ma"]
        if load.get("scales_with_brightness"):
            ma *= brightness
        rows.append((load["name"], ma, load["source"]))
    g = p["gnss"]
    module = g["module_tracking_ma"] if fix else g["module_acquisition_ma"]
    rows.append((f"GNSS: MAX-M10S, breakout LED, active antenna ({'tracking' if fix else 'acquisition, no fix'})",
                 module + g["breakout_led_ma"] + g["active_antenna_ma"],
                 g["_module"] + "; " + g["_active_antenna_ma"]))
    return rows


def battery_power(p, i5_ma):
    """Battery power (W), battery current (A) and boost efficiency for a 5 V rail current in mA."""
    i5 = i5_ma / 1000.0
    eff = interpolate(p["boost_efficiency_curve"], i5)
    p_boost_in = (p["rail_5v_v"] * i5 + i5 * i5 * p["load_switch_ron_ohm"]) / eff
    vb = p["battery_nominal_v"]
    iq = p["battery_side_quiescent_ma"] / 1000.0
    ib = p_boost_in / vb + iq
    for _ in range(20):  # solve for the I^2 R loss of the battery path
        p_batt = p_boost_in + iq * vb + ib * ib * p["path_resistance_ohm"]
        ib = p_batt / vb
    return p_batt, ib, eff


def scenarios(p):
    """One dict per combination of backlight, Wi-Fi and GNSS fix."""
    out = []
    for brightness in p["brightness_levels"]:
        for wifi in (False, True):
            for fix in (True, False):
                i5 = sum(ma for _, ma, _ in rail_loads(p, wifi, fix, brightness))
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


def charge(p, battery, src, system_w):
    """Charge current (A) and time from empty (h, None if it does not charge) for one source."""
    c = p["charging"]
    p_for_cell = c["input_v"] * src["input_limit_a"] * c["charger_efficiency"] - system_w
    i_chg = min(c["charge_current_a"], max(0.0, p_for_cell / p["battery_nominal_v"]))
    if i_chg <= 0.05:
        return i_chg, None
    cap = battery["capacity_mah"] / 1000.0
    return i_chg, cap * c["cc_fraction"] / i_chg + cap * c["cv_tail_hours_per_ah"]


def pct(fraction, digits=0):
    return f"{fraction * 100:.{digits}f} %"


def fmt_charge(i_chg, hours):
    if hours is None:
        return "does not charge"
    h, m = divmod(round(hours * 60), 60)
    return f"{h} h {m:02d} min ({i_chg:.2f} A)"


def render(p):
    scen = scenarios(p)
    bats = p["batteries"]
    full, dimmed = p["brightness_levels"][0], p["brightness_levels"][-1]
    vb = p["battery_nominal_v"]

    def baseline(brightness):  # Wi-Fi off, GNSS fix
        return next(s for s in scen if s["brightness"] == brightness and not s["wifi"] and s["fix"])

    lines = [
        "### Usage matrix and runtime",
        "",
        f"Runtime from a full charge to the supervisor cut-off, with {pct(p['usable_fraction'])} of the rated capacity usable. "
        f"The aged column assumes {pct(p['aged_capacity_fraction'])} remaining capacity. "
        "The display's Wi-Fi is off in all cases; the display talks to the ADSBee over UART.",
        "",
        "The boost efficiency is a budget estimate from the TI data sheet curve, derated for about 1 MHz. "
        "The heat model simulates the board at about 720 kHz and gives about 1 point more at the typical load (`hardware/sim/heat/results.md`).",
        "",
    ]
    for brightness in p["brightness_levels"]:
        lines += [
            f"**Backlight {pct(brightness)}**",
            "",
            "| Scenario | 5 V rail | Boost eff. | Battery power | " + " | ".join(f"{b['capacity_mah']} mAh (new / aged)" for b in bats) + " |",
            "|---|---|---|---|" + "---|" * len(bats),
        ]
        for s in scen:
            if s["brightness"] != brightness:
                continue
            runtimes = " | ".join(f"{runtime_h(p, b, s['p_batt_w']):.1f} h / {runtime_h(p, b, s['p_batt_w'], aged=True):.1f} h" for b in bats)
            lines.append(f"| Wi-Fi {'on' if s['wifi'] else 'off'}, GNSS {'fix' if s['fix'] else 'no fix'} "
                         f"| {s['i5_ma']:.0f} mA, {s['p5_w']:.2f} W | {pct(s['eff'])} "
                         f"| {s['p_batt_w']:.2f} W ({s['i_batt_a']*1000:.0f} mA at {vb} V) | {runtimes} |")
        lines.append("")

    adsbee, gnss = p["loads_5v"][0], p["gnss"]
    lines += [
        f"### Component breakdown (backlight {pct(full)}, Wi-Fi off, GNSS fix)",
        "",
        "| Load on the 5 V rail | Current | Source |",
        "|---|---|---|",
        *(f"| {name} | {ma:.1f} mA | {source} |" for name, ma, source in rail_loads(p, False, True, full)),
        f"| **Total at {p['rail_5v_v']} V** | **{baseline(full)['i5_ma']:.0f} mA** | |",
        "",
        f"Wi-Fi on raises the ADSBee from {adsbee['ma']} mA to {adsbee['wifi_on_ma']} mA. "
        f"Without a fix the GNSS receiver stays in acquisition and draws {gnss['module_acquisition_ma'] - gnss['module_tracking_ma']:.1f} mA more, below 1 % of the total.",
        "",
    ]

    off_ma = sum(p["off_state_ua"].values()) / 1000.0
    lines += [
        "### Standby (POWER slider off, no USB)",
        "",
        "| Source | Current |",
        "|---|---|",
        *(f"| {name} | {ua:g} µA |" for name, ua in p["off_state_ua"].items()),
        f"| **Total** | **{off_ma * 1000:.0f} µA** |",
        "",
    ]
    for b in bats:
        hours = b["capacity_mah"] / off_ma
        lines.append(f"- {b['capacity_mah']} mAh: the circuit takes {pct(24 * 30.4 / hours, 1)} per month. "
                     f"A full pack would last about {hours / 24 / 30.4 / 12:.0f} years, so cell self-discharge dominates.")
    lines.append("")

    typ = baseline(dimmed)
    lines += [
        "### Charging time from empty",
        "",
        f"The charge current is limited to {p['charging']['charge_current_a']} A (IFAST) and by the input limit of the source. "
        "The charger powers the system first; only the rest charges the cell. "
        f"\"On\" is the typical scenario: Wi-Fi off, GNSS fix, backlight {pct(dimmed)}, {typ['p_batt_w']:.2f} W.",
        "",
        "| Source | Input limit | " + " | ".join(f"{b['capacity_mah']} mAh off / on" for b in bats) + " |",
        "|---|---|" + "---|" * len(bats),
    ]
    for src in p["charging"]["sources"]:
        cells = " | ".join(f"{fmt_charge(*charge(p, b, src, 0.0))} / {fmt_charge(*charge(p, b, src, typ['p_batt_w']))}" for b in bats)
        lines.append(f"| {src['name']} | {src['input_limit_a']:.2f} A | {cells} |")
    lines += [
        "",
        "With Wi-Fi on the system needs more than a PC port can supply. The battery then supplements the input and slowly discharges.",
        "",
        "Parameters: `specs/power_params.json`. Regenerate with `python3 tools/power_budget.py`.",
    ]
    return "\n".join(lines)


def update_doc(text):
    with open(SPECS_DOC, encoding="utf-8") as f:
        doc = f.read()
    if BEGIN not in doc or END not in doc:
        sys.exit(f"markers not found in {SPECS_DOC}")
    head, rest = doc.split(BEGIN, 1)
    tail = rest.split(END, 1)[1]
    with open(SPECS_DOC, "w", encoding="utf-8") as f:
        f.write(head + BEGIN + "\n" + text + "\n" + END + tail)


def main():
    with open(PARAMS, encoding="utf-8") as f:
        text = render(json.load(f))
    print(text)
    if "--print" not in sys.argv:
        update_doc(text)
        print(f"\nupdated {os.path.relpath(SPECS_DOC, ROOT)}", file=sys.stderr)


if __name__ == "__main__":
    main()
