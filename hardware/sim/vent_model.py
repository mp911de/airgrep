"""Lumped vented-enclosure model for the FLARM carrier.

Uses the enclosure wall path of hardware/sim/run_heat.py (case 115 x 105 x 40 mm, 2 mm PETG wall, 7 W/m2K
inside and 9 W/m2K outside) and adds two paths in parallel with the wall: a natural convection vent
(stack effect, inlet low, outlet high) and a fan. Steady state. Estimate only: the enclosure does not
exist yet.

The scenario constants come from heat/<charger>/calibration.json, written by run_heat.py --write (sealed
case, 25 C ambient, high corner). bind(tag) reloads them for another project; the literals below are the
MAX77757 values and stand in when no calibration file exists.
"""
import json
import math
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import project  # noqa: E402

RHO, CP, G = 1.18, 1005.0, 9.81     # air at 25 to 30 C
CD = 0.6                            # orifice discharge coefficient
BLOCKAGE = 0.5                      # grille / mesh open-area fraction
VENTS_PER_SIDE = 2                  # two lower inlets and two upper outlets
VENT_OPENING_CM2 = 8.0              # geometric area per opening, before grille blockage
VENT_AREA_CM2 = VENTS_PER_SIDE * VENT_OPENING_CM2  # total area per inlet/outlet group
VENT_HEIGHT_M = 0.035               # vertical separation of inlet and outlet
R_WALL = 6.3                        # K/W, sealed case to ambient (run_heat.enclosure_resistance)
R_PACK = 9.2                        # K/W, pack to case air (run_heat.pack_resistance)
TREG = 130.0                        # C, MAX77757 thermal regulation (run_heat.CHG_TREG)
R_U4 = 37.0                         # K/W, U4 junction above case air per W of U4 loss, with the heatsink
THERMOSTAT_MIN, JEITA_HOT = 55.0, 42.1  # C, lowest thermostat opening point and charge stop

# Per scenario: p_in, heat inside the case without the U4 loss of a charging scenario [W]; u6, u7, cap,
# rise of U6, U7 and the hottest X7R capacitor over the case air [K]; p_pack, the pack's own I2R [W].
# Charging scenarios carry the U4 loss as p0 + k * charge current, a line through the heat model's
# results with the charge current capped at 0.6, 1.2 and 1.8 A (the last one in thermal regulation).
SCEN = {
    "bat_typ": dict(label="Battery, typical 1.22 W load", p_in=1.40, u6=4.4, u7=2.7, cap=3.2, p_pack=0.017, chg=None),
    "bat08":   dict(label="Battery 3.1 V, 0.8 A load",   p_in=4.65, u6=11.0, u7=7.8, cap=8.4, p_pack=0.173, chg=None),
    "bat10":   dict(label="Battery 3.1 V, 1.0 A load",   p_in=5.91, u6=14.5, u7=10.8, cap=11.2, p_pack=0.271, chg=None),
    "usb32":   dict(label="USB 3 A, cell 3.2 V, charging, 0.8 A load", p_in=5.11, u6=29.2, u7=25.1, cap=42.5,
                    p_pack=0.212, chg=dict(p0=0.38, k=0.87, imax=1.8)),
    "usb37":   dict(label="USB 3 A, cell 3.7 V, charging, 0.8 A load", p_in=5.21, u6=29.3, u7=25.5, cap=42.7,
                    p_pack=0.244, chg=dict(p0=0.15, k=0.91, imax=1.8)),
    "usb_off": dict(label="USB 3 A, cell 3.2 V, charging, device off", p_in=0.64, u6=0.0, u7=0.0, cap=19.6,
                    p_pack=0.292, chg=dict(p0=0.05, k=0.44, imax=1.8)),
}


CALIBRATION = None  # path of the calibration file in use, None for the literals above


def bind(tag=None):
    """Load SCEN, TREG and R_U4 of a charger's calibration file (default: the project's)."""
    global SCEN, TREG, R_U4, CALIBRATION
    tag = tag or project.charger(project.resolve())
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "heat", tag, "calibration.json")
    if not os.path.exists(path):
        return
    with open(path) as f:
        cal = json.load(f)
    SCEN = {k: dict(label=v["label"], p_in=v["p_in"], u6=v["u6"], u7=v["u7"], cap=v["cap"], p_pack=v["p_pack"],
                    r_pack=v.get("r_pack"), chg=dict(v["chg"]) if v["chg"] else None)
            for k, v in cal["scen"].items()}
    TREG = cal["treg"]
    global JEITA_HOT
    JEITA_HOT = cal.get("ts_hot", JEITA_HOT)
    R_U4 = cal["scen"]["usb32"]["chg"]["r_u4"] if cal["scen"].get("usb32", {}).get("chg") else R_U4
    CALIBRATION = path


