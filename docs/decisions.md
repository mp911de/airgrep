# Design decisions

One entry per decision, with its reason. The design is described in [power.md](power.md), [mechanical.md](mechanical.md) and [interfaces.md](interfaces.md). The sequence of changes is in git.

## Architecture

- A separate boost supplies the 5 V rail. The charger's reverse boost is not used. It drives the USB port, runs only without a USB source and is limited to 1.5 A. A boost on SYS keeps the rail regulated through plug and unplug.
- The boost is not bypassed on USB power. A power mux would save heat while charging. It would also add a switchover to validate and take the load out of the charger's input current limit.
- Each module feed has an LM73100 ideal diode, U11 and U12. A module powered from its own USB port then cannot feed the 5 V rail when its shunt is left in.

## Charger

- The charger is a MAX77757 without a host. It reads the CC lines and D+/D- itself and applies the lower limit without firmware. It also has JEITA and a power path with supplement mode.
- The ETA9740 of the LiPo Rider Plus was rejected. It has a fixed 3 A charge current, no input limit, no supplement mode, a shared USB and output node, and no enable or thermistor input.
- The BQ25895 was rejected. It needs an I2C host for its limits. With D+/D- connected it requests 9 V from Quick Charge adapters at power-on.
- The BQ25606 was rejected. It draws 2.4 A from any source with a DCP signature, whatever the Type-C advertisement says. Type-C gives the CC advertisement precedence over BC1.2.
- There is no Type-C controller and no USB-present 3.3 V rail. The charger handles CC itself, so the TUSB320, its gate logic and its LDO are not needed.
- The charge current is 1.8 A, about 0.18 C for the 10 Ah pack. Two Basic 6.2 kΩ resistors in series set it.
- Charging from a weak source may hit the safety timer. Accepted. There is no timer extension and no automatic retry.
- L1 is 1 µH, not the typical 0.47 µH. That halves the ripple, stays within the data sheet range and is well stocked.
- All bulk capacitors are X7R. The heat model puts the capacitors at U4 above 100 °C in a closed case. X5R is rated to 85 °C.
- BYP uses C6 10 µF 25 V and C33 22 µF 16 V, both X7R, matching MAX77757 Table 4. C33 is Taiyo Yuden EMK316BB7226ML-T (C385906), in 1206 to retain X7R and the required voltage rating. DC-bias derating remains an estimate.
- The SYS capacitors sit 4 to 10 mm from the pins. Accepted. The pin order and L1 leave no room closer in.
- U4 carries a heatsink. The package has no exposed pad. Without the sink, thermal regulation cuts the charge current to 1.2 to 1.5 A in a closed case while the device runs.

## Temperature sensing

- The pack thermistor is a Variohm ENTC-EB-100K4262-02 with one 154 kΩ bias resistor. The data sheet suggests a bias equal to the thermistor's 25 °C value. 154 kΩ places the hot cut-off at or below 45 °C over all tolerances.
- The cold cut-off lies below the pack's 0 °C rating. Accepted. The charger thresholds and the thermistor's B value fix the window width, and the current is already halved below about 5 °C.
- C35 puts 10 nF on THM. The bare thermistor leads pick up RF, and the thresholds are DC comparisons.
- Q2 is a MOSFET with R10 at 1 MΩ. A bipolar stage with a base resistor draws about 110 µA from the pack whenever the thermostat is closed, even with the device off.
- R12 is 100 kΩ, not 1 MΩ. Q2's maximum leakage of 1 to 5 µA into 1 MΩ lifts the Q6 gate to its threshold and blocks charging.
- R43 in the thermostat feed is 1 kΩ, not 10 kΩ. It limits a shorted lead to 4.2 mA. With 10 kΩ the Q8 gate of the undervoltage latch reaches only about 0.76 x VSYS. That is below the 2N7002's maximum threshold at the 2.9 V cut-off.
- An open thermostat lead blocks USB-only operation. Accepted. There is no interlock bypass.

## Battery protection

