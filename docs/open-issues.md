# Open issues

Open work only, ordered by what blocks the next step. The design itself is described in [specs.md](specs.md), [decisions.md](decisions.md), [mechanical.md](mechanical.md) and [interfaces.md](interfaces.md); closed items are in git.

The GNSS and ADSBee modules have arrived. The display is ordered. The carrier board is not.

## Before ordering the board

- [ ] **Reverse-polarity protection at J11.**
- [ ] **USB-C current detection, TI carrier.** Implement `CONTROL1.ENABLE`, CC advertisement handling and conservative fallback in firmware. Verify dead-battery attachment and current limiting with weak sources. 5 V only, no PD negotiation.
- [ ] **USB unplug, bench check.** Q12 holds U14 off while the POWER slider is on, so in normal operation the display runs from U12 only and an unplug causes no handover. C51 and C52 (2 x 22 µF on ESP_5V, behind JP1 at the display input) hold the feed when a handover does happen (slider switched with USB present). Bench: unplug and replug USB under load with the slider on, scope ESP_5V and RAIL_5V; ESP and ADSBee must not reset.
- [ ] **FLARM interference.** Measure receiver desense and converter coupling at 868 MHz. Use `hardware/sim/emi/results_ti.md` for model assumptions; calculated margins are not measured sensitivity.

## Assumptions to measure

The simulations rest on these. Each one changes a result if it is far off.

- [ ] **Converter noise in the receiver bands.** Measure the ADSBee noise floor with each converter on and off, then evaluate the optional R39/C36 snubber. Use actual results to decide on shielding or layout changes; candidate EMI reports do not describe the fitted board.
- [ ] **Charger and boost temperature** at the intended load, in the enclosure, with the heatsink on U4 (thermocouple on the sink base). Compare against `hardware/sim/heat/ti/results.md`.

## Bench tests at bring-up

- [ ] **SW4 system reset.** Implement and read back SFET_PRESENT at startup. With battery only and with USB connected, hold SW4 past the typical 10 s trigger and scope VSYS, both 5 V module rails and ESP_3V3. Verify they fall below each device's POR threshold before restart, at minimum load and with the real module capacitors. Test both POWER slider positions. Charger and MAX17048 are outside reset coverage. Confirm the stock 6 mm button fits the 6.5 x 4.5 mm footprint before fabrication.

- [ ] **Before first power-up.**
    - Inspect the U6 and U7 solder joints. The exposed pads have unfilled thermal vias next to the paste windows.
    - Confirm the pack lead polarity at J11 (pin 1, square pad, is plus).

- [ ] **Charger, carrier-ti-charger (BQ25798).**
    - Default mode after power-on: 1 A, STAT low, 12 h timer. Then I2C at 0x6B: ICHG 1.8 A, TREG 100 °C, WD_RST every 20 s; pull the host and confirm the fallback to 1 A after 40 s (WD_STAT).
    - D+/D- detection with a USB-A DCP, a PC port, a C-to-C cable from a PD adapter (expect "unknown adapter", 3 A, seted to 1.5 A by ILIM_HIZ) and an Apple adapter.
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

- [ ] **Physical controls.** Verify U13 address 0x20 alongside all other I2C devices, 400 kHz bus rise time, both slider positions, startup silence, debounce, last-known-state behavior on a disconnected expander, and Wi-Fi command acknowledgment. Check J10 pins 2-4 as display-control outputs with pin 1 GND. Measure added current; the budget is an estimate.

- [ ] **GNSS reception next to the charger.** Read the current U16-to-converter separation from the PCB. Compare reception with and without charging, and with SW5 on and off with each antenna type.