bind()


def vent_power(dT, area_cm2, height_m, t_amb):
    """Heat carried out by the chimney draft for a case-air rise dT [K].
    area_cm2: geometric vent area per side (inlet = outlet), reduced by the grille blockage;
    two equal orifices in series give an effective area of A / sqrt(2)."""
    if dT <= 0 or area_cm2 <= 0:
        return 0.0
    a = area_cm2 * 1e-4 * BLOCKAGE / math.sqrt(2)
    m_dot = CD * a * RHO * math.sqrt(2 * G * height_m * dT / (t_amb + 273.15))
    # the exhaust leaves at the case-air temperature; the mean driving height is the vent separation
    return m_dot * CP * dT


def air_rise(power, t_amb, area_cm2=VENT_AREA_CM2, height_m=VENT_HEIGHT_M, fan_m3h=0.0,
             wall_resistance=R_WALL):
    """Steady-state case-air rise with parallel wall, vent and fan heat paths."""
    fan_mcp = fan_m3h / 3600 * RHO * CP
    lo, hi = 0.0, power * wall_resistance
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        cooling = mid / wall_resistance + vent_power(mid, area_cm2, height_m, t_amb) + fan_mcp * mid
        if cooling < power:
            lo = mid
        else:
            hi = mid
    return 0.5 * (lo + hi)


def solve(scen, t_amb, area_cm2=VENT_AREA_CM2, height_m=VENT_HEIGHT_M, fan_m3h=0.0):
    """Returns case air, pack, U4 Tj, charge current, U6/U7/cap temperatures."""
    s = SCEN[scen]
    chg = s["chg"]
    i_chg = chg["imax"] if chg else 0.0

    r_u4 = chg.get("r_u4", R_U4) if chg else R_U4

    def u4_loss():
        return chg["p0"] + chg["k"] * i_chg if chg else 0.0

    def p_pack():
        # the pack's own I²R follows the regulated current when the calibration gives the pack resistance
        return i_chg ** 2 * s["r_pack"] if chg and s.get("r_pack") else s["p_pack"]

    dT = 10.0
    for _ in range(200):
        p_in = s["p_in"] + u4_loss() + (p_pack() - s["p_pack"])
        previous, dT = dT, air_rise(p_in, t_amb, area_cm2, height_m, fan_m3h)
        if chg:
            # thermal regulation: the charge current settles where Tj = TREG, between zero and the set current
            treg = chg.get("treg", TREG)
            i_treg = (treg - t_amb - dT - r_u4 * chg["p0"]) / (r_u4 * chg["k"])
            if i_treg < i_chg or i_chg < chg["imax"]:
                i_chg = min(chg["imax"], max(0.0, i_treg))
        if abs(dT - previous) < 0.01:
            break
    t_air = t_amb + dT
    return dict(t_air=t_air, t_pack=t_air + p_pack() * R_PACK, tj_u4=t_air + r_u4 * u4_loss() if chg else None,
                i_chg=i_chg, tj_u6=t_air + s["u6"], tj_u7=t_air + s["u7"], t_cap=t_air + s["cap"])


def fmt(v):
    return "-" if v is None else "%.0f" % v


