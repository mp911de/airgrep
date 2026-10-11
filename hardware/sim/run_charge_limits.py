#!/usr/bin/env python3
"""TMP300 charge cut-off sweeps; keeps heat/charging.* and the original SVGs unchanged.

python hardware/sim/run_charge_limits.py --write
Full 1.8 A while cool, zero while tripped, independent battery JEITA protection.
"""
import argparse
from dataclasses import asdict
import hashlib
import json
from pathlib import Path

import numpy as np

import run_charge_heat as charge

project = charge.project

TRIPS = (80, 90, 100, 110)
COLORS = ("#236a9f", "#18816d", "#ab7522", "#af4779")
DT = .25
TI_DATASHEET = "https://www.ti.com/lit/ds/symlink/tmp300.pdf"


def summary(data, error):
    pack_trip = np.flatnonzero(data[:, 5] >= charge.HOT)
    hot = data[:, 9].astype(bool) if data.shape[1] > 8 else np.zeros(len(data), dtype=bool)
    stops = np.flatnonzero(np.diff(hot.astype(int), prepend=0) == 1)
    # Charge-disable cycles after the battery has stopped charging are not charge interruptions.
    stops = stops[stops < (pack_trip[0] if len(pack_trip) else len(data))]
    periods = np.diff(data[stops, 0]) * 60
    return {"peak_u4": float(max(data[:, 1])), "peak_pack": float(max(data[:, 5])),
            "pack_stop_min": float(data[pack_trip[0], 0]) if len(pack_trip) else None,
            "charge_ah": float(data[-1, 7]), "end_min": float(data[-1, 0]),
            "charger_stops": len(stops), "cycle_median_s": float(np.median(periods)) if len(periods) else None,
            "cycle_min_s": float(min(periods)) if len(periods) else None,
            "cycle_max_s": float(max(periods)) if len(periods) else None,
            "energy_error_j": float(error)}


