# Circuit simulation

ngspice benches and closed-form checks for the parts of the carrier that can be simulated.
Simulated numbers live in the three `results.md` files. The design itself is described in [docs/power.md](../../docs/power.md).

## Setup

Python 3.10 or newer. The heat model needs NumPy and SciPy, the other suites use only the standard library.

```sh
python3 -m venv hardware/sim/.venv
source hardware/sim/.venv/bin/activate
python -m pip install -r hardware/sim/requirements.txt
```

The thermal cut-off and converter suites need ngspice on the PATH (`brew install ngspice`, version 47 used).

## Running

```sh
python3 hardware/sim/run_thermal.py --write    # thermal charge cut-off, report in thermal/results.md
python3 hardware/sim/run_power.py --write      # converters, report in power/results.md
python3 hardware/sim/run_heat.py --write       # power loss and temperatures, report in heat/results.md
python3 hardware/sim/vent_model.py             # vented enclosure sweep, prints to the terminal
```

Without `--write` a script prints its report and stores nothing. `vent_model.py` has no options and writes no file.

| Script | Option | Effect |
|---|---|---|
| `run_thermal.py` | `--mc N` | Monte Carlo units, default 400, 0 skips the run |
| `run_thermal.py` | `--seed N` | Random seed, default 1 |
| `run_power.py` | `--fast` | Skip the ngspice benches, closed-form checks only |
| `run_heat.py` | `--pcb FILE` | Read another board file, for example a copy with refilled zones |
| `run_heat.py` | `--no-heatsink` | Model the board without the U4 heatsink |

The ngspice suites take a few seconds. The heat run takes about 20 seconds.
The exit code is 1 if a model check fails. Such a failure describes a modelled design limit, not necessarily a solver failure.

Solver and operating-point regressions:

```sh
python -m unittest discover -s hardware/sim -p 'test_*.py'
```

## Files

Shared and scripts:

| Path | Content |
|---|---|
| `simlib.py` | ngspice batch runs with `.param` overrides, waveform helpers, Markdown report |
| `run_thermal.py` | Thermal charge cut-off: runs the benches over all corners and checks them against data sheet thresholds |
| `run_power.py` | Converters: TPS61088 loop gain in closed form, ripple and transient benches, undervoltage cut-off, switch-node coupling read from the board file |
| `run_heat.py` | Power loss and temperatures on the routed board inside an assumed enclosure, with and without the U4 heatsink |
| `vent_model.py` | Lumped vented-enclosure model: stack-effect vents and a fan against case air, pack and U4 temperature |
| `test_power.py` | Module-feed forward drop, branch currents, reverse isolation and shutdown solver regressions |
| `test_heat.py` | Analytic network checks, layer and via temperatures, zero-current cases, numerical failure checks, operating-point regressions, vent model constants against the heat model |
| `requirements.txt` | NumPy and SciPy for the heat model |

Thermal charge cut-off:

| Path | Content |
|---|---|
| `thermal/carrier_thermal.inc` | The network as drawn: R8, thermistor, R43 and the THERM_FEED lead, Q2, Q6, R10, R12, C31, R13. Net and reference names match the schematic |
| `thermal/jeita_sweep.cir` | THM/PVL ratio over pack temperature, thermostat closed |
| `thermal/thermostat_trip.cir` | Transient: thermostat opens at 10 ms and closes at 600 ms |
| `thermal/fet_check.cir` | Checks the FET models against the data sheet points they are fitted to |
| `thermal/results.md` | Last stored report |
| `models/ntc_entc_eb_100k4262.lib` | Variohm ENTC-EB-100K4262-02, B model with interchangeability offset and B tolerance |
| `models/fets.lib` | AO3401A and 2N7002 with the threshold as a parameter |

Converters:

| Path | Content |
|---|---|
| `power/boost_ripple.cir` | TPS61088 power stage, open loop with a slow duty trim, real inductor and derated capacitors |
| `power/buck_ripple.cir` | MAX77757 buck power stage, same approach, for the VSYS ripple |
| `power/boost_transient.cir` | TPS61088 averaged closed-loop model with soft start, TPS22975 slew and discharge, module capacitance: start-up, load steps, 1.5 A pulses, shutdown |
| `power/module_feeds.inc` | Shared U11/U12 LM73100 forward-drop and reverse-blocking model, C38/C39 and separate module loads |
| `power/undervoltage.cir` | Battery with pack and harness resistance, TLV840 with hysteresis and reset delay, the latch, and the model above: cut-off, rebound, load burst near cut-off |
| `power/results.md` | Last stored report |

