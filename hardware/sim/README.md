# Circuit simulation

The active target is `carrier-ti-charger`, with BQ25798, TPS61088 and TPS259474L module feeds. PCB and schematics define the hardware. MAX77757 models and results are frozen reference material; candidate boost studies describe proposed silicon, not fitted parts.

## Setup and running

Python 3.10 or newer, NumPy, SciPy, Matplotlib and ngspice (version 47 used):

```sh
python3 -m venv hardware/sim/.venv
source hardware/sim/.venv/bin/activate
python -m pip install -r hardware/sim/requirements.txt
```

Run from the repository root. Without `--write`, scripts print reports without replacing result files.

| Command | Purpose and output with `--write` |
|---|---|
| `python hardware/sim/run_thermal.py --write` | TS divider and thermostat, `thermal/results_ti.md` |
| `python hardware/sim/run_power.py --write` | Converter and undervoltage models, `power/results_ti.md` |
| `python hardware/sim/run_heat.py --write` | Board loss, steady heat and `heat/ti/calibration.json` |
| `python hardware/sim/run_charge_heat.py --write` | Transient charging, `heat/ti/charging.md` and plots |
| `python hardware/sim/run_charge_limits.py --write` | TREG/host-mode sweeps, `heat/ti/charging_limits.md` and plots |
| `python hardware/sim/run_emi.py --write` | Coupling and emissions, `emi/results_ti.md` and spectra |
| `python hardware/sim/vent_model.py` | Enclosure airflow/temperature sweep; no files written |

Board-reading scripts select `--project NAME` or `CARRIER_PROJECT`; the default is TI. Legacy MAX results require explicit project selection. Run the heat model first after layout changes because power and vent models consume its calibration. Refill and save PCB zones in KiCad before regenerating geometry-dependent results.

Common options:

- `run_thermal.py --mc N --seed N`: Monte Carlo count and seed; zero count skips sampling.
- `run_power.py --fast`, `run_emi.py --fast`: skip ngspice benches.
- `run_heat.py --pcb FILE`, `--no-heatsink`: choose input board or omit the U4 top sink.
- `run_heat.py` and `run_emi.py --boost tps61023` or `--boost tps61378q1`: candidate comparison only. These do not establish a candidate layout or change the fitted design.

Regression checks:

```sh
python -m unittest discover -s hardware/sim -p 'test_*.py'
tools/check.sh
```

An exit code of 1 can indicate a modeled design-limit violation, not only a solver failure. `tools/check.sh` also checks current ERC, DRC, parity and value bindings.

## Source ownership and stored results

`chargers.py` owns charger-specific constants and operating cases. `boosts.py` owns candidate assumptions. `run_heat.py` reads tracks, pads, vias, stored zones and stackup; an unrouted path is reported and substituted by an assumed 5 mΩ path. Never interpret that substitution as measured copper resistance.

Active outputs use `results_ti.md` in shared directories and `heat/ti/` for thermal work. MAX outputs use `results_max.md` or `heat/max/`. Candidate filenames carry a boost suffix. Preserve their recorded inputs when reading historical comparisons.

Before relying on stored geometry-dependent results, compare their board hash and calibration provenance with the current PCB. The charging reports and candidate studies can predate the steady-state report or current constants; regenerate all relevant reports together for a design decision. Re-running unit tests does not refresh reports.

## Model boundaries

### Converter and module-feed behavior

Converter benches are idealized power stages and closed-form calculations, not validated vendor macromodels. They evaluate TPS61088 loop margins, ripple, startup, load steps and undervoltage behavior. They do not prove a real controller's switching or fault response.

U11/U12 use the TPS259474L model on TI and LM73100 on the frozen MAX target. Each branch uses forward regulation, on-resistance and reverse blocking. Static OVLO checks use the fitted TI dividers. Transient benches omit eFuse enable/UVLO logic, OVLO switching, switching delays and parasitics; startup, circuit-breaker timing and recovery remain hardware checks.

Module input capacitance is assumed to total 40 µF, split 83.5 % ADSBee/GNSS and 16.5 % display, with 20 mΩ connector resistance per branch. The module voltage window, branch load split and capacitance require measurement. Compensation and transient claims apply only to these assumptions and fitted capacitor derating.

Candidate models reuse existing board geometry because no candidate layout exists. TPS61023 PFM timing is unpublished. TPS61378-Q1 uses a candidate 3.3 µH CKST0603 and a future loop-area constraint; old XGL5050 reports are superseded comparisons, not current hardware evidence.

### TS and thermostat

The BQ25798 TS network is evaluated against percent-of-REGN thresholds; Q2/R12 are checked against CE VIL/VIH. The models verify external node voltages, not the charger's internal reaction.

The NTC uses its B-value model because the series datasheet lacks an R/T table. TS leakage is assumed, not specified. FET fits and leakage corners do not establish hot behavior at the actual operating voltage. Harness noise, contact bounce, sensor attachment, self-heating and thermal lag require separate validation. JSCJ 2N7002 data is available in `datasheets/`; check which manufacturer's limits a particular model uses.

### Loss and temperature

Losses combine switch resistance, data-sheet efficiency curves, estimated inductor core loss, contacts, fuse, pack resistance and PCB copper. BQ25798 uses its own efficiency curve; TPS61088 uses the budget curve. Resistances include estimated temperature coefficients.

The thermal board is a 1 mm grid of four copper sheets on FR4, coupled by vias and pins, with estimated surface convection/radiation. The TI arrangement is a top heatsink on U4 through a copper spacer and epoxy. A bottom plate is not part of the selected design. Contact resistance and sink/air coupling remain estimates.

The enclosure assumes 115 x 105 x 40 mm, 2 mm printed walls, four 8 cm² vents, 50 % open grilles and 35 mm vertical separation. `vent_model.py` defines the shared vent constants. Calibration binds to the board snapshot; verify real airflow, heatsink bonding and pack temperature before accepting cooling performance.

Steady temperatures above a TS or thermostat threshold are hypothetical when that protective response is not simulated. Transient charging models include their stated thermal inertia and control policies; they do not replace an actual full-charge test.

### EMI and receiver desense

`run_emi.py` estimates converter hot-loop, digital and cable coupling from board geometry and assumed edge rates. Receiver-band dipole bounds and CISPR comparisons are planning estimates, not compliance or sensitivity measurements. FB1 behavior under DC bias, actual switch-node waveforms, antenna coupling and the cockpit noise floor remain unmeasured.

Near-field probing, conducted measurements with the real adapter and receiver comparisons with each converter on/off decide the physical result. Keep numerical acceptance bounds and assumptions in the generated report rather than copying them into several design documents.