def average_current(data, seconds=60):
    """Time-weighted non-overlapping bins; the last row has no following interval."""
    times, currents = [], []
    bucket = (data[:-1, 0] * 60 // seconds).astype(int)
    durations = np.diff(data[:, 0]) * 60
    for index in np.unique(bucket):
        keep = bucket == index
        times.append(float(np.average(data[:-1, 0][keep], weights=durations[keep])))
        currents.append(float(np.average(data[:-1, 6][keep], weights=durations[keep])))
    return times, currents


def run(cal):
    results = {}
    for pack_key, pack in charge.PACKS.items():
        for ambient in (25, 35):
            for hysteresis in (5, 10):
                for trip in (None,) + TRIPS:
                    limit = charge.TemperatureSwitch(trip, hysteresis) if trip else None
                    data, error = charge.simulate(cal, pack, ambient, limit=limit, dt=DT)
                    results[(pack_key, ambient, hysteresis, trip)] = (data, summary(data, error))
    return results


def graphs(cal, results, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages

    plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False,
                         "axes.grid": True, "grid.alpha": .18, "svg.fonttype": "none", "pdf.fonttype": 42})
    figures, sensitivities = [], []

    def page(title, subtitle):
        fig, axes = plt.subplots(3, 2, figsize=(11.7, 8.3), sharex=True)
        fig.subplots_adjust(top=.82, bottom=.18, left=.075, right=.975, hspace=.22, wspace=.23)
        fig.suptitle(title, x=.075, ha="left", fontsize=18, weight="bold", y=.98)
        fig.text(.075, .922, subtitle, fontsize=10)
        for row, label, limits in ((0, "U4 junction (°C)", (20, 145)),
                                   (1, "Battery (°C)", (20, 56)),
                                   (2, "Charge current (A)\n60-second mean", (-.05, 1.95))):
            for ax in axes[row]:
                ax.set_ylabel(label)
                ax.set_ylim(*limits)
                ax.set_xlim(0, 90)
                ax.set_xticks(range(0, 91, 15))
        for ax in axes[0]:
            ax.axhline(100, color="#888888", ls=":", lw=1)
        for ax in axes[1]:
            ax.axhline(charge.HOT, color="#888888", ls=":", lw=1)
        for ax in axes[2]:
            ax.set_xlabel("Time after cold start (minutes)")
        return fig, axes

    def curves(axes, column, data, color, label, style="-"):
        for row, value in ((0, 1), (1, 5)):
            axes[row, column].plot(data[:, 0], data[:, value], color=color, label=label, lw=1.1, ls=style)
        times, current = average_current(data)
        axes[2, column].plot(times, current, color=color, label=label, lw=1.4, ls=style)

    def finish(fig, stem, labels, footer):
        handles = [line for line in fig.axes[0].get_lines() if line.get_label() in labels]
        fig.legend(handles, labels, loc="upper left", bbox_to_anchor=(.07, .904),
                   ncol=len(labels), frameon=False, fontsize=9)
        fig.text(.075, .055, footer, fontsize=9, linespacing=1.5)
        figures.append((stem, fig))

    for pack_key, pack in charge.PACKS.items():
        for ambient in (25, 35):
            fig, axes = page(pack["label"] + " | fast charging with hysteresis",
                             "%d °C ambient; 1.8 A when enabled; 0.8 A system load; front sink; existing vents; fixed 3.7 V cell" % ambient)
            for column, hysteresis in enumerate((5, 10)):
                axes[0, column].set_title("%d °C hysteresis" % hysteresis, weight="bold")
                baseline = results[(pack_key, ambient, hysteresis, None)][0]
                curves(axes, column, baseline, "#333333", "No extra cutoff", "--")
                for trip, color in zip(TRIPS, COLORS):
                    curves(axes, column, results[(pack_key, ambient, hysteresis, trip)][0],
                           color, "%d °C sensor trip" % trip)
            stem = "charging_limits_" + pack_key + ("_warm" if ambient == 35 else "")
            finish(fig, stem, ["No extra cutoff"] + ["%d °C sensor trip" % t for t in TRIPS],
                   "TMP300 senses local PCB, with assumed 2 s lag. Its setting is NOT a U4 junction ceiling. Restart at trip minus hysteresis.\n"
                   "Dotted: 100 °C junction / 42.1 °C battery. Current is averaged. Curves end at 55 °C pack; thermostat reset is not modelled.")

    fig, axes = page("How uncertain is the heating delay with a cutoff?",
                     "25 °C ambient; 90 °C local PCB sensor trip; 10 °C hysteresis; full 1.8 A when enabled")
    labels = ("Base estimate", "Half thermal mass", "Double thermal mass", "Extra PCB-to-pack path")
    for column, (pack_key, pack) in enumerate(charge.PACKS.items()):
        axes[0, column].set_title(pack["label"], weight="bold")
        for options, color, label, style in zip(({}, {"capacity_scale": .5}, {"capacity_scale": 2},
                                                {"direct_pack_r": 100.0}), COLORS, labels, ("-", "--", "--", ":")):
            limit = charge.TemperatureSwitch(90)
            data, error = charge.simulate(cal, pack, limit=limit, dt=DT, **options)
            curves(axes, column, data, color, label, style)
            sensitivities.append({"pack": pack_key, "scenario": label, "limit": asdict(limit),
                                  "network": options, **summary(data, error)})
    finish(fig, "charging_limits_delay", labels,
           "Thermal mass is unmeasured: base pack 80 / 160 J/K; surroundings 100 J/K. Sensor lag remains 2 s in all cases.\n"
           "The extra path is an assumed 100 K/W across the gap/insulator. Sensitivity scenarios, not probability or lifetime estimates.")

    fig, axes = page("Sensor placement, lag and TMP300B tolerance",
                     "25 °C ambient; nominal 90 °C trip, 10 °C hysteresis. RSET assumed 0.1%; B-grade trip error up to ±6 °C.")
    labels = ("Ideal junction sensing", "PCB sensor, 2 s lag", "Early trip, zero lag", "Late trip, 10 s lag")
    variants = (charge.TemperatureSwitch(90, sensor_node=0, sensor_tau=0), charge.TemperatureSwitch(90),
                charge.TemperatureSwitch(90, sensor_tau=0, trip_error=-6.14),
                charge.TemperatureSwitch(90, sensor_tau=10, trip_error=6.14))
    for column, (pack_key, pack) in enumerate(charge.PACKS.items()):
        axes[0, column].set_title(pack["label"], weight="bold")
        for limit, color, label, style in zip(variants, COLORS, labels, ("--", "-", ":", ":")):
            data, error = charge.simulate(cal, pack, limit=limit, dt=DT)
            curves(axes, column, data, color, label, style)
            sensitivities.append({"pack": pack_key, "scenario": label, "limit": asdict(limit),
                                  **summary(data, error)})
    finish(fig, "charging_limits_sensor", labels,
           "Ideal junction sensing is a reference, not a TMP300 capability. Local PCB temperature is below the junction during charging.\n"
           "±6.14 °C combines the sensor's ±6 °C specification and 0.1% RSET. Thermal lag values are assumptions, not datasheet limits.")

    fig, axes = page("What the hysteresis actually does",
                     "10 Ah pack, 25 °C ambient, 90 °C sensor trip. Full current resumes only after the sensor has cooled.")
    for column, hysteresis in enumerate((5, 10)):
        data, stats = results[("10000", 25, hysteresis, 90)]
        axes[0, column].set_title("Stop at 90 °C / restart at %d °C" % (90 - hysteresis), weight="bold")
        axes[0, column].plot(data[:, 0], data[:, 1], color=COLORS[0], label="U4 junction")
        axes[0, column].plot(data[:, 0], data[:, 8], color=COLORS[1], label="TMP300 sensor")
        axes[0, column].axhline(90, color="#777777", ls="--", lw=.8)
        axes[0, column].axhline(90 - hysteresis, color="#777777", ls="--", lw=.8)
        axes[0, column].set_ylim(65, 108)
        axes[1, column].plot(data[:, 0], data[:, 5], color=COLORS[2])
        axes[1, column].set_ylim(25, 45)
        axes[2, column].step(data[:, 0], data[:, 6], where="post", color=COLORS[0])
        axes[2, column].set_ylabel("Actual charge current (A)")
        stops = np.flatnonzero(np.diff(data[:, 9], prepend=0) == 1)
        start = max(0, data[stops[0], 0] - .5) if len(stops) else 0
        for ax in axes[:, column]:
            ax.set_xlim(start, start + 5)
            ax.set_xticks(np.linspace(start, start + 5, 6))
            ax.xaxis.set_major_formatter(matplotlib.ticker.FormatStrFormatter("%.1f"))
    finish(fig, "charging_limits_cycles", ["U4 junction", "TMP300 sensor"],
           "Five-minute windows around the first charger-temperature cutoff; all rows share the same time axis in each column.\n"
           "A wider band reduces switching frequency but does not eliminate thermal cycling. This model does not estimate battery wear.")

    with PdfPages(out / "charging_limits.pdf", metadata={"Title": "Fast charging with TMP300 hysteresis"}) as pdf:
        for stem, fig in figures:
            fig.savefig(out / (stem + ".svg"))
            pdf.savefig(fig)
            plt.close(fig)
    return sensitivities, [stem for stem, _ in figures]