- Reverse-polarity protection at J3 is planned with two CSD25404Q3 back to back. It is not in the schematic or on the board yet. The pair loses 97 mW at 2 A. F1 stays, and there is no reversed-polarity LED. Detection and gate control are still to be designed, see [issue 0002](../issues/0002-battery-reverse-polarity-protection.md).
- There is no P-FET battery disconnect. It would save the off-state draw of about 50 µA. It would also deadlock recovery of a fully discharged pack, drop USB-only operation and add a part. [Issue 0001](../issues/0001-battery-disconnect-switch.md) proposes one for the next revision.
- The supervisor monitors SYS, not the battery. The device then runs from USB with an empty or missing pack. It is a TLV840 at 2.9 V. The TPS3839 at 3.0 V is not stocked at JLCPCB.
- A latch follows the supervisor. The load drop across pack, harness and switch exceeds the supervisor's hysteresis. Without the latch the system would restart about every 11 ms at the end of a discharge. A CT capacitor only stretches the off time.
- The slider or USB clears the latch. Q9 inhibits it while USB is present, so USB operation with an empty pack still works.
- F1 is 5 A, not 3 A or 7 A. A 3 A fuse carries about 2 A continuously. That is too close to the 1.9 A battery current at the 1 A peak. A 7 A fuse does little for the 2 A JST-PH lead.
- The battery connector is JST-PH. It matches the pack lead. The battery current stays below its 2 A rating up to the 1 A load.

## Input protection

- There is no eFuse at the input. The charger provides reverse blocking, over-voltage lockout and the current limit.
- An SMAJ12A protects VBUS. It covers ESD and surge below the charger's 16 V limit. A USB-C source stays at 5 V without PD, so the 12 V standoff is sufficient.
- Each USB line has its own H5VLD3B ESD diode. Single SOD-323 diodes from JLCPCB's Preferred stock avoid the feeder fee of an array.

## Boost

- The boost runs in forced PWM, with MODE at GND. A fixed switching frequency keeps harmonics predictable next to the 1090 MHz and GNSS receivers.
- The switching frequency is 720 kHz with R16 at 220 kΩ, not 1 MHz. Boost loss at 1 A drops by about 20 %. Ripple rises from 20 to 28 mV.
- R18 is 6.8 kΩ, not 10 kΩ. It brings the crossover inside the fSW/10 and RHPZ/5 guideline up to 1 A. The loop is slower, and the light-load phase margin drops from 72° to 60°.
- The snubber has footprints but no parts. With the loop inductance taken from the board, the EMI estimate puts the ring at about 80 MHz (`hardware/sim/emi/results.md`); the receiver bands see the edge tail, which the snubber barely changes. Whether it matters is a bench question.
- U6 has three unfilled vias in one row under its pad, with the paste beside them. That gives thermal vias without a paid fill process, and the paste windows keep clear of the drills.

## Board

- The board has four layers. A solid GND plane 0.2 mm below the top layer and power tracks on In2 lower the path resistances and spread the heat of U4 and U6.
- Passives come from JLCPCB Basic stock to save feeder fees. The exceptions are values that Basic stock does not have: R8 and the X7R capacitors.

## Indication

- Red comes from INOKB, green from STAT. STAT blinks at 1 Hz while charging and is solid low when done. A steady "charging" LED would need a blink filter.
- The status pins are not wired to the display. The display has no free GPIO. The ESP reads the charge state from the gauge.
- D4 is common cathode. The available red/green 3 mm LED is built that way, so P-FETs switch both anodes from VSYS.

## Mechanics and interfaces