Power loss and temperatures:

| Path | Content |
|---|---|
| `heat/results.md` | Last stored report |
| `heat/map_*.svg` | Top-copper temperature maps, see below |

The maps cover four cases at 25 °C ambient:

| File stem | Case |
|---|---|
| `map_usb32_closed_25` | Charging at 3.2 V cell voltage, closed case |
| `map_usb37_open_25` | Charging at 3.7 V cell voltage, open bench |
| `map_bat08_closed_25` | Battery, 0.8 A load, closed case |
| `map_bat08_open_25` | Battery, 0.8 A load, open bench |

Each case is a pair on one colour scale. The plain file includes the U4 heatsink, with a dashed square at its footprint. The `_bare` file is the same case without it.

## Running a bench on its own

Each bench also runs standalone and writes a `.txt` table next to it:

```sh
cd hardware/sim/thermal && ngspice -b jeita_sweep.cir
```

A standalone run uses the `.param` defaults in the netlist, not the values the scripts pass.
For the thermal benches the defaults are the nominal design values.
The boost benches default to the board's switching frequency (721 kHz at 3.7 V input, 707 kHz at 3.0 V in `undervoltage.cir`). Other defaults in `undervoltage.cir` differ from the scenario `run_power.py` reports.

The thermal network is not read from the schematic. After a schematic change to it, update `carrier_thermal.inc` by hand.

## What the results mean

### Converters

There is no usable SPICE model for either converter. TI's TPS61088 models are for PSpice for TI, and ADI publishes none for the MAX77757.

The boost loop is evaluated in closed form with the small-signal model from the data sheet (SLVSCM8D, Equations 13 to 20).
That model is a current-mode power stage with load pole, ESR zero and right-half-plane zero, plus the error amplifier with R18, C21 and C22.
It reproduces TI's design procedure, not the chip's internals.
Phase margin is therefore a data sheet figure, not a measurement.

Not in the loop model:

- sampling effects near half the switching frequency
- slope compensation
- the quasi-constant-frequency control
- the MAX77757 buck loop, which is internal and undocumented; only its ripple is evaluated

The ripple benches switch the real power stages open loop at the operating point, with a slow integrator holding the mean output.
They give the inductor current and the capacitor ripple with DC-bias derating, ESR and ESL.
Two assumptions dominate the fine structure: the copper loop inductance between switch, capacitors and PGND (0.3 nH assumed), and the switch-node capacitance with its damping.
They decide the edge ringing, not the 20 MHz ripple, and are bench items.

The transient and undervoltage benches include U11/U12 as separate LM73100 feeds. Their forward drop is the larger of the regulated forward voltage and branch current times on-resistance, with typical and maximum data sheet corners matched to the heat model. A native diode supplies reverse blocking and numerical limiting; its extra forward drop is less than 0.6 mV at 1.5 A. This is a quasistatic model, not a vendor macromodel. It omits the chip's enable/UVLO logic, switching delays and parasitics.

The assumed 40 µF module input capacitance and total load are divided 83.5 % to ADSBee/GNSS and 16.5 % to the display, using the heat model's Wi-Fi-on load split. Each branch has 20 mΩ of connector resistance. Both terminal voltages are checked; reported minima and maxima cover both. The undervoltage bench uses maximum diode drop. The split, capacitance and module voltage limits still need measurement. C38 and C39 contribute 100 nF each upstream of the diodes; module capacitance is isolated from U7's discharge path.

The coupling section reads track and pad geometry from the board file.
Capacitive coupling uses a coplanar-strip formula for the parallel run and a 5 pF victim node.
The magnetic bound treats the shielded inductor as a dipole with 10 % leakage.
Both are order-of-magnitude estimates.

### Thermal charge cut-off

The MAX77757 has no SPICE model. It is represented by an ideal PVL source, a THM leakage current and its data sheet thresholds:

| Threshold | THM/PVL |
|---|---|
| cold | 74 % |
| cool | 65 % |
| warm | 32.5 % |
| hot | 23 % |
| charger disable | 6 % |