if __name__ == "__main__":
    areas = [0, 2, 5, 10, 20, 40]
    print("Vent sweep, geometric vent area per side (cm2), grille 50 % open, inlet low / outlet high.")
    print("Values: case air / pack  [C]; charging rows add U4 Tj and the regulated charge current.\n")
    for h in (0.035, 0.070):
        print("=== vent separation H = %.0f mm ===" % (h * 1e3))
        hdr = "%-46s %5s |" % ("scenario", "Tamb") + "".join(" %12s" % ("%d cm2" % a) for a in areas)
        print(hdr)
        for scen, t_amb in [("bat_typ", 25), ("bat08", 25), ("bat10", 25), ("bat08", 50), ("bat10", 50),
                            ("usb32", 25), ("usb37", 25), ("usb32", 35), ("usb_off", 25), ("usb_off", 35)]:
            cells = []
            for a in areas:
                r = solve(scen, t_amb, a, h)
                if SCEN[scen]["chg"]:
                    cells.append("%3.0f/%3.0f %3.0f %.1fA" % (r["t_air"], r["t_pack"], r["tj_u4"], r["i_chg"]))
                else:
                    cells.append("%3.0f/%3.0f" % (r["t_air"], r["t_pack"]))
            print("%-46s %5.0f |" % (SCEN[scen]["label"], t_amb) + "".join(" %12s" % c for c in cells))
        print()
    print("=== component temperatures with %.0f cm2 per side, H %.0f mm ===" % (VENT_AREA_CM2, VENT_HEIGHT_M * 1000))
    print("%-46s %5s %7s %7s %7s %7s %7s %7s" % ("scenario", "Tamb", "air", "pack", "U4", "U6", "U7", "capX7R"))
    for scen, t_amb in [("bat08", 25), ("bat10", 25), ("bat08", 50), ("bat10", 50), ("usb32", 25), ("usb32", 35),
                        ("usb37", 25), ("usb_off", 25), ("usb_off", 35)]:
        r = solve(scen, t_amb)
        print("%-46s %5.0f %7.0f %7.0f %7s %7.0f %7.0f %7.0f" % (SCEN[scen]["label"], t_amb, r["t_air"], r["t_pack"],
                                                              fmt(r["tj_u4"]), r["tj_u6"], r["tj_u7"], r["t_cap"]))
    print()
    print("=== fan: delivered airflow, no stack effect credited ===")
    print("%-46s %5s %8s %8s %8s %8s" % ("scenario", "Tamb", "2 CFM", "4 CFM", "U4@2CFM", "Ichg@2CFM"))
    for scen, t_amb in [("bat08", 25), ("bat10", 50), ("usb32", 25), ("usb32", 35), ("usb37", 25), ("usb_off", 35)]:
        r2 = solve(scen, t_amb, 0, 0.035, fan_m3h=3.4)
        r4 = solve(scen, t_amb, 0, 0.035, fan_m3h=6.8)
        print("%-46s %5.0f %4.0f/%3.0f %4.0f/%3.0f %8s %8.2f" % (SCEN[scen]["label"], t_amb, r2["t_air"], r2["t_pack"],
                                                             r4["t_air"], r4["t_pack"], fmt(r2["tj_u4"]), r2["i_chg"]))
    print()
    print("=== sealed reference (0 cm2): ===")
    for scen, t_amb in [("bat08", 25), ("bat10", 25), ("usb32", 25)]:
        r = solve(scen, t_amb, 0, 0.035)
        print("%-46s %5.0f air %.0f pack %.0f U4 %s Ichg %.2f" % (SCEN[scen]["label"], t_amb, r["t_air"], r["t_pack"],
                                                                fmt(r["tj_u4"]), r["i_chg"]))
    print()
    # minimum vent area to meet the pack criteria
    print("=== minimum geometric vent area per side [cm2] (50 % grille) to meet a criterion ===")
    for h in (0.035, 0.070):
        for label, scen, t_amb, key, limit in [
            ("bat 0.8 A @25: pack < 45 (JEITA margin while charging later)", "bat08", 25, "t_pack", 45),
            ("bat 1.0 A @25: pack < 45", "bat10", 25, "t_pack", 45),
            ("bat 1.0 A @50: pack < 55 (thermostat)", "bat10", 50, "t_pack", THERMOSTAT_MIN),
            ("charging+0.8 A @25 (cell 3.2): pack < 42 (JEITA hot)", "usb32", 25, "t_pack", JEITA_HOT),
            ("charging+0.8 A @25 (cell 3.2): full 1.8 A (U4 < 130)", "usb32", 25, "i_chg", SCEN["usb32"]["chg"]["imax"]),
            ("charging+0.8 A @35 (cell 3.2): pack < 42", "usb32", 35, "t_pack", JEITA_HOT),
            ("charging off @35: pack < 42", "usb_off", 35, "t_pack", JEITA_HOT),
        ]:
            def met(area):
                value = solve(scen, t_amb, area, h)[key]
                return value >= limit - 1e-6 if key == "i_chg" else value < limit

            found = next((a10 / 10 for a10 in range(2001) if met(a10 / 10)), None)
            print("H %.0f mm  %-62s %s" % (h * 1e3, label, "%.1f cm2" % found if found is not None else "> 200 cm2 (not reachable)"))
