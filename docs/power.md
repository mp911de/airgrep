# Power architecture

Decided 24 Sep 2026 after design review. Replaces the LiPo Rider Plus (ETA9740) and the BQ25895 draft.

```
USB-C ── TVS ── MAX77757 CHGIN (CC and D+/D- detection on chip, no PD, no HV handshake)
                    │
battery ── fuse ──┬── MAX77757 BATT ── SYS ─ TPS61088 boost ── TPS22975 load switch ── 5 V rail
                  │                          EN ◄── SYS supervisor AND thermostat AND POWER slider
                  ├── MAX17048 gauge ── I2C ── display ESP32
                  ├── NTC ── MAX77757 THM
                  └── thermostat contact ── THERM_OK
5 V rail ── shunt JP_PWR_ESP ── display VBUS   (remove for display USB service)
5 V rail ── shunt JP_PWR_ADS ── ADSBee 5VIN    (remove for ADSBee USB service)
ADSBee 3V3 out ── GNSS 3.3 V
```

Battery path: one conversion (boost). USB path: two conversions (buck to SYS, boost to 5 V), about 80 %, heat only, does not affect runtime. Loads cannot run from SYS: ADSBee uses an AZ1117 LDO with 3.7 V minimum input.

## Charger: ADI MAX77757JEFG420 (JLC C7454852, FC2QFN-24 3 × 3 mm, 0.4 mm pitch)

Standalone, no I2C, no ESP involvement. Replaced the BQ25606 on 29 Sep. The BQ25606 applies its ILIM resistor only to sources it cannot identify and draws 2.4 A from any source with a DCP signature, whatever the Type-C advertisement says (BQ25606 datasheet, pin 8 and Table 9-2; TI E2E confirms). The USB Type-C precedence rule puts the CC advertisement above BC1.2. The MAX77757 reads the CC lines and D+/D- itself and applies the lower of the two results without firmware. TI has no standalone charger with CC input; the MAX77751 (no JEITA, 9 in stock) and MAX77757 (JEITA, 100 in stock) are the only ones found.

