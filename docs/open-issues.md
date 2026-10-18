# Open issues

Open work only, ordered by what blocks the next step. The design itself is described in [power.md](power.md), [mechanical.md](mechanical.md) and [interfaces.md](interfaces.md); closed items are in git.

The GNSS and ADSBee modules have arrived. The display is ordered. The carrier board is not.

## Before ordering the board

- [ ] **Reverse-polarity protection at J11.** CSD25404Q3 (LCSC C2865523) selected; design and validate the bidirectional protection stage, including USB-powered reversed-pack insertion and depleted-pack recovery. Coordinate with the planned off-state battery disconnect. No indicator LED; see [issue 0002](../issues/0002-battery-reverse-polarity-protection.md).
- [ ] **USB-C current detection, TI carrier.** Implement `CONTROL1.ENABLE`, CC advertisement handling and conservative fallback in firmware. Verify dead-battery attachment and current limiting with weak sources. 5 V only, no PD negotiation.
- [ ] **K3-1280S-F1 fit and assembly placement.** Compare a real C92658 F1 sample with the manufacturer-derived footprint, including its 0.90 mm peg holes and 3.075 mm peg-to-terminal-row offset. The supplier CAD mixes F2/K1 names and uses a different peg offset. The assigned 3D model is checked for visualization; physical fit remains unverified. Verify SW1-SW3 orientation and seating in the JLCPCB preview before ordering. Details in [mechanical.md](mechanical.md#control-sliders).
- [ ] **Spare control inputs.** J10 exposes P4-P6 only, three dry-contact inputs versus four required. Resolve the hardware gap or agree a requirement change before release.
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
- [ ] **C53 value (boost output HF bypass).** Compare fitted 100 nF with a 1 nF alternative using near-field probing at U6. The PDN estimate depends on assumed package ESL and omits plane inductance; it is not a reason to change the fitted value without measurement.
- [ ] **USB unplug, bench check.** Q12 holds U14 off while the POWER slider is on, so in normal operation the display runs from U12 only and an unplug causes no handover. C51 and C52 (2 x 22 µF on ESP_5V, behind JP1 at the display input) hold the feed when a handover does happen (slider switched with USB present). Bench: unplug and replug USB under load with the slider on, scope ESP_5V and RAIL_5V; ESP and ADSBee must not reset.
- [ ] **FLARM interference.** Measure receiver desense and converter coupling at 868 MHz. Use `hardware/sim/emi/results_ti.md` for model assumptions; calculated margins are not measured sensitivity.

## Assumptions to measure

The simulations rest on these. Each one changes a result if it is far off.

- [ ] **Module input capacitance on the 5 V rail** (assumed 40 µF) and the modules' input voltage limits. The transient checks use a 4.75 to 5.25 V normal window; the display SGM6029 is rated 5.5 V operating, 6 V absolute, and the ADSBee schematic specifies 3.7 to 6.5 V. Verify the complete module input limits. The diode-inclusive load-step model leaves only 42 mV above the lower limit at the assumed high module-feed diode drop. Measure each branch's capacitance and load share as well as the voltage window; they set the load-step and start-up results.
- [ ] **Peak 5 V current**, in particular ADSBee Wi-Fi bursts (assumed 1.0 A peak, 1.5 A short-term headroom).
- [ ] **Battery pack DC resistance** near empty, new and aged, warm and cold (assumed 50 to 150 mΩ), and harness resistance (assumed 60 to 100 mΩ). They set the voltage at which the undervoltage latch trips.
- [ ] **Converter noise in the receiver bands.** Measure the ADSBee noise floor with each converter on and off, then evaluate the optional R39/C36 snubber. Use actual results to decide on shielding or layout changes; candidate EMI reports do not describe the fitted board.
- [ ] **Charger and boost temperature** at the intended load, in the enclosure, with the heatsink on U4 (thermocouple on the sink base). Compare against `hardware/sim/heat/ti/results.md`.

## Bench tests at bring-up

- [ ] **SW4 system reset.** Implement and read back SFET_PRESENT at startup. With battery only and with USB connected, hold SW4 past the typical 10 s trigger and scope VSYS, both 5 V module rails and ESP_3V3. Verify they fall below each device's POR threshold before restart, at minimum load and with the real module capacitors. Test both POWER slider positions. Charger and MAX17048 are outside reset coverage. Confirm the stock 6 mm button fits the 6.5 x 4.5 mm footprint before fabrication.

- [ ] **Module assembly.** Verify socket mating, clearance under the ADSBee rear shield, access to JP3 and room for the ADSBee USB plug above the boost converter.
- [ ] **Before first power-up.**
  - Inspect the U6 and U7 solder joints. The exposed pads have unfilled thermal vias next to the paste windows.
  - Confirm the pack lead polarity at J11 (pin 1, square pad, is plus).
- [ ] **Charger, carrier-ti-charger (BQ25798).**
  - Default mode after power-on: 1 A, STAT low, 12 h timer. Then I2C at 0x6B: ICHG 1.8 A, TREG 100 °C, WD_RST every 20 s; pull the host and confirm the fallback to 1 A after 40 s (WD_STAT).
  - D+/D- detection with a USB-A DCP, a PC port, a C-to-C cable from a PD adapter (expect "unknown adapter", 3 A, clamped to 1.5 A by ILIM_HIZ) and an Apple adapter.
  - Dead-battery attach to a Type-C source, U15 unpowered.
  - Weak source: a current-limited 5 V supply, VINDPM holds VBUS.
  - TDIE, IBAT and VBAT over the ADC against a thermocouple and a meter; the TREG loop holding 100 °C in the closed case.
  - TS transitions at J12 (2.7 / 8.5 / 31.8 / 42.6 °C nominal), NTC open and shorted, thermostat trip to CE.
  - Red LED with USB present, green LED on STAT.
  - Conducted emission at 750 kHz with the real adapter. 1.5 MHz needs L1 = 1 µH and is not an option on this board.
- [ ] **Thermal cut-off.**
  - Sweep the resistance at J12 through all TS transitions.
  - NTC open and shorted.
  - Thermostat trip.
  - Switch off with USB present; no backfeed.
- [ ] **5 V rail.** Start-up, load steps between 0.1 and 0.8 A and between 0.8 and 1 A at the module terminals, boost ripple. Compare against `hardware/sim/power/results_ti.md`.
- [ ] **Undervoltage.** Discharge to cut-off under load. The latch must hold the system off while the battery voltage recovers. Clear it with the slider and with USB. Measure the latched current (about 60 µA expected).
- [ ] **Physical controls.** Verify U13 address 0x20 alongside all other I2C devices, 400 kHz bus rise time, both slider positions, startup silence, debounce, last-known-state behavior on a disconnected expander, and Wi-Fi command acknowledgment. Check J10 pins 3, 4 and 5 with contacts to GND. Measure added current; the budget is an estimate.
- [ ] **GNSS reception next to the charger.** Read the current U16-to-converter separation from the PCB. Compare reception with and without charging, and with SW5 on and off with each antenna type.

## Firmware

- [ ] Charger mode and host control: see [firmware.md](firmware.md).
- [ ] **ADSBee GNSS UART power sequencing.** `GNSSReceiver::Init()` in ADSBee `pico/application/peripherals/gnss/gnss_receiver.cc` switches GNSS power off, enables UART TX, then restores power; `SetEnable(false)` also leaves TX driven. Keep TX high impedance while GNSS V_IO is off, including shutdown, and enable UART only after the receiver supply is valid. Verify cold start, disable/re-enable and host reset. Backfeed or failed startup has not been measured.

- [ ] Verify FLARM 868 MHz reception on the ADSBee hardware; software progress is in [firmware-plan.md](firmware-plan.md).
- [ ] Display firmware: gauge readout, traffic input and buzzer PWM on display D11 or Plus GPIO8; poll and debounce U13 at 0x20. Apply both sliders at startup, keep PWM silent until mute is read, and retain both last known states on read failure. Show the shared failure icon for expander, gauge or BMP faults.
- [ ] Deploy and bench-test the custom ADSBee UART console: 115200 8E1, CSBEE plus AT replies. Verify Wi-Fi control, CRC/parity error recovery and dense-traffic throughput with the display. Source support is implemented in the local ADSBee checkout.
- [ ] BMP580: verify polled initialization with INT intentionally unconnected; configure INT_CONFIG.int_en=1 and int_od=0 per Bosch section 6.2.
- [ ] Low state of charge: warning and optional backlight dimming. Do not override the Wi-Fi slider.

## Enclosure

The enclosure is not designed yet. The carrier does not depend on these, the finished device does.

- [ ] **Antenna parts.** Confirm the bulkhead cables from the BOM in the 6.5 mm holes MH6 to MH8, the coax bend radius and the routing.
- [ ] **Barometer vent and Wi-Fi antennas.** The case needs an opening for the BMP580 and no metal near the module antennas.

## Optional improvements

- [ ] **Lower battery path resistance** to delay the undervoltage cut-off: a connector with two contacts per pole or JST-XH, and AWG 22 leads.
- [ ] **OVLO transient validation.** Measure module-feed fault overshoot, input discharge and recovery with POWER and USB cycling. Verify complete-module input limits; [static divider corners](power.md#module-feeds) do not establish transient protection.

## Housekeeping outside the docs

Small corrections in scripts, parameters and project files. None changes the board.

- [ ] The L1 symbol uses the library symbol of the L2 part. Value and footprint are correct, so this is cosmetic.