- The enclosure has a 5 cm² lower inlet and a 5 cm² upper outlet. The heat model assumes a 50 % open grille and 35 mm vertical separation. These are design inputs, not measured cooling performance.
- All modules use one socket height, 8.5 mm. That means one part type for all seven sockets and one standoff length.
- All case and module holes are 2.7 mm for M2.5 hardware, on KiCad stock footprints. The module holes take M2.5 as delivered and are not enlarged for M3.
- The GNSS has two standoffs, the display one. The header rows carry the rest.
- The ADSBee holes follow the manufacturer's enclosure pattern. MH12 and MH14 use the 23.0 x 40.5 mm pattern of the enclosure STEP file, not the 22.6 x 40.4 mm photo estimate. The delivered module decides.
- A UART connects display and ADSBee. Traffic data then does not depend on Wi-Fi. The custom ADSBee UART console provides wired commands and replies at 115200 baud, 8E1, alongside CSBEE traffic.
- Maintained mute and Wi-Fi sliders use a TCA9534PWR input expander on the existing I2C bus. Its eight inputs meet the requirement for six while leaving six spare after the two sliders. It uses no extra ESP GPIO; INT is unused and firmware polls. TSSOP-16, low standby current and the available C783615 assembly listing favor it over PCF8574.
- Mute gates ESP buzzer PWM in firmware. The POWER slider remains independent and is the recovery control for a firmware hang. A failed expander read retains both last known states. Low battery does not override the Wi-Fi slider.
- All three sliders use K3-1280S-F1, C92658. The local footprint follows the manufacturer drawing, including its peg offsets; the generic supplier CAD is inconsistent and needs a sample/assembly-preview check before production.

## Charger variant: carrier-ti-charger with the BQ25798

- A second board variant replaces the MAX77757 with a TI BQ25798 (QFN-29 4x4, LCSC C2876593). The reasons: the MAX77757 regulates its junction at a fixed 130 °C and needs a heatsink to hold 1.8 A in a closed case, and nothing can read its state. The BQ25798 charges at a silicon default of 1 A with no host, the display's ESP32-S3 raises it to 1.8 A over I2C, and the watchdog (40 s default) drops it back to the default when the host stops talking. Its regulation set point is 60/80/100/120 °C and it reports die temperature, battery current and voltage through a 16-bit ADC.
- The earlier rejection of I2C chargers assumed no host. The display module is the host now; charging without it still works at 1 A.
- BC1.2 stays on chip (D+/D-). USB-C CC detection is not on the BQ25798: the carrier fits Rd 5.1 kΩ on CC1 and CC2 and the input limit comes from D+/D- (SDP 0.5 A, CDP 1.5 A, DCP 3.25 A, unknown adapter 3 A). A C-to-C cable from a PD adapter without D+/D- reads as unknown, so 3 A; the ILIM_HIZ divider (47 kΩ / 100 kΩ from REGN) caps the hardware input limit at about 2.8 to 3 A.
- Switching frequency 750 kHz (PROG 4.7 kΩ) with a 2.2 µH FXL0630-2R2-M: about 6 dB lower converter lines at 868 MHz than 1.5 MHz and the better efficiency curve, at the cost of sitting next to the boost's 720 kHz and a conducted-emission fundamental at 750 kHz where the CISPR limit is 56 dBµV (`hardware/sim/emi/results_ti.md`: 5 dB short with FB1 at its DC-bias inductance). The host can switch to 1.5 MHz (PWM_FREQ) and the bench decides.
- The thermostat drives the CE pin through Q2 and R12 (CE high disables charging). Q6, which pulled the MAX77757 THM pin low, is gone.
- The red LED shows USB present (Q10 from a 47 kΩ / 100 kΩ divider on USB_VBUS) because the BQ25798 has no input-OK pin. The green LED follows STAT (charging).
- The charger INT pin ends on TP8. The display module has no free GPIO (D11 to D13 carry the buzzer PWM and the UART), so firmware polls the charger every 1 to 2 s and pets the watchdog.
- Pack NTC stays the 100 kΩ Variohm. The TS divider 80.6 kΩ / 768 kΩ from REGN puts the chip's T1/T2/T3/T5 thresholds at 3.1 / 8.9 / 32.2 / 43.0 °C, so charging stops inside the pack's 0 to 45 °C rating in the worst case (1.1 to 44.2 °C with typical TS leakage), without the host. The first divider (39 kΩ / 150 kΩ) charged up to 58.7 °C.
- No ship FET: SDRV gets 1 nF, QON stays open. Ship mode through SDRV remains an option for issue 0001.
- Heat path: the same 10 x 10 mm aluminium sink on the package top as on the MAX board (RθJC(top) 20.9 K/W is published for the VQFN-HR). A bottom-side spreader plate was evaluated in the model and dropped: not an agreed design.