- Input limit: Type-C advertisement 0.5 / 1.5 / 3.0 A from CC; BC1.2 on D+/D-: SDP 0.5 A, CDP and DCP 1.5 A, Apple dividers 0.5 to 2.4 A, Samsung 2 A; unknown source or open D+/D- 3 A, always bounded by the CC result. Range 0.5 to 3.0 A. No 9 V / 12 V requests, no PD.
- Dead battery: the CC pins carry a voltage clamp that acts as Rd until the chip is up, then swap to the 5.1 kΩ resistor within 15 ms (tClampSwap), so a Type-C source enables VBUS with an empty pack.
- AICL: input current reduces when CHGIN sags to 4.5 V, weak sources are never collapsed.
- Smart power selector: system first, charge current = remainder, battery supplements a weak source (VSYS = VBAT − 70 mV in supplement mode). VSYSMIN 3.5 V.
- Input: 4.5 to 13.7 V operating (over-voltage lockout 13.7 V), 16 V absolute maximum. Lower than the BQ25606 (22 V). The SMAJ12A TVS stays (breakdown 13.3 V min, clamp 19.9 V at 20.8 A): it covers ESD and surge, and a USB-C source never delivers more than 5 V without a PD negotiation the device does not do (open issue 0b).
- Charge current: IFAST resistor R6 14 kΩ, 2.0 A (datasheet Table 3; 14 kΩ is an Extended part, C22803). Top-off 100 mA. Termination 4.20 V fixed by the variant, no VSET resistor. Trickle below 3.1 V at 300 mA, precharge below 2.5 V at 55 mA, prequalification timer 30 min, fast-charge timer 6 h, restart 100 mV below termination.
- THM with JEITA (J variant): NTC from THM to GND, bias R8 from PVL to THM. Thresholds are fixed ratios of PVL: cold 74 %, cool 65 %, warm 32.5 %, hot 23 %, charger disabled below 6 %. The hot/cold ratio of these thresholds needs an NTC with B ≈ 4350 K or more to land on the pack's 0 to 45 °C window; the 10 kΩ / 3435 K Variohm cannot (no bias value fits both ends). NTC changed 29 Sep to the TDK (EPCOS) B57164K0104 (100 kΩ ±10 %, R/T curve 2005, B25/100 4600 K ±3 %, lacquered disk, 5 mm lead spacing) with R8 130 kΩ (Extended C22795). From the datasheet R/T table: charging stopped below 0.0 °C, half current from 0 to 7.7 °C, termination 4.05 V above 34.7 °C, stopped above 45.2 °C (threshold tolerance ±1.5 % of PVL is about ±1 °C at each end). THM leakage (0.1 µA typical, 1 µA max) across the 30 kΩ node impedance at the hot end shifts the hot stop by 0.1 to 1.3 °C downward. NTC unplugged reads as cold: no charging.
- Charging is also stopped by pulling THM to GND (Q6, driven by the thermostat, see below). The MAX77757 has no CE pin.
- Battery overcurrent: the BATT-to-SYS switch opens above 6 A for 6 ms. Worst-case battery current about 2.7 A (1.5 A load at 3.1 V).
- Battery-only quiescent current 30 µA typical, 50 µA maximum (BQ25606: 58 µA typical).
- Status: INOKB (open drain, low while the input is valid and the buck runs) and STAT (open drain: 1 Hz toggle while charging, low in top-off and done, high impedance on no input, faults, timer expiry and while disabled). Both drive the indicator LEDs, see Indication.
- Capacitors per datasheet Table 4: CHGIN 2.2 µF (C5), BYP 10 µF + 22 µF (C6, C33), SYS 2 × 22 µF (C9, C10; 2 × 10 µF required), BATT 10 µF (C11), VDD 2.2 µF (C7), PVL 2.2 µF (C34), BST 100 nF (C8). All Basic 0603/0805 25 V. VDD and PVL are 1.8 V-class internal rails, only pull-ups may load them (R8 on PVL).
- Inductor L1 1 µH FXL0630-1R0-M (C167216, 7 × 6.6 mm, 12 A saturation, 7.4 mΩ). Datasheet: 0.47 to 1 µH, 5.5 A saturation or more; 1 µH halves the ripple of the 0.47 µH typical circuit and has 24k in stock. Kept on the IND-SMD_L7.4-W6.6 footprint (pads 2.5 × 3.3 mm at 6.6 mm against the vendor's 2.4 × 3.6 mm at 6.26 mm, within the terminal).
- Layout (datasheet Figure 13): CHGIN, BYP, SYS, VDD and BATT capacitors at the pins; PGND (pins 18, 19) returns through the SYS and BYP capacitor ground to the plane, GND (9) straight to the plane. No exposed pad; the nine thermal vias of the BQ25606 are gone. The fine-pitch rule area FINE_U4 already relaxes the spacing rules to 5 mil around U4.
- Resistor values:

| Function | Resistor | Value | Result |
|---|---|---|---|
| IFAST fast-charge current | R6 | 14 kΩ 1 % (Extended, C22803) | 2.0 A, 0.2 C; top-off 100 mA. Pack allows 5 A standard, JST-PH lead 2 A per contact. |
| THM bias from PVL | R8 | 130 kΩ 1 % (E24, Extended C22795) | 0.0 / 7.7 / 34.7 / 45.2 °C with the TDK B57164K0104 100 kΩ NTC (see above) |
| STAT pull-up | R33 | 47 kΩ to VSYS | gate of Q3 (green LED) |
| INOKB pull-up | R36 | 100 kΩ to VSYS | gate of Q4 (red LED) |
| CHG_DIS pull-down | R12 | 1 MΩ | thermostat charge disable |
| LED series | R23 / R31 | 560 Ω from VSYS | 2.7 to 4.3 mA |
| Boost feedback | R14 / R15 | 240 kΩ / 75 kΩ (E24) | 5.06 V with VREF 1.204 V |

The USB input sheet now holds only the connector, the SMAJ12A, the four ESD diodes and C1. The TUSB320 with its 74LVC1G02 gate, Q1, R1 to R5, R7, C2 to C4, C32 and the TPS70933 LDO (U1) are removed; nothing on the carrier needs a USB-present 3.3 V rail any more.

Why not the ETA9740 (Rider): fixed 3 A charge current, no input limit, no supplement mode, shared USB/output node, no enable or thermistor input. Why not the BQ25895: needs an I2C host for limits, and with D+/D- connected it requests 9 V from Quick Charge adapters at power-on. Why not keep the BQ25606: see the charger section, its input limit ignores the Type-C advertisement whenever D+/D- carry a signature.

## Input protection

The TPS259470 eFuse is removed; the charger already provides reverse blocking, overvoltage cutoff and current limit.

- TVS SMAJ12A on VBUS, unidirectional, cathode to VBUS (charger withstands 16 V, lockout at 13.7 V, open issue 0b), plus four H5VLD3B 5 V ESD diodes (SOD-323, JLCPCB Preferred) on CC1, CC2, D+, D-. Replaced the two TPD2E2U06 (24 Sep) to avoid feeder fees.
- VBUS capacitance 3.2 µF (C1 1 µF at the connector, C5 2.2 µF at the charger; USB hot-plug limit 10 µF); BYP 32 µF (C6, C33) is behind the charger input switch.
- CC1 and CC2: no external pull-downs. The MAX77757 presents Rd (5.1 kΩ ±10 %) on both CC pins; while it is unpowered a clamp on the CC pins stands in for Rd and is swapped for the resistor within 15 ms of power-up (tClampSwap), so a Type-C source enables VBUS with an empty pack.

## Battery pack (verified 24 Sep, `datasheets/AKKU_SOLD333290_DB-EN.pdf`)

Soldered 333290 = Fullest Energy FE1160100 Li-Po, 10 Ah, 3.7 V, 100 × 60 × 11.1 mm, JST-PH 2 mm lead, built-in PCM.

| Parameter | Pack | Design |
|---|---|---|
| Charge voltage | 4.25 V cut-off, 4.2 V standard | MAX77757 JEFG420 variant, 4.20 V fixed |
| Charge current | 5 A standard, 10 A max | IFAST 2.0 A |
| Charging temperature | 0 to 45 °C | JEITA 0.0 to 45.2 °C (TDK 100 kΩ NTC, R8 130 kΩ), thermostat 60 °C |
| Discharge current | 5 A standard, 10 A max | 2.7 A worst case |
| Discharge cut-off | 2.5 V (PCM 2.5 or 3.0 V, variant dependent) | supervisor 3.0 V |
| Discharge temperature | −10 to 60 °C | thermostat 60 °C |

## Battery protection

- J_BAT is JST-PH 2 mm (B2B-PH-K), matching the pack lead, 2 A per contact. Fuse F1 3 A SMD 1206; the pack's own PCM is the second line.
- Undervoltage: supervisor TLV840CADL29 (LCSC C3686404, 2.9 V threshold, open-drain, CT open) monitoring SYS pulls PWR_EN low. Decided 24 Sep; TPS3839 3.0 V is not stocked at JLCPCB. Monitoring SYS instead of BAT lets the device run from USB with an empty or missing battery, because the charger holds SYS at 3.5 V or more when USB is present.

## Thermal protection

Thermostat: Limitor LIM-P-82-60-5-100-10A (Reichelt LIM-P-82-60), normally closed, auto reset, 60 °C ±5 K, 14.5 × 8 × 3.6 mm, stranded leads, taped to the pack. User-made harness to J4 (JST-PH PHR-2, SPH-002T-P0.5S).

```
SYS ──[thermostat]── THERM_OK ──[slider]── EN (boost + load switch, via supervisor)
                     THERM_OK ── Q2 AO3401A gate (source VSYS), drain = CHG_DIS, R12 1 MΩ to GND
                     CHG_DIS ── Q6 2N7002 gate, drain = charger THM (pulled to GND = charging off)
                     THERM_OK ── R10 1 MΩ + C31 100 nF to GND
```

Closed: THERM_OK = VSYS, Q2 off, CHG_DIS low, Q6 off, THM free, charging allowed, slider decides EN. Open: R10 pulls THERM_OK low, EN low, Q2 on, CHG_DIS high, Q6 pulls THM to GND, charger disabled (below 6 % of PVL). Nothing flows through Q2 and R12 while the thermostat is closed, 4 µA through R12 while it is open. Changed 29 Sep from the BQ25606 CE input (Q2 as N-FET with a 100 kΩ pull-up to REGN): the MAX77757 has no CE pin and is disabled by pulling THM low, which needs the opposite polarity. Changed 25 Sep from an NPN with 47 kΩ base resistor and 100 kΩ pull-down: that network drew about 110 µA from the pack whenever the thermostat was closed, even with the device off. The MOSFET gate draws nothing, R10 draws 4 µA, and C31 keeps the high-impedance thermostat cable from picking up noise.

Off-state battery draw (slider off, no USB): MAX77757 about 30 µA typical (50 µA max), boost feedback divider about 11 µA, gauge, load switch, boost shutdown and supervisor about 6 µA, R10 4 µA; about 51 µA total, 0.4 % of the 10 Ah pack per month. A P-FET battery disconnect was considered (25 Sep) and rejected: it saves about 75 µA but deadlocks recovery of a fully discharged pack, drops USB-only operation and adds an Extended part.

## Boost and switch

- TPS61088: SYS (3.1 to 4.4 V) to 5 V, sized for 1.5 A. Revision C values (sheet 5) are initial, loop not validated. MODE tied to GND (forced PWM, about 1 MHz via R16 150 kΩ to SW): the load never drops below about 0.8 A while running, so PFM would not save anything, and a fixed switching frequency keeps harmonics predictable next to the 1090 MHz and GNSS receivers. Current limit in this mode: about 1.03 MΩ·A / R17.
- TPS22975 load switch: true disconnect of the 5 V rail; a disabled TPS61088 still conducts input to output.
- Sequencing: load switch rise time (CT capacitor) set slower than the boost soft-start so rail inrush does not trip the boost current limit.
- POWER slider C&K JS102011SAQN (JLC C221660) carries control current only. Terminal 2 (middle pad) is the common, verified on the C&K drawing page 4.

## Gauge

MAX17048 on the carrier, VDD from battery, I2C to display TP15/TP16 (3.3 V bus shared with touch and IMU), address 0x36. Pull-ups on the display side.

## Indication

Two 3 mm through-hole LEDs next to the USB port, user-fitted. D5 green (Kingbright L-7104GD) from the 5 V rail through R26 470 Ω: running. D4 is a Kingbright L-115WEGW red/green LED, common cathode on the middle leg (LED_D3.0mm-3: 1 = red anode, 2 = cathode to GND, 3 = green anode). Both anodes are switched from VSYS by P-channel MOSFETs (AO3401A) through 560 Ω (R23 red, R31 green). The charger's open-drain outputs drive the gates: Q4 from INOKB (pull-up R36 100 kΩ), Q3 from STAT (pull-up R33 47 kΩ). With no USB source both outputs are high impedance, both gates sit at VSYS, both FETs are off, nothing flows. Red (Q4): on while the charger input is valid and the buck runs. Green (Q3): blinks at 1 Hz while charging (trickle, precharge, fast charge), solid from top-off on, off on a fault, on timer expiry and while the thermostat holds THM low. Result: red plus blinking green = charging, red plus green = charged, red alone = USB present but not charging (temperature, thermostat, timer), dark = no USB. Changed 29 Sep from the BQ25606 STAT/REGN logic (red charging, green complete, alternate blink on fault; Q5, R32, R35 removed): the MAX77757 STAT pin encodes charging as the blink and done as solid low, which cannot be turned into a steady red without a blink filter. STAT and INOKB are not wired to the display: the display has only three free GPIOs (D11 buzzer, D12/D13 UART to the ADSBee). The ESP reads charging state from the MAX17048 charge-rate register.

## Feeder-fee policy (24 Sep)

Passive values are E24 values with JLCPCB Basic stock (150R, 470R, 560R, 1k, 4.7k, 10k, 47k, 75k, 100k, 150k, 160k, 240k, 1M; 100n, 1u 50V, 2.2u 25V, 10u 25V 0805, 22u 25V 0805). Remaining extended parts: the seven ICs (MAX77757, TPS61088, TPS22975, MAX17048, TLV840, SN74LVC1G17, BMP581), sliders, buzzer, USB-C, the two inductors, fuse, about 14 fees per order. The 29 Sep charger change removed four extended ICs (BQ25606, TUSB320, SN74LVC1G02, TPS70933) and the 910 kΩ resistor, added the MAX77757 and the 1 µH inductor, and uses Basic passives except the 14 kΩ IFAST and 130 kΩ THM bias resistors (47 kΩ, 560 Ω, 100 kΩ, 1 MΩ, 2.2 µF, 22 µF, 100 nF) and the Basic AO3401A.

## Removed from revision C

TPS259470 eFuse and its resistors, TUSB320 charge gate (SN74LVC1G08, NPN stages; its TPS70933 LDO returned 25 Sep as U1), BQ25895 and its I2C dependency. TUSB320 and gate returned on 25 Sep as an ILIM helper for the BQ25606 and went again on 29 Sep with the BQ25606 and the TPS70933: the MAX77757 reads CC itself.

## Reference for re-entry

Revision C sheets 5, 6, 7, 8, 9 (boost, switch, modules, gauge, buzzer/baro) in `archive/rev-c-easyeda/revision-c/sheet-N.svg` are still valid. Sheets 1 to 4 are replaced by the MAX77757 circuit above.
