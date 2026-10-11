# Open issues

Open work only, ordered by what blocks the next step. The design itself is described in [power.md](power.md), [mechanical.md](mechanical.md) and [interfaces.md](interfaces.md); closed items are in git.

The GNSS and ADSBee modules have arrived. The display is ordered. The carrier board is not.

## Before ordering the board

- [ ] **Reverse-polarity protection at J3.** CSD25404Q3 (LCSC C2865523) selected; design and validate the bidirectional protection stage, including USB-powered reversed-pack insertion and depleted-pack recovery. Coordinate with the planned off-state battery disconnect. No indicator LED; see [issue 0002](../issues/0002-battery-reverse-polarity-protection.md).
- [ ] **Silkscreen.** Labels on the top side:
  - Add "ESP 5V" at JP1.
  - Add "THERM" at J4, which is labelled on the bottom only.
- [ ] **Battery connector rating.** J3 is JST-PH, rated 2 A per contact. The battery current is about 1.9 A at the 1 A load peak and about 3 A at the 1.5 A headroom near empty, and F1 is 5 A. Accept this for short peaks or change the connector ([interfaces.md](interfaces.md#carrier-connectors)).
- [ ] **K3-1280S-F1 fit and assembly placement.** Compare a real C92658 F1 sample with the manufacturer-derived footprint, including its 0.90 mm peg holes and 3.075 mm peg-to-terminal-row offset. The supplier CAD mixes F2/K1 names and uses 3.406 mm; its 3D model is not trusted. Verify SW1-SW3 orientation and seating in the JLCPCB preview before ordering. Details in [physical controls](../workflows/physical-controls.md).
- [ ] **Display fit.**
  - The 8.5 mm socket stack against the controller and the microSD slot on the back of the display.
  - An M2.5 screw in the module hole at MH13.
  - One standoff plus four header groups against the touch force.
- [ ] **Production files.** Regenerate `jlcpcb/` of the board being ordered (`carrier-ti-charger/` is the active one, `carrier-max-charger/` the reference) from the final board:
  - Fill all zones, save, run DRC with schematic parity.
  - Export Gerbers, BOM and CPL.
  - Run `tools/kpy tools/fix_cpl.py` after every CPL export. It corrects rotation and position of the parts whose JLCPCB model origin differs from the footprint; see the script header.
  - Record the JLCPCB class (Basic or Extended) per part; the project does not hold it yet.
  - Select stackup JLC04161H-2116C in the order.
- [ ] **Insulator board.** Order `hardware/insulator/` together with the carrier ([mechanical.md](mechanical.md#insulator-board)).
- [ ] **C53 value (boost output HF bypass).** Fitted 100 nF 0603 next to C18 per the TPS61088 typical application. The EMC pre-check's PDN model (kicad-happy, SPICE) shows the 100 nF moves the bank's anti-resonance from 158 MHz (2.3 Ω) to 200 MHz (3.9 Ω); 1 nF (C1588, in the BOM) gives the lowest peak, 1.5 Ω at 126 MHz. The model uses fixed package ESL and no plane inductance. Decide on the bench with a near-field probe at U6; the footprint takes either value.
- [ ] **USB unplug, bench check.** Q12 holds U14 off while the POWER slider is on, so in normal operation the display runs from U12 only and an unplug causes no handover. C51 and C52 (2 x 22 µF on ESP_5V, behind JP1 at the display input) hold the feed when a handover does happen (slider switched with USB present). Bench: unplug and replug USB under load with the slider on, scope ESP_5V and RAIL_5V; ESP and ADSBee must not reset.
- [ ] **Case venting.** With the 10 x 10 mm sink and two 5 cm² openings (50 % grille, 35 mm apart) the closed case is short of air: the battery 1.0 A load case needs about 10 cm² per opening to keep the pack under the thermostat's 55 °C, and 1 A default charging at 25 °C needs about 25 cm² per opening to hold the pack under the 43 °C stop (1.8 A host mode about 45 cm²). Sweep: `hardware/sim/vent_model.py`, grid runs in the heat report. Decide vent size with the case design.
- [ ] **carrier-ti-charger layout, optional improvements** from the data sheet review (hard rules are met, [power.md](power.md#layout-1)): widen the In2 SW runs to 100 mil or move L1 100 mil north; four VBUS vias at pins 2/3 instead of two; a 0.1 µF at U6 VIN (pin 9, schematic change); REGN trace 12 to 15 mil; trim the "L2 Boost" switch-node zone to an 80 mil band; C49 (SDRV, no ship FET) blocks C47 from sitting at the BAT pins, move or drop it; U7 exposed pad has room for a second via.
- [x] **BOM consolidation** applied: C5 and C15 (2.2 µF) to the 100 nF 50 V and the 4.7 µF 16 V parts, R23/R31 to 470 Ω, R61 to 150 Ω, value strings unified per LCSC code. 49 to 46 part numbers. Kept: two inductors (L1 FXL0630, L2 CKST0603), C33 22 µF 1206. Critical values untouched.
- [ ] **carrier-ti-charger decisions.** Switching frequency 750 kHz (PROG 4.7 kΩ, L1 2.2 µH) against 1.5 MHz (3.0 kΩ, 1 µH), see `hardware/sim/emi/results_ti.md`. TS window: decided, 3 to 43 °C with 80.6 kΩ / 768 kΩ. INT on TP8 only, or on D11 if the buzzer PWM moves. JLCPCB stock of C2876593 (mirror showed 560, not verified on jlcpcb.com).

## Assumptions to measure

The simulations rest on these. Each one changes a result if it is far off.

- [ ] **Module input capacitance on the 5 V rail** (assumed 40 µF) and the modules' input voltage window (assumed 4.75 to 5.25 V, 5.5 V absolute). The diode-inclusive load-step model leaves only 42 mV above the lower limit at maximum LM73100 drop. Measure each branch's capacitance and load share as well as the voltage window; they set the load-step and start-up results.
- [ ] **Peak 5 V current**, in particular ADSBee Wi-Fi bursts (assumed 1.0 A peak, 1.5 A short-term headroom).
- [ ] **Battery pack DC resistance** near empty, new and aged, warm and cold (assumed 50 to 150 mΩ), and harness resistance (assumed 60 to 100 mΩ). They set the voltage at which the undervoltage latch trips.
- [ ] **Converter noise in the receiver bands.** `hardware/sim/emi/results_max.md` (and `results_ti.md`) puts the edge tails of the boost and the charger within a few dB of the ADS-B and FLARM noise floors, with assumed edges and switch capacitances. Measure the ADSBee noise floor (trigger level) with each converter on and off, then with the snubber R39/C36 fitted (start with 2.2 Ω and 1 nF). The snubber gains under 1 dB at 868 MHz (`emi/spectrum_flarm.pdf`). Moving the switching frequency does not help: U4 is fixed, and the U6 lines sweep 27 MHz with VIN. A shield can over U6/L2 and U4/L1 is the remaining fix. Deferred: it conflicts with the heat sinks. Decide after the heat problem is solved and the bench shows a loss.
- [ ] **USB input ripple.** FB1 and C1 at 4.7 µF filter the charger's 1.3 MHz ripple from the USB cable. Without them the model gives 74 dBµV into a LISN, 18 dB over the CISPR 32 class B limit for a DC port. With them the margin is 13 to 32 dB, depending on FB1's inductance at up to 3 A, which is estimated. Get Murata's DC-bias curve for BLM31KN471SZ1L, or measure the ripple at J1 with a 3 A source.
- [ ] **Charger and boost temperature** at the intended load, in the enclosure, with the heatsink on U4 (thermocouple on the sink base). Compare against `hardware/sim/heat/max/results.md`.

## Bench tests at bring-up

- [ ] **Module assembly.** Verify socket mating, clearance under the ADSBee rear shield, access to JP2 and room for the ADSBee USB plug above the boost converter.
- [ ] **Before first power-up.**
  - Inspect the U6 and U7 solder joints. The exposed pads have unfilled thermal vias next to the paste windows.
  - Confirm the pack lead polarity at J3 (pin 1, square pad, is plus).
  - Check the GNSS module's position in J5. The socket is not keyed; the silkscreen is the only guard.
- [ ] **Charger, carrier-ti-charger (BQ25798).**
  - Default mode after power-on: 1 A, STAT low, 12 h timer. Then I2C at 0x6B: ICHG 1.8 A, TREG 100 °C, WD_RST every 20 s; pull the host and confirm the fallback to 1 A after 40 s (WD_STAT).
  - D+/D- detection with a USB-A DCP, a PC port, a C-to-C cable from a PD adapter (expect "unknown adapter", 3 A, capped by ILIM_HIZ) and an Apple adapter.
  - TDIE, IBAT and VBAT over the ADC against a thermocouple and a meter; the TREG loop holding 100 °C in the closed case.
  - TS transitions at J2 (3.1 / 8.9 / 32.2 / 43.0 °C nominal), NTC open and shorted, thermostat trip to CE.
  - Red LED with USB present, green LED on STAT.
  - Conducted emission at 750 kHz and 1.5 MHz (PWM_FREQ) with the real adapter.
- [ ] **Charger, carrier-max-charger (MAX77757).**
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
- [ ] **5 V rail.** Start-up, load steps between 0.1 and 0.8 A and between 0.8 and 1 A at the module terminals, boost ripple. Compare against `hardware/sim/power/results_<charger>.md`.
- [ ] **Undervoltage.** Discharge to cut-off under load. The latch must hold the system off while the battery voltage recovers. Clear it with the slider and with USB. Measure the latched current (about 60 µA expected).
- [ ] **Physical controls.** Verify U13 address 0x20 alongside all other I2C devices, 100 kHz bus rise time, both slider positions, startup silence, debounce, last-known-state behavior on a disconnected expander, and Wi-Fi command acknowledgment. Check all six J12 inputs with contacts to GND. Measure added current; the budget is an estimate.
- [ ] **GNSS reception next to the charger.** L1 is closer to the GNSS module than the 40 mm planning target ([mechanical.md](mechanical.md#placement)). Compare reception with and without charging.

## Firmware

- [ ] Charger mode and host control: see [firmware.md](firmware.md).

- [ ] FLARM 868 MHz decoding is not demonstrated in the ADSBee firmware.
- [ ] Display firmware: gauge readout, traffic input and buzzer PWM on D11; poll and debounce U13 at 0x20. Apply both sliders at startup, keep PWM silent until mute is read, and retain both last known states on read failure. Show the shared failure icon for expander, gauge or BMP faults.
- [ ] Custom ADSBee firmware: add Wi-Fi command/response support to the existing COMMS_UART alongside traffic output. Stock AT intake on this port is not established.
- [ ] BMP581: keep the interrupt output disabled; the INT pin is tied to GND.
- [ ] Low state of charge: warning and optional backlight dimming. Do not override the Wi-Fi slider.

## Enclosure

The enclosure is not designed yet. The carrier does not depend on these, the finished device does.

- [ ] **Vents.** Implement a 5 cm² lower inlet and a 5 cm² upper outlet. The model assumes a 50 % open grille and 35 mm vertical separation. At 25 °C ambient, the high-corner pack estimate is 51.3 °C at 0.8 A but 58.3 °C at 1.0 A, so one heat check still fails. Charging while running also exceeds the pack's charge-temperature limit in the model. Verify airflow and temperatures in the actual enclosure; see [operating limits](specs.md#limits-of-the-operating-range).
- [ ] **Antenna parts.** Confirm the bulkhead cables from the BOM in the 6.5 mm holes MH6 to MH8, the coax bend radius and the routing.
- [ ] **Barometer vent and Wi-Fi antennas.** The case needs an opening for the BMP581 and no metal near the module antennas.

## Optional improvements

- [ ] **Lower battery path resistance** to delay the undervoltage cut-off: a connector with two contacts per pole or JST-XH, and AWG 22 leads. The same change settles the connector rating above.
- [ ] **Overvoltage lockout on U11 and U12 (rev B).** OVLO (pin 2) is tied to GND, so the overvoltage cut-off does nothing. If the boost feedback divider R14/R15 fails, the 5 V rail can rise toward 13 V and U7 and the modules see it. Fix: a divider from RAIL_5V on each OVLO pin, two resistors per chip.
- [ ] **SMAJ10A instead of SMAJ12A** for D3 if 12 V adapters need not be tolerated. The charger locks out at 13.7 V and withstands 16 V.

## Housekeeping outside the docs

Small corrections in scripts, parameters and project files. None changes the board.

- [ ] The L1 symbol uses the library symbol of the L2 part. Value and footprint are correct, so this is cosmetic.
