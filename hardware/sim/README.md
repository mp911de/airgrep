# Circuit simulation

ngspice benches and closed-form checks for the parts of the carrier that can be simulated.
Simulated numbers live in the four `results.md` files. The design itself is described in [docs/power.md](../../docs/power.md).

## Setup

Python 3.10 or newer. The heat and EMI models need NumPy (heat also SciPy). Charging transient graphs use Matplotlib. The other suites use only the standard library.

```sh
python3 -m venv hardware/sim/.venv
source hardware/sim/.venv/bin/activate
python -m pip install -r hardware/sim/requirements.txt
```

The thermal cut-off, converter and EMI suites need ngspice on the PATH (`brew install ngspice`, version 47 used).

## Running

```sh
python3 hardware/sim/run_thermal.py --write    # thermal charge cut-off, report in thermal/results_<charger>.md
python3 hardware/sim/run_power.py --write      # converters, report in power/results_<charger>.md
python3 hardware/sim/run_heat.py --write       # power loss and temperatures, report in heat/<charger>/results.md
python3 hardware/sim/run_charge_heat.py --write # battery heating delay, heat/<charger>/charging.pdf and SVGs
python3 hardware/sim/run_charge_limits.py --write # MAX: TMP300 trip sweeps; TI: TREG and host-mode sweeps
python3 hardware/sim/run_emi.py --write        # emission estimate, report in emi/results_<charger>.md, FLARM spectrum in emi/spectrum_flarm_<charger>.svg/.pdf
python3 hardware/sim/vent_model.py             # vented enclosure sweep, prints to the terminal
```

Without `--write` a script prints its report and stores nothing. `vent_model.py` has no options and writes no file.

Every board-reading script takes `--project NAME` (or `CARRIER_PROJECT`), default `carrier-ti-charger`. The heat outputs land in `heat/ti/` or `heat/max/` after the project's charger; the shared folders carry the charger tag in the file name (`results_ti.md`, `results_max.md`). `chargers.py` holds the per-charger constants (switch resistances, frequency, TREG, package thermal figures, pad numbers, heatsink or bottom plate, efficiency curve); `run_heat.py --write` also writes `heat/<charger>/calibration.json`, which `vent_model.py` and `run_power.py` read instead of hand-copied numbers. An unrouted net (the TI board until Mark routes it) is replaced by an assumed 5 mΩ path and listed in the report.

| Script | Option | Effect |
|---|---|---|
| `run_thermal.py` | `--mc N` | Monte Carlo units, default 400, 0 skips the run |
| `run_thermal.py` | `--seed N` | Random seed, default 1 |
| `run_power.py` | `--fast` | Skip the ngspice benches, closed-form checks only |
| `run_heat.py` | `--pcb FILE` | Read another board file, for example a copy with refilled zones |
| `run_heat.py` | `--no-heatsink` | Model the board without the U4 heatsink |
| `run_emi.py` | `--fast` | Skip the ngspice benches, closed-form spectra and board checks only |

The ngspice suites take a few seconds, the EMI run about 10. The heat run takes about 20 seconds.
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
| `chargers.py` | One entry per charger (MAX77757, BQ25798): data sheet constants with their sources, operating cases, heatsink or plate, transient policies |
| `run_thermal.py` | Thermal charge cut-off: runs the benches over all corners and checks them against data sheet thresholds |
| `run_power.py` | Converters: TPS61088 loop gain in closed form, ripple and transient benches, undervoltage cut-off, switch-node coupling read from the board file |
| `run_heat.py` | Power loss and temperatures on the routed board inside an assumed enclosure, with and without the U4 heatsink (MAX) or bottom plate (TI) |
| `run_charge_heat.py` | Five-node transient charging model, pack inertia and JEITA cutoff, 5 Ah and 10 Ah graphs |
| `run_charge_limits.py` | MAX: full-current charging with 80/90/100/110 °C TMP300 sensor trips and 5/10 °C hysteresis. TI: 1.8 A host and 1 A default mode over TREG 80/100/120 °C. Thermal-mass sensitivity for both |
| `run_emi.py` | Emission estimate: converter hot loops and edges, digital signals, radiated and conducted levels against CISPR 32 class B, converter lines in the receiver bands, return paths and rail ripple, all with geometry from the board |
| `vent_model.py` | Lumped vented-enclosure model: stack-effect vents and a fan against case air, pack and U4 temperature |
| `test_power.py` | Module-feed forward drop, branch currents, reverse isolation and shutdown solver regressions |
| `test_heat.py` | Analytic network checks, layer and via temperatures, zero-current cases, numerical failure checks, operating-point regressions, vent model constants against the heat model |
| `test_charge_heat.py` | Transient energy balance, thermal inertia, pack cutoff, charger hysteresis, sensor lag and timestep convergence |
| `test_emi.py` | Spectrum helpers against closed forms, Ott's loop formula, limits, microstrip, geometry, and the board: hot loops found, In1 solid, return-path gaps |
| `requirements.txt` | NumPy and SciPy for the heat and EMI models |

