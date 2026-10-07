#!/usr/bin/env python3
"""Transient charging estimate with pack thermal inertia and JEITA hysteresis.

python hardware/sim/run_charge_heat.py --write
Calibrates a five-node thermal network from the current board's steady-state model.
Heat capacities and contact resistances are estimates, not measured timing data.
"""
import argparse
from dataclasses import dataclass
import hashlib
import json
import math
from pathlib import Path

import numpy as np

import run_heat as heat
import vent_model

OUT = Path(heat.OUT)
HOT, RESTART = 42.1, 40.8  # nominal pack NTC transitions, thermal/results.md
PACKS = {
    "5000": {"label": "5,000 mAh / FE105573", "mm": (73, 55, 10.1), "capacity": 80.0},
    "10000": {"label": "10,000 mAh / FE1160100", "mm": (100, 60, 11.1), "capacity": 160.0},
}
COLORS = ("#ba4b32", "#2477a6", "#25846c", "#8058a5")
POLICIES = ("1.8 A + pack JEITA", "1.2 A + pack JEITA", "90-100 °C target + JEITA")


@dataclass
class Calibration:
    currents: np.ndarray
    power: np.ndarray  # U4, local PCB, other board + modules
    r_board_air: float
    r_top: float
    r_sink_air: float
    r_pack_dc: float
    reference: list
    pcb_sha256: str = ""

    def losses(self, current):
        return np.array([np.interp(current, self.currents, self.power[:, k]) for k in range(3)])


def calibrate():
    board_hash = hashlib.sha256(Path(heat.PCB).read_bytes()).hexdigest()
    board = heat.read_board()
    copper = heat.Copper(board)
    params = json.loads(Path(heat.PARAMS).read_text())
    k, p0, _ = heat.fit_boost(params["boost_efficiency_curve"])
    model = heat.Model(board, copper, (k, p0), params, 1, heat.HEATSINK)
    model.r_net, model.r_gnd_bat, model.r_gnd_usb = heat.solve_paths(model, board)
    env = heat.ENVIRONMENTS["closed"]
    system = model.thermal_sheet(env).factorize()
    source = np.zeros(heat.NL * model.grid.nc + len(model.nodes))
    source[model.nodes["U4"]] = 1.0
    r_u4_air = system.solve(source)[model.nodes["U4"]]
    hs = heat.HEATSINK["U4"]
    r_top = hs["r_jt"] + hs["r_tim"]
    r_sink = heat.heatsink_resistance(hs, env) - r_top
    r_parallel = r_top + r_sink
    r_board = 1 / (1 / r_u4_air - 1 / r_parallel) - heat.CHG_THETA_JC
    gain_board = r_u4_air * r_board / (r_board + heat.CHG_THETA_JC)
    currents, powers, reference = [], [], []
    case = next(c for c in heat.CASES if c["key"] == "usb37")
    for current in np.linspace(0, heat.CHARGE_A, 7):
        result = heat.run_case(model, case, "closed", 25, k * heat.BUCK_SW_HIGH,
                               charge_current=current, regulation_temperature=math.inf)
        p_u4 = heat.u4_loss(result)
        p_other = result["board_loss"] + result["modules"] - p_u4
        p_local = (result["tj"]["U4"] - result["t_air"] - r_u4_air * p_u4) / gain_board
        if not 0 <= p_local <= p_other or result["i_chg"] < -1e-6:
            raise RuntimeError("Cannot reduce this operating point to a passive charging network")
        currents.append(result["i_chg"])
        powers.append((p_u4, p_local, p_other - p_local))
        reference.append({"current": result["i_chg"], "u4": result["tj"]["U4"],
                          "air": result["t_air"], "pack": result["t_pack"]})
        print("Calibrated %.2f A: U4 %.1f °C, enclosure %.1f °C" %
              (result["i_chg"], result["tj"]["U4"], result["t_air"]), flush=True)
    if hashlib.sha256(Path(heat.PCB).read_bytes()).hexdigest() != board_hash:
        raise RuntimeError("Board changed during calibration; rerun")
    return Calibration(np.array(currents), np.array(powers), r_board, r_top, r_sink,
                       heat.PACK_R[1], reference, board_hash)