def report(cal, results, sensitivities, stems, error):
    lines = ["# Fast charging with a TMP300 temperature switch", "",
             "Generated by `hardware/sim/run_charge_limits.py --write`. "
             "[Combined PDF](charging_limits.pdf). The original charging graphs are preserved.", "",
             "SVGs: " + ", ".join("[%s](%s.svg)" % (s, s) for s in stems) + ".", "",
             "## Control and assumptions", "",
             "Full 1.8 A is permitted while cool. A TMP300-like switch disables charging through THM at its "
             "trip point, then permits it again at trip minus 5 or 10 °C. The existing nominal pack JEITA "
             "stop/restart at 42.1/40.8 °C takes priority. Native U4 foldback at 130 °C remains active. "
             "USB keeps supplying the system when charging is disabled. This is a proposed control model, "
             "not a circuit or PCB change. Stop/resume operation of MAX77757 through THM still needs a bench check.", "",
             "The local PCB node is used as the TMP300 sensor location with an assumed 2 s first-order lag. "
             "It is not the U4 junction. Exact placement, thermal coupling and sensor delay are unmeasured. "
             "The sensor has negligible added heating/capacity in this reduced network. Trip and hysteresis "
             "comparisons are evaluated every %.2f s; turn-off is immediate and current rises with the "
             "existing assumed 5 s response when enabled. No separate minimum on/off time is imposed." % DT, "",
             "Other assumptions are unchanged from [charging.md](charging.md): fixed 3.7 V cell, USB 5 V / 3 A, "
             "0.8 A system load, existing front sink and 5 cm² inlet/outlet, estimated thermal masses. "
             "No SOC evolution, CV taper, charger timer, NTC attachment lag, or battery ageing model. "
             "Curves end at 55 °C pack, the earliest thermostat trip. All current averaging is for display only; "
             "losses and I²R use instantaneous current. A fresh baseline is calibrated from the current board, "
             "so old artifacts may reflect a different board revision.", "",
             "## TMP300B details", "",
             "[TI datasheet, Rev F](%s): resistor-programmable trip, 5/10 °C hysteresis, open-drain output. "
             "B-grade total trip accuracy is ±6 °C maximum, ±2 °C typical; RSET tolerance adds to this. "
             "For all four thresholds, assume a 3.3 V sensor supply. Below 2.35 V the upper measurement limit "
             "is 100 × (VS - 0.95) °C, only 85 °C at 1.8 V. VTEMP is high impedance (210 kΩ); reading it "
             "requires care not to shift the comparator threshold. JLCPCB stock was not verified." % TI_DATASHEET,
             "", "| Sensor trip | Ideal RSET | 0.1% resistor contribution |",
             "|---|---|---|"]
    lines += ["| %d °C | %.2f kΩ | ±%.2f °C |" % (t, 10 * (50 + t) / 3, .001 * (50 + t)) for t in TRIPS]
    lines += ["", "## Nominal 25 °C ambient results", "",
              "Trip values below refer to the sensor, not the charger junction. Cycle periods are between "
              "successive charger-temperature interruptions before the battery stops charging. Their "
              "median varies with heating, mass and sensor placement. Battery interruptions are not a "
              "battery wear metric.", "",
              "| Pack | Sensor trip | Hysteresis | Peak U4 | Pack stop | Charge added | Charger stops | Median cycle |",
              "|---|---|---|---|---|---|---|---|"]
    for (pack, ambient, hysteresis, trip), (_, s) in results.items():
        if ambient != 25 or (trip is None and hysteresis == 10):
            continue
        lines.append("| %s mAh | %s | %s | %.1f °C | %s | %.2f Ah | %d | %s |" %
                     (pack, "%d °C" % trip if trip else "no extra cutoff", "%d °C" % hysteresis if trip else "-",
                      s["peak_u4"], "%.1f min" % s["pack_stop_min"] if s["pack_stop_min"] else "none",
                      s["charge_ah"], s["charger_stops"],
                      "%.0f s" % s["cycle_median_s"] if s["cycle_median_s"] else "-"))
    lines += ["", "## Validation", "",
              "Reduced steady model agrees with its board calibration within %.3g K. Maximum main-run "
              "energy residual is %.3g J per step. Unit tests cover hysteresis memory, immediate stop, "
              "restart, pack precedence, thermal lag, energy conservation and timestep convergence. "
              "These are numerical checks, not physical validation." %
              (error, max(s["energy_error_j"] for _, s in results.values())), "",
              "Board SHA256: `%s`. Full inputs, 35 °C results and sensitivity results: "
              "[charging_limits.json](charging_limits.json)." % cal.pcb_sha256, ""]
    return "\n".join(lines)