Thermal charge cut-off:

| Path | Content |
|---|---|
| `thermal/carrier_thermal.inc` | MAX77757 network as drawn: R8, thermistor, R43 and the THERM_FEED lead, Q2, Q6, R10, R12, C31, R13. Net and reference names match the schematic |
| `thermal/carrier_thermal_ti.inc` | BQ25798 network: REGN, RT1/RT2 (R59/R60), thermistor, the thermostat chain to CE with its 1 µA pin leakage |
| `thermal/jeita_sweep.cir`, `ts_sweep_ti.cir` | THM/PVL or TS/REGN ratio over pack temperature, thermostat closed |
| `thermal/thermostat_trip.cir`, `thermostat_trip_ti.cir` | Transient: thermostat opens at 10 ms and closes at 600 ms |
| `thermal/fet_check.cir` | Checks the FET models against the data sheet points they are fitted to |
| `thermal/results_<charger>.md` | Last stored report per charger |
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
| `power/results_<charger>.md` | Last stored report per charger |

Power loss and temperatures:

| Path | Content |
|---|---|
| `heat/max/`, `heat/ti/` | One folder per charger, contents below |
| `heat/*/results.md` | Last stored report |
| `heat/*/map_*.svg` | Top-copper temperature maps, see below |
| `heat/*/charging.pdf`, `heat/*/charging_*.svg` | Both pack sizes, current limits, heating-delay sensitivity and illustrative external spreader paths |
| `heat/*/charging.md`, `heat/*/charging.json` | Transient assumptions, results, board/input hashes and steady calibration points |
| `heat/*/calibration.json` | Sealed-case scenario constants and copper path resistances for `vent_model.py` and `run_power.py` |
| `heat/*/charging_limits.pdf`, `heat/*/charging_limits_*.svg` | MAX: TMP300 comparison graphs for both packs, 25/35 °C ambient, delay/tolerance scenarios and raw switching cycles. TI: TREG sweep in host and default mode |
| `heat/*/charging_limits.md`, `heat/*/charging_limits.json` | Threshold-sweep assumptions, results and input hashes; original charging outputs remain untouched |

EMI:

| Path | Content |
|---|---|
| `emi/switch_edge.cir` | One switching period of a converter hot loop at 10 ps: gate-limited switches, body diodes, switch output capacitances, the loop inductance from the board, capacitor bank, optional snubber. Serves both converters through gate weights |
| `emi/usb_lisn.cir` | Buck input current pulses into the BYP, CHGIN and J1 capacitors, the USB_VBUS copper, a USB cable and a CISPR 16 LISN; optional bead at J1 and adapter output capacitor |
| `emi/results_<charger>.md` | Last stored report per charger |
| `emi/spectrum_flarm_<charger>.svg`, `.pdf` | Converter and digital lines at the FLARM antenna, 1 MHz to 2 GHz and a zoom on 868.2 / 868.4 MHz |

The maps cover four cases at 25 °C ambient:

| File stem | Case |
|---|---|
| `map_usb32_closed_25` | Charging at 3.2 V cell voltage, closed case |
| `map_usb37_open_25` | Charging at 3.7 V cell voltage, open bench |
| `map_bat08_closed_25` | Battery, 0.8 A load, closed case |
| `map_bat08_open_25` | Battery, 0.8 A load, open bench |

Each case is a pair on one colour scale. The plain file includes the U4 heatsink (the 10 x 10 mm top sink on both boards; `type="plate"` in chargers.py models a bottom-side plate instead), with a dashed square at its footprint. The `_bare` file is the same case without it.

## Running a bench on its own

Each bench also runs standalone and writes a `.txt` table next to it:

```sh
cd hardware/sim/thermal && ngspice -b jeita_sweep.cir
```

A standalone run uses the `.param` defaults in the netlist, not the values the scripts pass.
For the thermal benches the defaults are the nominal design values.
The boost benches default to the board's switching frequency (721 kHz at 3.7 V input, 707 kHz at 3.0 V in `undervoltage.cir`). Other defaults in `undervoltage.cir` differ from the scenario `run_power.py` reports.

The thermal network is not read from the schematic. After a schematic change to it, update `carrier_thermal.inc` or `carrier_thermal_ti.inc` by hand.

## What the results mean

### Converters

There is no usable SPICE model for any converter. TI's TPS61088 models are for PSpice for TI, ADI publishes none for the MAX77757, and TI none for the BQ25798.

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