@dataclass
class Network:
    # Nodes: U4 junction, local PCB, front sink, enclosure surroundings, battery.
    c: np.ndarray
    g: np.ndarray
    ambient_g: np.ndarray

    def stepper(self, dt):
        matrix = np.diag(self.c / dt) + self.g
        inverse = np.linalg.inv(matrix)
        return lambda temperature, power, ambient: inverse @ (
            self.c / dt * temperature + power + self.ambient_g * ambient)


def network(cal, pack, capacity_scale=1.0, pack_r_scale=1.0, spreader_r=math.inf,
            direct_pack_r=math.inf):
    c = np.array([0.02, 3.0, 1.5, 100.0, pack["capacity"]]) * capacity_scale
    g, ambient_g = np.zeros((5, 5)), np.zeros(5)

    def link(a, b, resistance):
        value = 1 / resistance
        g[a, a] += value
        g[b, b] += value
        g[a, b] -= value
        g[b, a] -= value

    a, b, d = np.array(pack["mm"]) / 1000
    r_pack = pack_r_scale / (heat.H_IN * 2 * (a * b + a * d + b * d))
    link(0, 1, heat.CHG_THETA_JC)
    link(0, 2, cal.r_top)
    link(2, 3, cal.r_sink_air)
    link(1, 3, cal.r_board_air)
    link(3, 4, r_pack)
    link(1, 4, direct_pack_r)
    ambient_g[3] = 1 / heat.enclosure_resistance()[0]
    ambient_g[1] = 1 / spreader_r
    g += np.diag(ambient_g)
    return Network(c, g, ambient_g)


def jeita_hot(hot, pack_temperature):
    return pack_temperature > RESTART if hot else pack_temperature >= HOT


def current_target(tj, pack_hot, policy):
    if pack_hot:
        return 0.0
    imax = 1.2 if policy == "low" else 1.8
    target = imax * max(0.0, min(1.0, (150 - tj) / 20))
    if policy == "early":
        target = min(target, imax * max(0.0, min(1.0, (100 - tj) / 10)))
    return target


def steady_error(cal):
    net = network(cal, PACKS["10000"])
    error = 0.0
    for point in cal.reference:
        current, air = point["current"], point["air"]
        pu, pb, other = cal.losses(current)
        power = np.array([pu, pb, 0.0, other, current ** 2 * cal.r_pack_dc])
        power[3] -= vent_model.vent_power(air - 25, vent_model.VENT_AREA_CM2,
                                          vent_model.VENT_HEIGHT_M, 25)
        temperatures = np.linalg.solve(net.g, power + net.ambient_g * 25)
        error = max(error, abs(temperatures[0] - point["u4"]), abs(temperatures[3] - air))
    if error > .1:
        raise RuntimeError("Reduced model differs from board model by %.3f K" % error)
    return error


def simulate(cal, pack, ambient=25.0, policy="stock", minutes=90, dt=1.0, **kwargs):
    net = network(cal, pack, **kwargs)
    step = net.stepper(dt)
    temperature = np.full(5, ambient, dtype=float)
    rows, hot, current, charge = [], False, 0.0, 0.0
    energy_error = 0.0
    for index in range(round(minutes * 60 / dt) + 1):
        hot = jeita_hot(hot, temperature[4])
        # Stop at the thermostat's earliest possible trip; its reset cycle is not modelled.
        if temperature[4] >= heat.THERMOSTAT_MIN:
            break
        target = current_target(temperature[0], hot, policy)
        current = 0.0 if hot else current + min(1.0, dt / 5) * (target - current)
        rows.append([index * dt / 60, *temperature, current, charge])
        if index == round(minutes * 60 / dt):
            break
        pu, pb, other = cal.losses(current)
        power = np.array([pu, pb, 0.0, other, current ** 2 * cal.r_pack_dc])
        vents = vent_model.vent_power(temperature[3] - ambient, vent_model.VENT_AREA_CM2,
                                      vent_model.VENT_HEIGHT_M, ambient)
        power[3] -= vents
        updated = step(temperature, power, ambient)
        stored = np.dot(net.c, updated - temperature)
        supplied = dt * (sum(power) - np.dot(net.ambient_g, updated - ambient))
        energy_error = max(energy_error, abs(stored - supplied))
        temperature = updated
        charge += current * dt / 3600
    return np.array(rows), energy_error


