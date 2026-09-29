# Open issues

Open work only, ordered by what blocks the next step. The design itself is described in [power.md](power.md), [mechanical.md](mechanical.md) and [interfaces.md](interfaces.md); closed items are in git.

The three modules are ordered. The carrier board is not.

## Before ordering the board

- [ ] **Reverse-polarity protection at J3.** CSD25404Q3 (LCSC C2865523) selected; design and validate the bidirectional protection stage, including USB-powered reversed-pack insertion and depleted-pack recovery. Coordinate with the planned off-state battery disconnect. No indicator LED; see [issue 0002](../issues/0002-battery-reverse-polarity-protection.md).
- [ ] **Silkscreen.** Labels on the top side:
  - Update the revision text; the board reads "Rev A 2026-09-30".
  - Add "ESP 5V" at JP1. ("REMOVE BEFORE USB" is on the board at JP1 and JP2 since 6 Oct.)
  - Add "THERM" at J4, which is labelled on the bottom only.
  - Add POWER at SW1 and MUTE at SW2.
- [ ] **Battery connector rating.** J3 is JST-PH, rated 2 A per contact. The battery current is about 1.9 A at the 1 A load peak and about 3 A at the 1.5 A headroom near empty, and F1 is 5 A. Accept this for short peaks or change the connector ([interfaces.md](interfaces.md#carrier-connectors)).
- [ ] **Production files.** The set in `hardware/kicad/jlcpcb/` is older than the board: BOM and CPL lack R43, the CPL has outdated positions for R6, R8 and F1, and L1 is exported with the footprint `IND-SMD_L7.4-W6.6` instead of `IND-SMD_L7.0-W6.6_FXL0630`. Export again:
  - Fill all zones, save, run DRC with schematic parity.
  - Export Gerbers, BOM and CPL.
  - Run `tools/kpy tools/fix_cpl.py` after every CPL export. It corrects rotation and position of the parts whose JLCPCB model origin differs from the footprint; see the script header.
  - Record the JLCPCB class (Basic or Extended) per part; the project does not hold it yet.
  - Select stackup JLC04161H-2116C in the order.
- [ ] **Schematic print.** `hardware/kicad/carrier-schematic.pdf` is older than four of the sheets. Print it again.
- [ ] **Insulator board.** Order `hardware/insulator/` together with the carrier ([mechanical.md](mechanical.md#insulator-board)).

## Checks when the modules arrive

These decide whether the board can be ordered as drawn.

- [ ] **ADSBee geometry.** The sockets and holes rest on an image estimate ([adsbee-estimate.md](../mechanical/adsbee-estimate.md)). Measure on the module:
  - Position and pin count of both header rows (J10, J11) and the offset between them.
  - Hole positions against the header rows. MH14 lies outside the 0.5 mm allowance of the estimate ([mechanical.md](mechanical.md#mounting)).
  - Clearance under the rear shield.
  - Access to JP2, which lies under the estimated module outline, and room for a USB plug above the boost converter.
- [ ] **Display fit.**
  - The 8.5 mm socket stack against the controller and the microSD slot on the back of the display.
  - An M2.5 screw in the module hole at MH13.
  - One standoff plus four header groups against the touch force.
- [ ] **GNSS standoff at MH11.** A track passes close to the hole ([mechanical.md](mechanical.md#mounting)). Check the standoff and the screw head on the real hardware.

## Assumptions to measure

The simulations rest on these. Each one changes a result if it is far off.

- [ ] **Module input capacitance on the 5 V rail** (assumed 40 µF) and the modules' input voltage window (assumed 4.75 to 5.25 V, 5.5 V absolute). The diode-inclusive load-step model leaves only 42 mV above the lower limit at maximum LM73100 drop. Measure each branch's capacitance and load share as well as the voltage window; they set the load-step and start-up results.
- [ ] **Peak 5 V current**, in particular ADSBee Wi-Fi bursts (assumed 1.0 A peak, 1.5 A short-term headroom).
- [ ] **Battery pack DC resistance** near empty, new and aged, warm and cold (assumed 50 to 150 mΩ), and harness resistance (assumed 60 to 100 mΩ). They set the voltage at which the undervoltage latch trips.
- [ ] **Boost switch-node ringing** near 1 GHz against the 1090 MHz receiver. The snubber R39/C36 has footprints but is not fitted (start with 2.2 Ω and 1 nF).
- [ ] **Charger and boost temperature** at the intended load, in the enclosure, with the heatsink on U4 (thermocouple on the sink base). Compare against `hardware/sim/heat/results.md`.
- [ ] **GNSS antenna bias current.** It must fit the ADSBee's 200 mA 3.3 V output.

## Bench tests at bring-up

- [ ] **Before first power-up.**
  - Inspect the U6 and U7 solder joints. The exposed pads have unfilled thermal vias next to the paste windows.
  - Confirm the pack lead polarity at J3 (pin 1, square pad, is plus).
  - Check the GNSS module's position in J5. The socket is not keyed; the silkscreen is the only guard.
- [ ] **Charger.**
  - Supplement mode with 0.5, 1, 2 and 3 A sources.
  - Input current with a 1.5 A Type-C source that also shorts D+ and D-. It must stay at 1.5 A.
  - Dead-battery attach to a Type-C source.
  - LED pattern as described in [power.md](power.md#indication).
  - Charging from a weak source may hit the safety timer. This is accepted.
- [ ] **Thermal cut-off.**
  - Sweep the resistance at J2 through all THM transitions.
  - NTC open and shorted.
  - Thermostat trip.
  - Switch off with USB present; no backfeed.
- [ ] **5 V rail.** Start-up, load steps between 0.1 and 0.8 A and between 0.8 and 1 A at the module terminals, boost ripple. Compare against `hardware/sim/power/results.md`.
- [ ] **Undervoltage.** Discharge to cut-off under load. The latch must hold the system off while the battery voltage recovers. Clear it with the slider and with USB. Measure the latched current (about 60 µA expected).
- [ ] **GNSS reception next to the charger.** L1 is closer to the GNSS module than the 40 mm planning target ([mechanical.md](mechanical.md#placement)). Compare reception with and without charging.

## Firmware

- [ ] FLARM 868 MHz decoding is not demonstrated in the ADSBee firmware.
- [ ] Display firmware: gauge readout, Wi-Fi traffic feed, buzzer PWM on D11.
- [ ] BMP581: keep the interrupt output disabled; the INT pin is tied to GND.
- [ ] Load shedding at low state of charge (MAX17048): Wi-Fi off and backlight down. This lowers the battery current near cut-off and moves the cut-off to a lower cell voltage.

## Enclosure

The enclosure is not designed yet. The carrier does not depend on these, the finished device does.

- [ ] **Vents.** In a closed, unvented case the pack reaches the thermostat's opening range at 0.8 A load and 25 °C ambient, and two heat checks fail (`hardware/sim/heat/results.md`). Size the vents with `hardware/sim/vent_model.py` once its constants are current (see below), then check the limits in [specs.md](specs.md#limits-of-the-operating-range) against the result.
- [ ] **Antenna parts.** Confirm the bulkhead cables from the BOM in the 6.5 mm holes MH6 to MH8, the coax bend radius and the routing.
- [ ] **Barometer vent and Wi-Fi antennas.** The case needs an opening for the BMP581 and no metal near the module antennas.

## Optional improvements

- [ ] **Lower battery path resistance** to delay the undervoltage cut-off: a connector with two contacts per pole or JST-XH, and AWG 22 leads. The same change settles the connector rating above.
- [ ] **SMAJ10A instead of SMAJ12A** for D3 if 12 V adapters need not be tolerated. The charger locks out at 13.7 V and withstands 16 V.

## Housekeeping outside the docs

Small corrections in scripts, parameters and project files. None changes the board.

- [ ] `hardware/sim/power/undervoltage.cir` has defaults that differ from what `run_power.py` passes (`vin_uvlo`, `voc0`, `tc`, `tdn`, `tstop`). A standalone ngspice run is a different scenario from the report.
- [ ] `R_BOARD` in `run_power.py` and the constants in `vent_model.py` are copied by hand from the heat model. `test_heat.py` catches drift in the vent model; nothing checks `R_BOARD`.
- [ ] `tools/power_budget.py` uses its own efficiency estimates: boost 90 and 93 % (heat model: 91.4 % at the typical load) and charger 90 % (heat model: about 87 %). The charger figure affects only the PC-port rows of the charge-time table.
- [ ] `mechanical/antenna-mounts.json` holds positions from an early study and nothing reads it. Remove it or update it from the board.
- [ ] The L1 symbol uses the library symbol of the L2 part. Value and footprint are correct, so this is cosmetic.
- [ ] Data sheets are missing for L1, D3, D4, D5, BZ1 and the thermostat; nine PDFs have no text extraction. See [datasheets/README.md](../datasheets/README.md).
- [ ] The product name differs between places: "Flybee" in `specs.md`, "Airfrep" in the board title block, "airgrep" on the silkscreen. Pick one.