Neither charger has a SPICE model. For the BQ25798 the TS divider from REGN is evaluated against its percent-of-REGN thresholds (T1 73.3, T2 68.4, T3 44.8, T5 34.2 %), and the thermostat chain against the CE pin thresholds (1.3 V high, 0.4 V low). The MAX77757 has no SPICE model. It is represented by an ideal PVL source, a THM leakage current and its data sheet thresholds:

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
- every power net as a resistor network of its tracks, vias and pours (zone fills as 0.25 mm cells; a track end or via within its own copper radius of a pour cell joins it), the GND pour as a four-layer sheet
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

- The enclosure does not exist yet. The closed case is a 115 x 105 x 40 mm printed box holding board, modules and pack, with a 5 cm² inlet and a 5 cm² outlet. The wall path is 6.3 K/W to ambient. Natural draft through the vents adds a parallel heat path. Everything dissipated inside heats its air.
- The open case is the board on a bench. Real numbers lie between the two and depend on vents.
- The MAX77757 switching loss is an estimate. The TPS61088 fit rests on the project's curve, which is TI's figure less an estimated 2 points.
- Contact, fuse and pack DC resistances are data sheet maxima or estimates. The USB cable is not included.
- Zone fills are read as stored. Refill the zones in KiCad before a run after layout changes, or pass a refilled copy with `--pcb`.

### EMI

`run_emi.py` is a pre-compliance estimate, good to an order of magnitude. It exists to rank the sources and to show which knob moves each one before a chamber visit, not to predict a pass.

Sources:

- The converters are simulated for one period in `emi/switch_edge.cir`. The switches conduct the smaller of their ohmic current and a gate-limited current, so the edge parameter sets the current slew (10 A per gate ramp, 5 ns nominal, 2 and 10 ns swept). The loop inductance is derived from the board: the copper runs between the pads of the hot loop as microstrip over the plane 0.2 mm below, plus capacitor ESL, vias and 1 nH for the package. The loop area is the polygon through the pad centres. The switch output capacitance is not on either data sheet; 1 nF (boost) and 300 pF (buck) are assumed and swept by a factor of two. Line spectra come from the Fourier transform of one period, cross-checked against the trapezoid closed form at the low harmonics.
- Digital nets get their track length, capacitance and layer from the board. The edge at the far end combines an assumed driver edge (2 to 10 ns) with the RC of the series resistor and the load. Their loop is the track over the plane.
- The inductor leakage field uses the triangle ripple current, 5 turns of 10 mm² and 10 % leakage, as in `run_power.py`.

Levels:

- Radiated: Ott's small-loop formula with the test-site ground reflection at 3 m, against CISPR 32 class B. Lines closer than the receiver bandwidth add as power.
- Receiver bands: the same loops at the antenna (mount position plus 30 mm, free space with the near-field term), received by a dipole with its far-field aperture, compared with the receiver's thermal noise floor less 6 dB. The aperture overstates the coupling at a quarter wavelength; the result ranks the sources, it does not predict the desense in dB.
- Conducted: `emi/usb_lisn.cir` measures the buck input ripple that reaches a LISN at the end of a 1 m USB cable. This is the CISPR 32 method for a DC power port and the worst case for the mains port; a real adapter's output capacitor absorbs most of it, which the adapter row shows.
- Return paths: bottom-layer signal tracks over In2 power copper and crossings of In2 power tracks, sampled every 0.1 mm. In1 is checked to be GND only.
- Rails: track R and L per net, the RAIL_5V ripple reaching the module sockets through the LM73100 and the copper, and the VSYS ripple that drives the untwisted pack leads as a 500 mm² loop.

Not modelled: common-mode currents on the antenna coax and the USB cable, the modules' own emissions (ESP32-S3 clocks and Wi-Fi, RP2040, CC1312), the enclosure, cable resonances, and the electric-dipole radiation of the switch-node copper (negligible at these areas). Digital nets below 1 Hz repetition (PPS) are read with the peak detector in the radiated table, which overstates the quasi-peak by 20 to 40 dB.

### Vented enclosure

`vent_model.py` supplies the shared wall and vent heat balance used by `run_heat.py`, and adds a fan path for its own sweeps.
It is a lumped steady-state estimate of case air, pack and U4 temperature over vent area.

- The lumped model's enclosure parameters and component rises over case air are copied from the sealed heat model (25 °C, high corner). Its consistency test explicitly disables vents in both models. Temperature tolerance is 1 °C; charge-current tolerance is 0.05 A.
- Both models default to 5 cm² per opening, inlet low and outlet high, 35 mm vertical separation and a 50 % open grille. Each opening therefore has 2.5 cm² free area. `VENT_AREA_CM2` and `VENT_HEIGHT_M` in `vent_model.py` set these assumptions. Pass zero vent area for the sealed reference.