def save_graphs(cal, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages

    plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False,
                         "axes.grid": True, "grid.alpha": 0.18, "svg.fonttype": "none",
                         "pdf.fonttype": 42, "axes.titleweight": "bold"})
    summaries, figures = [], []

    def page(title, subtitle):
        fig, axes = plt.subplots(3, 2, figsize=(11.7, 8.3), sharex=True)
        fig.subplots_adjust(top=.82, bottom=.18, left=.075, right=.975, hspace=.22, wspace=.23)
        fig.suptitle(title, x=.075, ha="left", fontsize=19, weight="bold", y=.98)
        fig.text(.075, .922, subtitle, fontsize=10)
        for row, label, limits in ((0, "U4 junction (°C)", (20, 150)),
                                   (1, "Battery (°C)", (20, 56)),
                                   (2, "Charge current (A)", (-.05, 1.95))):
            for ax in axes[row]:
                ax.set_ylabel(label)
                ax.set_ylim(*limits)
                ax.set_xlim(0, 90)
                ax.set_xticks(range(0, 91, 15))
        for ax in axes[0]:
            ax.axhspan(90, 100, color="#d8e9df", alpha=.6)
            ax.axhline(130, color="#777777", ls=":", lw=1)
        for ax in axes[1]:
            ax.axhline(HOT, color="#777777", ls=":", lw=1)
        for ax in axes[2]:
            ax.set_xlabel("Time after cold start (minutes)")
        return fig, axes

    def curves(axes, column, data, color, label, style="-"):
        for row, value in ((0, 1), (1, 5), (2, 6)):
            axes[row, column].plot(data[:, 0], data[:, value], color=color, label=label, lw=1.6, ls=style)

    def finish(fig, stem, labels, footer):
        handles = fig.axes[0].get_lines()
        handles = [line for line in handles if line.get_label() in labels]
        fig.legend(handles, labels, loc="upper left", bbox_to_anchor=(.07, .904), ncol=len(labels),
                   frameon=False, fontsize=9)
        fig.text(.075, .055, footer, fontsize=9, linespacing=1.5)
        figures.append((stem, fig))

    for key, pack in PACKS.items():
        fig, axes = page(pack["label"] + " | delayed battery heating",
                         "5 V / 3 A USB, fixed 3.7 V cell, 0.8 A system load, front heatsink and 5 cm² inlet + outlet")
        for column, ambient in enumerate((25, 35)):
            axes[0, column].set_title("Ambient %d °C" % ambient)
            for policy, color, label in zip(("stock", "low", "early"), COLORS, POLICIES):
                data, error = simulate(cal, pack, ambient, policy)
                curves(axes, column, data, color, label)
                off = np.flatnonzero(data[:, 5] >= HOT)
                summaries.append({"pack": key, "ambient": ambient, "policy": policy,
                                  "peak_u4": float(max(data[:, 1])), "peak_pack": float(max(data[:, 5])),
                                  "first_stop_min": float(data[off[0], 0]) if len(off) else None,
                                  "charge_ah": float(data[-1, 7]), "end_min": float(data[-1, 0]),
                                  "energy_error_j": error})
        finish(fig, "charging_" + key, POLICIES,
               "Dotted: U4 native foldback at 130 °C; battery JEITA stop at 42.1 °C, restart at 40.8 °C.\n"
               "Green band: proposed junction target, not an existing circuit. Curves end at 55 °C pack; no thermostat reset model.")

    fig, axes = page("How uncertain is the heating delay?",
                     "25 °C ambient, 1.8 A setting + JEITA. Separate sensitivity scenarios, not a confidence interval.")
    labels = ("Base estimate", "Half all heat capacities", "Double all heat capacities", "Extra PCB-to-pack path")
    for column, pack in enumerate(PACKS.values()):
        axes[0, column].set_title(pack["label"])
        for options, color, label, style in zip(({}, {"capacity_scale": .5}, {"capacity_scale": 2},
                                                 {"direct_pack_r": 100.0}), COLORS, labels, ("-", "--", "--", ":")):
            data, _ = simulate(cal, pack, **options)
            curves(axes, column, data, color, label, style)
    finish(fig, "charging_delay", labels,
           "Base battery heat capacities: 80 J/K (5 Ah), 160 J/K (10 Ah). Enclosure surroundings: 100 J/K. All unmeasured.\n"
           "Extra PCB-to-pack path: assumed 100 K/W across the gap/insulator, in addition to the shared enclosure surroundings.")

    fig, axes = page("What an external heat path could change",
                     "25 °C ambient. Added local PCB-to-outside resistance includes contact, spreader and outside cooling.")
    labels = ("Front sink only", "Plus 20 K/W external path", "Plus 10 K/W external path")
    for column, pack in enumerate(PACKS.values()):
        axes[0, column].set_title(pack["label"])
        for resistance, color, label in zip((math.inf, 20, 10), COLORS, labels):
            data, _ = simulate(cal, pack, spreader_r=resistance)
            curves(axes, column, data, color, label)
    finish(fig, "charging_spreader", labels,
           "Illustrative resistances, not ratings or predictions for a selected part. A spreader wholly inside the case is not this path.\n"
           "All pages: frozen 3.7 V operating point; no SOC/CV taper. Curves end if the battery reaches the earliest thermostat trip, 55 °C.")
    with PdfPages(out / "charging.pdf", metadata={"Title": "Charger and battery thermal transients"}) as pdf:
        for stem, fig in figures:
            fig.savefig(out / (stem + ".svg"))
            pdf.savefig(fig)
            plt.close(fig)
    return summaries