Each threshold carries ±1.5 points and 1 point of hysteresis.

The simulation verifies the network around the chip: the right voltage reaches THM and PWR_EN in every state and across tolerances.
It does not verify how the charger reacts. That remains a bench test ([docs/open-issues.md](../../docs/open-issues.md)).

Assumptions that affect the numbers:

- The thermistor uses the exponential B model from B25/85, because its data sheet has no R/T table. The nominal transitions may be off by up to about 1 °C.
- PVL is taken as 1.8 V. The data sheet gives no value. The thresholds are ratios, so PVL only scales the effect of leakage currents.
- The FET models are data sheet fits with a 90 mV/decade subthreshold swing, which no data sheet guarantees.
- The 2N7002 limits are Nexperia's. The carrier fits the JSCJ part, for which the project has no data sheet.
- FET parameters are for 25 °C. Thresholds fall and leakage rises with temperature. The 5 µA Q2 leakage case uses the 55 °C limit with 25 °C thresholds.
- Leakage limits are taken at face value, although they are specified at −30 V (AO3401A) and 48 V (2N7002). At 2 to 4 V the real leakage is normally far lower.
- The thermistor is at pack temperature. Thermal coupling to the pack, self-heating (under 10 µW here) and the cable are not modelled.
- Thermostat contact bounce and noise pickup on the thermistor cable are not modelled. The THM filter capacitor C35 is not in the network; the thresholds are DC comparisons.

### Power loss and temperatures

`run_heat.py` reads the routed board: tracks, vias, pads, zone fills and stackup. It needs no simulator.

Losses:

- MAX77757, TPS61088 and TPS22975 from their data sheet switch resistances with a temperature coefficient
- the inductors from DCR plus an estimated core loss
- a switching term fitted to the TPS61088 efficiency curve in `specs/power_params.json`, borrowed for the MAX77757, which publishes no curve
- every power net as a resistor network of its tracks and vias, the GND pour as a four-layer sheet
- contacts (USB-C, JST-PH, shunts, module sockets), the fuse and the pack with their resistance

Temperatures:

- The board is a 1 mm grid of four copper sheets on FR4. Each cell's copper share is rasterised from the stored zone fills and the copper items.
- Vias and through-hole pins connect the layers. Each face has a convection plus radiation coefficient.
- Junctions sit on their pads through the data sheet θJC, the inductor windings through an estimated 12 K/W.
- The board's own θJA follows from that. The JEDEC θJA values are printed for comparison only.
- The MAX77757 thermal regulation is modelled: above 130 °C junction the charge current folds back, then the input current, until the junction holds 130 °C.

Solver:

- The conductance networks use sparse direct solves. The thermal matrix is factored once per operating case and reused as the losses change.
- Each solve checks its residual against the requested tolerance. Singular systems and unconverged operating points stop the run.
- Tracks use their own layer's temperature. A via segment uses the mean temperature of its adjoining layers and splits its heat between them.
- Ground-plane losses are recalculated at each thermal iteration.
- The solver is checked against the analytic solution for a point source on an infinite plate.

Assumptions that decide the numbers:

- The enclosure does not exist yet. The closed case is a sealed 115 x 105 x 40 mm printed box (6.3 K/W to ambient) holding board, modules and pack. Everything dissipated inside heats its air.
- The open case is the board on a bench. Real numbers lie between the two and depend on vents.
- The MAX77757 switching loss is an estimate. The TPS61088 fit rests on the project's curve, which is TI's figure less an estimated 2 points.
- Contact, fuse and pack DC resistances are data sheet maxima or estimates. The USB cable is not included.
- Zone fills are read as stored. Refill the zones in KiCad before a run after layout changes, or pass a refilled copy with `--pcb`.

### Vented enclosure

`vent_model.py` adds a vent path and a fan path in parallel with the sealed wall path of the heat model.
It is a lumped steady-state estimate of case air, pack and U4 temperature over vent area.

- The enclosure parameters and the component rises over case air are constants copied from the heat model (closed case, 25 °C, high corner). Update them by hand after the heat model changes; `test_heat.py` fails when they drift.
- Vents are modelled as stack-effect openings, inlet low and outlet high, with a 50 % open grille.
