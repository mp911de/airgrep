# Open issues

Open work only, ordered by what blocks the next step. The design itself is described in [power.md](power.md), [mechanical.md](mechanical.md) and [interfaces.md](interfaces.md); closed items are in git.

The GNSS and ADSBee modules have arrived. The display is ordered. The carrier board is not.

## Before ordering the board

- [ ] **Reverse-polarity protection at J3.** CSD25404Q3 (LCSC C2865523) selected; design and validate the bidirectional protection stage, including USB-powered reversed-pack insertion and depleted-pack recovery. Coordinate with the planned off-state battery disconnect. No indicator LED; see [issue 0002](../issues/0002-battery-reverse-polarity-protection.md).
- [ ] **USB-C current detection, TI carrier.** FUSB303BTMX (LCSC C895444), R80 and R81 are routed and synchronized between schematic and PCB. Implement `CONTROL1.ENABLE`, CC advertisement handling and conservative fallback in firmware. Verify dead-battery attachment and current limiting with weak sources. 5 V only, no PD negotiation.
-
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
- [ ] **Case venting.** Selected four 8 cm² vents, two lower inlets and two upper outlets, with 50 % open grilles and 35 mm vertical separation. The updated TI heat model passes the 55 °C pack check at sustained 1 A and 25 °C ambient (51.0 °C). Charging while running still reaches the charge-temperature stop; verify the actual enclosure. See [vent implementation](#enclosure) and `hardware/sim/heat/ti/results.md`.
- [ ] **carrier-ti-charger layout, optional improvements** from the data sheet review (hard rules are met, [power.md](power.md#layout-1)): widen the In2 SW runs to 100 mil or move L1 100 mil north; four VBUS vias at pins 2/3 instead of two; a 0.1 µF at U6 VIN (pin 9, schematic change); REGN trace 12 to 15 mil; trim the "L2 Boost" switch-node zone to an 80 mil band; U7 exposed pad has room for a second via.
- [x] **BOM consolidation** applied: C5 and C15 (2.2 µF) to the 100 nF 50 V and the 4.7 µF 16 V parts, R23/R31 to 470 Ω, R61 to 150 Ω, value strings unified per LCSC code. 49 to 46 part numbers. Kept: two inductors (L1 FXL0630, L2 CKST0603), C33 22 µF 1206. Critical values untouched.

- [ ] **FLARM interference estimate.** The TI EMI model bounds the boost at −115 dBm and the charger at −114 dBm (−109 dBm for a 1 µH, 1.5 MHz variant with 2 ns edges) in the 868 MHz receiver band. The limit is −105 dBm, the assumed receiver sensitivity and about the level of a 14 dBm FLARM aircraft 20 km away in free space; the earlier −121 dBm target (6 dB below the thermal floor) ignored cockpit electronics, which set the real floor. Check with near-field probing and receiver desense measurements; modeled bounds are not measured interference. See `hardware/sim/emi/results_ti.md`.

## Assumptions to measure

The simulations rest on these. Each one changes a result if it is far off.

- [ ] **Module input capacitance on the 5 V rail** (assumed 40 µF) and the modules' input voltage limits. The transient checks use a 4.75 to 5.25 V normal window; the display SGM6029 is rated 5.5 V operating, 6 V absolute, and the ADSBee schematic specifies 3.7 to 6.5 V. Verify the complete module input limits. The diode-inclusive load-step model leaves only 42 mV above the lower limit at the assumed high module-feed diode drop. Measure each branch's capacitance and load share as well as the voltage window; they set the load-step and start-up results.
- [ ] **Peak 5 V current**, in particular ADSBee Wi-Fi bursts (assumed 1.0 A peak, 1.5 A short-term headroom).
- [ ] **Battery pack DC resistance** near empty, new and aged, warm and cold (assumed 50 to 150 mΩ), and harness resistance (assumed 60 to 100 mΩ). They set the voltage at which the undervoltage latch trips.
- [ ] **Converter noise in the receiver bands.** `hardware/sim/emi/results_max.md` (and `results_ti.md`) puts the edge tails of the boost and the charger within a few dB of the ADS-B and FLARM noise floors, with assumed edges and switch capacitances. Measure the ADSBee noise floor (trigger level) with each converter on and off, then with the snubber R39/C36 fitted (start with 2.2 Ω and 1 nF). The snubber gains under 1 dB at 868 MHz (`emi/spectrum_flarm.pdf`). Moving the switching frequency does not help: U4 is fixed, and the U6 lines sweep 27 MHz with VIN. A shield can over U6/L2 and U4/L1 is the remaining fix. Deferred: it conflicts with the heat sinks. Decide after the heat problem is solved and the bench shows a loss.
- [x] **USB input ripple.** Closed. The EMI model's worst conducted margin is −5 dB only at an assumed 0.1 µH FB1 corner under 3 A; Murata publishes no DC-bias data for BLM31KN471SZ1L (SimSurfing gives 1.6 µH at 0 A, QNFA9149 rates 4 A at 85 °C). Accepted as unverified. If the bench shows ripple at J1, replace FB1 with the 2.2 µH FC-ALX4030D-2R2MT (+22 dB modelled) or raise C1 to 10 µF (+1.6 dB).
- [ ] **Charger and boost temperature** at the intended load, in the enclosure, with the heatsink on U4 (thermocouple on the sink base). Compare against `hardware/sim/heat/ti/results.md`.

## Bench tests at bring-up

- [ ] **SW4 system reset.** Implement and read back SFET_PRESENT at startup. With battery only and with USB connected, hold SW4 past the typical 10 s trigger and scope VSYS, both 5 V module rails and ESP_3V3. Verify they fall below each device's POR threshold before restart, at minimum load and with the real module capacitors. Test both POWER slider positions. Charger and MAX17048 are outside reset coverage. Confirm the stock 6 mm button fits the 6.5 x 4.5 mm footprint before fabrication.

- [ ] **Module assembly.** Verify socket mating, clearance under the ADSBee rear shield, access to JP2 and room for the ADSBee USB plug above the boost converter.
- [ ] **Before first power-up.**
  - Inspect the U6 and U7 solder joints. The exposed pads have unfilled thermal vias next to the paste windows.
  - Confirm the pack lead polarity at J3 (pin 1, square pad, is plus).
- [ ] **Charger, carrier-ti-charger (BQ25798).**
  - Default mode after power-on: 1 A, STAT low, 12 h timer. Then I2C at 0x6B: ICHG 1.8 A, TREG 100 °C, WD_RST every 20 s; pull the host and confirm the fallback to 1 A after 40 s (WD_STAT).
  - D+/D- detection with a USB-A DCP, a PC port, a C-to-C cable from a PD adapter (expect "unknown adapter", 3 A, clamped to 1.5 A by ILIM_HIZ) and an Apple adapter.
  - U15 at 0x21 with default, 1.5 A and 3 A Type-C sources: ENABLE startup, IINDPM follows the lower of CC and BC1.2, EN_EXTILIM clears only at 3 A, detach and replug restore the clamp.
  - Dead-battery attach to a Type-C source, U15 unpowered.
  - Weak source: a current-limited 5 V supply, VINDPM holds VBUS.
  - TDIE, IBAT and VBAT over the ADC against a thermocouple and a meter; the TREG loop holding 100 °C in the closed case.
  - TS transitions at J2 (3.1 / 8.9 / 32.2 / 43.0 °C nominal), NTC open and shorted, thermostat trip to CE.
  - Red LED with USB present, green LED on STAT.
  - Conducted emission at 750 kHz with the real adapter. 1.5 MHz needs L1 = 1 µH and is not an option on this board.
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
- [ ] **Physical controls.** Verify U13 address 0x20 alongside all other I2C devices, 400 kHz bus rise time, both slider positions, startup silence, debounce, last-known-state behavior on a disconnected expander, and Wi-Fi command acknowledgment. Check all six J12 inputs with contacts to GND. Measure added current; the budget is an estimate.
- [ ] **GNSS reception next to the charger.** U16 is 41 mm from L1, just past the 40 mm planning target ([mechanical.md](mechanical.md#placement)). Compare reception with and without charging, and with SW5 on and off with each antenna type.

## Firmware

- [ ] Charger mode and host control: see [firmware.md](firmware.md).

- [ ] FLARM 868 MHz decoding is not demonstrated in the ADSBee firmware.
- [ ] Display firmware: gauge readout, traffic input and buzzer PWM on D11; poll and debounce U13 at 0x20. Apply both sliders at startup, keep PWM silent until mute is read, and retain both last known states on read failure. Show the shared failure icon for expander, gauge or BMP faults.
- [ ] Deploy and bench-test the custom ADSBee UART console: 115200 8E1, CSBEE plus AT replies. Verify Wi-Fi control, CRC/parity error recovery and dense-traffic throughput with the display. Source support is implemented in the local ADSBee checkout.
- [ ] BMP580: keep the interrupt output disabled; the INT pin is tied to GND.
- [ ] Low state of charge: warning and optional backlight dimming. Do not override the Wi-Fi slider.

## Enclosure

The enclosure is not designed yet. The carrier does not depend on these, the finished device does.

- [ ] **Vents.** Implement four 8 cm² vents, two lower inlets and two upper outlets. The model assumes a 50 % open grille and 35 mm vertical separation. At 25 °C ambient, high-corner pack estimates are 45.7 °C at 0.8 A and 50.9 °C at sustained 1.0 A; both pass the 55 °C check. Charging while running still exceeds the charge-temperature stop. Verify airflow and temperatures in the actual enclosure.
- [ ] **Antenna parts.** Confirm the bulkhead cables from the BOM in the 6.5 mm holes MH6 to MH8, the coax bend radius and the routing.
- [ ] **Barometer vent and Wi-Fi antennas.** The case needs an opening for the BMP580 and no metal near the module antennas.

## Optional improvements

- [ ] **Lower battery path resistance** to delay the undervoltage cut-off: a connector with two contacts per pole or JST-XH, and AWG 22 leads. The same change settles the connector rating above.
- [ ] **0805 OVLO resistor layout.** Reposition and reroute R73/R75/R77 for their 0805 footprints. Current placement has DRC shorts, clearance, mask and courtyard violations. Refill zones, run `tools/check.sh`, then regenerate the geometry-dependent heat and EMI reports.
- [ ] **OVLO transient validation.** U11/U12 use basic 1 % 5.6 kΩ / 1.5 kΩ dividers to save feeder fees: trip 5.426–5.976 V, release 4.935–5.453 V. These bands overlap, so a part may release above another part's trip point. U14 on the USB side uses 4.7 kΩ 1 % over 1.24 kΩ 0.1 %: trip 5.562–5.969 V, inside the 5.5–6.0 V window between the USB maximum and the display's absolute maximum, release 5.059–5.447 V. A 6.0 V USB PD transient trips U14, and a source sitting above the release point keeps the display off until USB is reconnected. Measure fault overshoot, input discharge and recovery by cycling POWER or USB. Check U7 survival and module limits; static OVLO corners do not certify fault protection. See [power design](power.md#charger-variant-bq25798-carrier-ti-charger).
- [ ] **SMAJ10A instead of SMAJ12A** for D3 if 12 V adapters need not be tolerated. The charger locks out at 13.7 V and withstands 16 V.

## Housekeeping outside the docs

Small corrections in scripts, parameters and project files. None changes the board.

- [ ] The L1 symbol uses the library symbol of the L2 part. Value and footprint are correct, so this is cosmetic.