# --- BQ25798: the charger has a programmable TREG and the host reads the die temperature, so the sweep is
# over the regulation set point and the host current, not over an external temperature switch.
TREGS = (80.0, 100.0, 120.0)


def run_treg(cal):
    results = {}
    for pack_key, pack in charge.PACKS.items():
        for ambient in (25, 35):
            for imax, mode in ((1.8, "host"), (1.0, "default")):
                for treg in TREGS:
                    policy = ("%s %.1f A, TREG %.0f" % (mode, imax, treg), imax, treg, None)
                    data, error = charge.simulate(cal, pack, ambient, policy=policy, dt=DT)
                    results[(pack_key, ambient, mode, treg)] = (data, summary(data, error))
    return results


def graphs_treg(cal, results, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages

    plt.rcParams.update({"font.size": 10, "axes.spines.top": False, "axes.spines.right": False,
                         "axes.grid": True, "grid.alpha": .18, "svg.fonttype": "none", "pdf.fonttype": 42})
    figures, sensitivities = [], []

    def page(title, subtitle):
        fig, axes = plt.subplots(3, 2, figsize=(11.7, 8.3), sharex=True)
        fig.subplots_adjust(top=.82, bottom=.18, left=.075, right=.975, hspace=.22, wspace=.23)
        fig.suptitle(title, x=.075, ha="left", fontsize=18, weight="bold", y=.98)
        fig.text(.075, .922, subtitle, fontsize=10)
        for row, label, limits in ((0, "U4 junction (°C)", (20, 135)), (1, "Battery (°C)", (20, 56)),
                                   (2, "Charge current (A)\n60-second mean", (-.05, 1.95))):
            for ax in axes[row]:
                ax.set_ylabel(label)
                ax.set_ylim(*limits)
                ax.set_xlim(0, 90)
                ax.set_xticks(range(0, 91, 15))
        for ax in axes[1]:
            ax.axhline(charge.HOT, color="#888888", ls=":", lw=1)
        for ax in axes[2]:
            ax.set_xlabel("Time after cold start (minutes)")
        return fig, axes

    def curves(axes, column, data, color, label, style="-"):
        for row, value in ((0, 1), (1, 5)):
            axes[row, column].plot(data[:, 0], data[:, value], color=color, label=label, lw=1.1, ls=style)
        times, current = average_current(data)
        axes[2, column].plot(times, current, color=color, label=label, lw=1.4, ls=style)

    def finish(fig, stem, labels, footer):
        handles = [line for line in fig.axes[0].get_lines() if line.get_label() in labels]
        fig.legend(handles, labels, loc="upper left", bbox_to_anchor=(.07, .904), ncol=len(labels), frameon=False, fontsize=9)
        fig.text(.075, .055, footer, fontsize=9, linespacing=1.5)
        figures.append((stem, fig))

    labels = ["TREG %.0f °C" % t for t in TREGS]
    for pack_key, pack in charge.PACKS.items():
        for ambient in (25, 35):
            fig, axes = page(pack["label"] + " | BQ25798 regulation set point",
                             "%d °C ambient; 0.8 A system load; bottom plate; existing vents; fixed 3.7 V cell" % ambient)
            for column, (imax, mode) in enumerate(((1.8, "host"), (1.0, "default"))):
                axes[0, column].set_title("%s mode, %.1f A" % (mode, imax), weight="bold")
                for ax in axes[0]:
                    for treg in TREGS:
                        ax.axhline(treg, color="#888888", ls=":", lw=.8)
                for treg, color in zip(TREGS, COLORS):
                    curves(axes, column, results[(pack_key, ambient, mode, treg)][0], color, "TREG %.0f °C" % treg)
            stem = "charging_limits_" + pack_key + ("_warm" if ambient == 35 else "")
            finish(fig, stem, labels, "Host mode: ESP32-S3 sets ICHG 1.8 A and TREG over I2C. Default mode: after power-on or watchdog "
                   "expiry, 1 A and TREG 120 °C.\nThe chip holds the junction at TREG by reducing the charge current. "
                   "Curves end at 55 °C pack; thermostat reset is not modelled.")

    fig, axes = page("How uncertain is the heating delay?", "25 °C ambient; host mode 1.8 A, TREG 100 °C")
    labels2 = ("Base estimate", "Half thermal mass", "Double thermal mass", "Extra PCB-to-pack path")
    policy = ("host 1.8 A, TREG 100", 1.8, 100.0, None)
    for column, (pack_key, pack) in enumerate(charge.PACKS.items()):
        axes[0, column].set_title(pack["label"], weight="bold")
        for options, color, label, style in zip(({}, {"capacity_scale": .5}, {"capacity_scale": 2}, {"direct_pack_r": 100.0}),
                                                COLORS, labels2, ("-", "--", "--", ":")):
            data, error = charge.simulate(cal, pack, policy=policy, dt=DT, **options)
            curves(axes, column, data, color, label, style)
            sensitivities.append({"pack": pack_key, "scenario": label, **summary(data, error)})
    finish(fig, "charging_limits_delay", labels2, "Heat capacities and the pack path are assumptions, not measurements.")

    with PdfPages(out / "charging_limits.pdf", metadata={"Title": "BQ25798 regulation set point sweep"}) as pdf:
        for stem, fig in figures:
            fig.savefig(out / (stem + ".svg"))
            pdf.savefig(fig)
            plt.close(fig)
    return sensitivities, [stem for stem, _ in figures]


def report_treg(cal, results, sensitivities, stems, error):
    lines = ["# Fast charging against the BQ25798 regulation set point", "",
             "Generated by `hardware/sim/run_charge_limits.py --write`. [Combined PDF](charging_limits.pdf).", "",
             "SVGs: " + ", ".join("[%s](%s.svg)" % (s, s) for s in stems) + ".", "",
             "## Control and assumptions", "",
             "Host mode: the ESP32-S3 writes ICHG 1.8 A and the TREG register (60/80/100/120 °C) and pets the watchdog. "
             "Default mode: 1 A and TREG 120 °C, what the chip does after power-on or when the watchdog expires. "
             "The charger holds the junction at TREG with its analog loop (zero current %.0f K above TREG in this model). "
             "The nominal pack JEITA stop/restart at 42.1/40.8 °C takes priority. USB keeps supplying the system when "
             "charging is reduced." % charge.heat.CHG["foldback_width"], "",
             "Other assumptions as in [charging.md](charging.md): fixed 3.7 V cell, USB 5 V / 3 A, 0.8 A system load, "
             "bottom plate, 5 cm² inlet/outlet, estimated thermal masses, unrouted copper paths assumed where the board "
             "has none yet. No SOC evolution, CV taper, charger timer or battery ageing.", "",
             "## Nominal 25 °C ambient results", "",
             "| Pack | Mode | TREG | Peak U4 | Pack stop | Charge added |", "|---|---|---|---|---|---|"]
    for (pack_key, ambient, mode, treg), (_, s) in results.items():
        if ambient != 25:
            continue
        stop = "%.1f min" % s["pack_stop_min"] if s["pack_stop_min"] is not None else "none"
        lines.append("| %s mAh | %s | %.0f °C | %.1f °C | %s | %.2f Ah |" % (pack_key, mode, treg, s["peak_u4"], stop, s["charge_ah"]))
    lines += ["", "## Validation", "",
              "Reduced steady model agrees with its board calibration within %.3g K. Maximum energy residual %.3g J per step." %
              (error, max(s["energy_error_j"] for _, s in results.values())), "",
              "Board SHA256: `%s`. Full inputs, 35 °C results and sensitivity results: [charging_limits.json](charging_limits.json)." % cal.pcb_sha256, ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--write", action="store_true")
    project.add_argument(parser)
    args = parser.parse_args()
    if args.project:
        charge.heat.select_project(args.project)
        charge.bind_pack_stop()
    out = Path(charge.heat.OUT)
    cal = charge.calibrate()
    error = charge.steady_error(cal)
    treg_mode = len(charge.heat.CHG["treg_options"]) > 1
    results = run_treg(cal) if treg_mode else run(cal)
    if args.write:
        sensitivities, stems = (graphs_treg if treg_mode else graphs)(cal, results, out)
        root = Path(charge.heat.HERE).parents[1]
        payload = {"pcb_sha256": cal.pcb_sha256, "dt_s": DT, "calibration": asdict(cal),
                   "input_sha256": {str(Path(p).resolve().relative_to(root)): hashlib.sha256(Path(p).read_bytes()).hexdigest()
                                    for p in (charge.heat.PARAMS, charge.heat.__file__, charge.__file__, __file__)},
                   "scenarios": [{"pack": k[0], "ambient": k[1], ("mode" if treg_mode else "hysteresis"): k[2],
                                  ("treg" if treg_mode else "trip"): k[3], **v[1]} for k, v in results.items()],
                   "sensitivity": sensitivities}
        if not treg_mode:
            payload.update({"trips_c": TRIPS, "hysteresis_c": [5, 10], "sensor_tau_s": 2, "sensor_node": "local PCB"})
        (out / "charging_limits.json").write_text(json.dumps(payload, indent=2, default=lambda a: a.tolist()) + "\n")
        (out / "charging_limits.md").write_text((report_treg if treg_mode else report)(cal, results, sensitivities, stems, error))
    for key, (_, stats) in results.items():
        if key[1] == 25 and key[2] in (10, "host"):
            print(key, stats, flush=True)


if __name__ == "__main__":
    main()