def report(cal, summaries, error):
    lines = ["# Charging with battery thermal inertia", "",
             "Generated by `hardware/sim/run_charge_heat.py --write`. [Four-page PDF](charging.pdf). "
             "SVGs: [5 Ah](charging_5000.svg), [10 Ah](charging_10000.svg), "
             "[delay sensitivity](charging_delay.svg), [external spreader](charging_spreader.svg).", "",
             "## Model", "",
             "Five energy-conserving nodes: U4 junction, local PCB, front heatsink, enclosure surroundings, pack. "
             "Thermal resistances and interpolated losses come from seven current points on the current board's "
             "high-corner steady-state model. The enclosure node includes air and the thermally participating "
             "case, main board and modules; it is not air alone. The pack has its own heat capacity and I²R heating. "
             "It heats through finite thermal resistance to the enclosure surroundings.", "",
             "| Input | Value | Basis |", "|---|---|---|",
             "| USB / cell / system load | 5 V, 3 A / fixed 3.7 V / 0.8 A on 5 V rail | Charging while running |",
             "| Ambient / duration | 25 and 35 °C / up to 90 min from uniform ambient | No preheated start |",
             "| Pack dimensions | 73 x 55 x 10.1 mm / 100 x 60 x 11.1 mm | FE105573 / FE1160100 datasheets |",
             "| Pack heat capacity | 80 / 160 J/K | Assumed, no reliable pack mass or specific heat |",
             "| U4 / local PCB / sink / surroundings heat capacity | 0.02 / 3 / 1.5 / 100 J/K | Assumed |",
             "| Pack DC resistance | 90 mΩ for both | Existing high-corner estimate; both specify <=50 mΩ AC |",
             "| Pack surface heat transfer | 7 W/m²K times all six faces | Existing estimate; mounting can obstruct it |",
             "| U4 to local PCB | 7.5 K/W | Existing model's interpretation of vendor θJC |",
             "| Local PCB to surroundings | %.2f K/W | Reduced from live board thermal network |" % cal.r_board_air,
             "| U4 to front sink / sink to surroundings | %.1f / %.1f K/W | Existing top/spacer/epoxy estimates |" %
             (cal.r_top, cal.r_sink_air),
             "| Vents | 5 cm² each, 50% free area, 35 mm height | Existing enclosure model |",
             "| JEITA hot / restart | 42.1 / 40.8 °C | Nominal pack NTC thresholds |",
             "| Native die foldback | 130 °C, 5% of set current per °C | MAX77757 datasheet |",
             "| Proposed early limiter | Full below 90 °C, linear to zero at 100 °C | Ideal junction feedback, not fitted hardware |",
             "| Current response / thermal timestep | Assumed 5 s / 1 s implicit Euler | Numerical estimate, not vendor timing |",
             "| Earliest pack thermostat trip | 55 °C | Curves stop here; reset behaviour not modelled |", "",
             "The 31.4 °C JEITA warm state lowers termination to 4.05 V, not charge current. "
             "At this fixed 3.7 V cell operating point the model remains below that voltage. "
             "No state-of-charge curve, constant-voltage taper, ageing or charge-completion prediction. "
             "The pack NTC is assumed to track pack temperature exactly; real attachment adds delay. "
             "The pack is one average temperature, not a core/surface model. "
             "Losses are frozen at the steady calibration points, so cold-start losses can be overestimated. "
             "The 35 °C runs use the same loss table. These are scenarios, not guaranteed bounds.", "",
             "The delay page halves/doubles all heat capacities, and separately adds a 100 K/W PCB-to-pack "
             "path for gap/radiation/insulator coupling. These are sensitivity assumptions, not confidence intervals. "
             "The spreader page adds a 20 or 10 K/W local PCB-to-outside path. This includes interface, lateral "
             "spreading and external heat rejection. No selected product is claimed to achieve either value.", "",
             "## Results", "",
             "| Pack | Ambient | Policy | Peak U4 | Peak pack | First charge stop | Charge added | End |",
             "|---|---|---|---|---|---|---|---|"]
    for item in summaries:
        stop = "%.1f min" % item["first_stop_min"] if item["first_stop_min"] is not None else "none"
        lines.append("| %s mAh | %.0f °C | %s | %.1f °C | %.1f °C | %s | %.2f Ah | %.1f min |" %
                     (item["pack"], item["ambient"], item["policy"], item["peak_u4"], item["peak_pack"],
                      stop, item["charge_ah"], item["end_min"]))
    lines += ["", "Charging stops before the old sustained-charge steady temperatures occur. "
              "At this load, module heat keeps warming the pack after JEITA suspends charging. "
              "The larger pack delays this; it does not remove the steady enclosure heat.", "",
              "## Verification", "",
              "Reduced steady U4/enclosure temperatures match the seven board-model calibration points "
              "within %.3g K. Maximum per-step energy residual across the main scenarios: %.3g J. "
              "Analytic exponential, energy conservation, JEITA hysteresis and timestep tests are in "
              "`test_charge_heat.py`. These establish numerical consistency, not physical accuracy." %
              (error, max(s["energy_error_j"] for s in summaries)), "",
              "Board SHA256: `%s`. Input hashes and calibration points are in [charging.json](charging.json)." %
              cal.pcb_sha256, ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--write", action="store_true", help="write heat/charging.pdf, SVGs and model report")
    args = parser.parse_args()
    cal = calibrate()
    error = steady_error(cal)
    if not args.write:
        for pack in PACKS.values():
            data, error = simulate(cal, pack)
            print("%s: U4 peak %.1f °C, pack peak %.1f °C, %.2f Ah, energy residual %.2g J" %
                  (pack["label"], max(data[:, 1]), max(data[:, 5]), data[-1, 7], error))
        return
    OUT.mkdir(exist_ok=True)
    summaries = save_graphs(cal, OUT)
    payload = {"pcb_sha256": cal.pcb_sha256, "currents": cal.currents.tolist(), "power_w": cal.power.tolist(),
               "r_board_air": cal.r_board_air, "r_top": cal.r_top, "r_sink_air": cal.r_sink_air,
               "r_pack_dc": cal.r_pack_dc, "packs": PACKS,
               "input_sha256": {str(Path(path).resolve().relative_to(Path(heat.HERE).parents[1])):
                                hashlib.sha256(Path(path).read_bytes()).hexdigest()
                                for path in (heat.PARAMS, heat.__file__, __file__)},
               "reference": cal.reference, "summary": summaries}
    (OUT / "charging.json").write_text(json.dumps(payload, indent=2) + "\n")
    (OUT / "charging.md").write_text(report(cal, summaries, error))
    for item in summaries:
        print(item)


if __name__ == "__main__":
    main()
