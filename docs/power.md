# Power architecture

Decided 24 Sep 2026 after design review. Replaces the LiPo Rider Plus (ETA9740) and the BQ25895 draft.

```
USB-C ── TVS ── BQ25606 VBUS (D+/D- BC1.2 detection, no HV handshake)
                    │
battery ── fuse ──┬── BQ25606 BAT ── SYS ── TPS61088 boost ── TPS22975 load switch ── 5 V rail
                  │                          EN ◄── SYS supervisor AND thermostat AND POWER slider
                  ├── MAX17048 gauge ── I2C ── display ESP32
                  ├── NTC ── BQ25606 TS
                  └── thermostat contact ── THERM_OK
5 V rail ── shunt JP_PWR_ESP ── display VBUS   (remove for display USB service)
5 V rail ── shunt JP_PWR_ADS ── ADSBee 5VIN    (remove for ADSBee USB service)
ADSBee 3V3 out ── GNSS 3.3 V
```

Battery path: one conversion (boost). USB path: two conversions (buck to SYS, boost to 5 V), about 80 %, heat only, does not affect runtime. Loads cannot run from SYS: ADSBee uses an AZ1117 LDO with 3.7 V minimum input.

## Charger: TI BQ25606 (JLC C374063, VQFN-24 4 × 4 mm)

Standalone, no I2C, no ESP involvement.

- Input limit from D+/D- detection: DCP (USB-A chargers) 2.4 A, SDP (PC port) 0.5 A, Apple/Samsung dividers 1 to 2.4 A, unknown source = ILIM resistor. No 9 V / 12 V requests.
- VINDPM: input current reduces when VBUS sags, weak sources are never collapsed.
- NVDC power path: system first, charge current = remainder, battery supplements a weak source.
- Reverse-blocking input FET. Input overvoltage cutoff at 14.3 V, absolute maximum 22 V, so a wrong 9 V or 12 V supply is bucked down, not a fault.
- TS pin, JEITA, 10 kΩ from REGN, no low-side resistor, NTC Variohm ENTC-G1B-10k3435-1 (Reichelt ENTC-10K3435-1, B25/85 3435 K ±1 %) taped to the pack. Charging suspended below 0.0 °C (worst −1.5 °C) and above 43.3 °C (worst 44.6 °C, pack limit 45 °C); half current below 6 °C; 4.05 V charge voltage above 30.8 °C. NTC unplugged reads as cold: no charging. Changed 25 Sep from 5.1k/30k (TI window, stopped at 61 °C, above the pack's 45 °C rating).
- CE active low, driven from THERM_OK through one NPN inverter.
- BATFET overload threshold 6.0 A. Worst-case battery current about 2.7 A (1.5 A load at 3.1 V).
- Resistor values (datasheet constants KILIM = 478 A·Ω, KICHG = 677 A·Ω, both ±5 %):

| Function | Resistor | Value | Result |
|---|---|---|---|
| ILIM, unknown source | R5 | 330 Ω (E24, Basic) | 1.45 A |
| ILIM with 3 A Type-C, R4 in parallel | R4 | 560 Ω (E24, Basic) | 2.30 A total |
| ICHG max charge current | R6 | 330 Ω (E24, Basic) | 2.05 A, 0.2 C; pack allows 5 A standard, 10 A max (FE1160100 datasheet). JST-PH lead is 2 A per contact. |
| TS pull-up | R8 (R9 removed) | 10 kΩ 1 % (E24, Basic) | with Variohm ENTC-G1B-10k3435-1: charge 0 to 43 °C nominal, hot stop never above 45 °C worst case |
| Boost feedback | R14 / R15 | 240 kΩ / 75 kΩ (E24) | 5.06 V with VREF 1.204 V |
| VSET | R7 | 100 kΩ to GND | 4.208 V (short = 4.352 V, 10 kΩ = 4.40 V; never fit those) |

Unknown-source ILIM: 1.45 A default (R5 330 Ω). A TUSB320 in GPIO mode (PORT = GND for UFP, ADDR open, VBUS_DET via 910 kΩ, TI asks for 900 kΩ) drives a 74LVC1G02 NOR gate; OUT1 = OUT2 = low means 3 A advertised, the gate turns on a 2N7002 that parallels R4, raising ILIM to 2.30 A. TUSB320 and gate run from a 3.3 V TPS70933 LDO on VBUS (30 V input, EN open); OUT pull-ups must stay at or below 3.6 V.

Why not the ETA9740 (Rider): fixed 3 A charge current, no input limit, no supplement mode, shared USB/output node, no enable or thermistor input. Why not the BQ25895: needs an I2C host for limits, and with D+/D- connected it requests 9 V from Quick Charge adapters at power-on.

## Input protection

The TPS259470 eFuse is removed; the charger already provides reverse blocking, overvoltage cutoff and current limit.

- TVS SMAJ12A on VBUS, unidirectional, cathode to VBUS (charger tolerates 22 V), plus four H5VLD3B 5 V ESD diodes (SOD-323, JLCPCB Preferred) on CC1, CC2, D+, D-. Replaced the two TPD2E2U06 (24 Sep) to avoid feeder fees.
- VBUS capacitance 2 µF (C1 at the connector, C5 at the charger; USB hot-plug limit 10 µF); PMID 10 µF is behind the reverse-blocking FET.
- CC1 and CC2: no external pull-downs. The TUSB320 presents Rd (5.1 kΩ ±20 %) on both CC pins even without VDD (datasheet 7.4.3, dead battery mode), so a Type-C source enables VBUS before the 3.3 V LDO is up.

## Battery pack (verified 24 Sep, `datasheets/AKKU_SOLD333290_DB-EN.pdf`)

Soldered 333290 = Fullest Energy FE1160100 Li-Po, 10 Ah, 3.7 V, 100 × 60 × 11.1 mm, JST-PH 2 mm lead, built-in PCM.

| Parameter | Pack | Design |
|---|---|---|
| Charge voltage | 4.25 V cut-off, 4.2 V standard | VSET 4.208 V |
| Charge current | 5 A standard, 10 A max | ICHG 2 A |
| Charging temperature | 0 to 45 °C | TS divider 0 to 45 °C, thermostat 60 °C |
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
                     THERM_OK ── Q2 2N7002 gate, drain = CE with 100 kΩ pull-up to REGN
                     THERM_OK ── R10 1 MΩ + C31 100 nF to GND
```

Closed: THERM_OK high, CE low, charging allowed, slider decides EN. Open: R10 pulls THERM_OK low, EN low, CE high, charging off. Changed 25 Sep from an NPN with 47 kΩ base resistor and 100 kΩ pull-down: that network drew about 110 µA from the pack whenever the thermostat was closed, even with the device off. The MOSFET gate draws nothing, R10 draws 4 µA, and C31 keeps the high-impedance thermostat cable from picking up noise.

Off-state battery draw (slider off, no USB): BQ25606 about 58 µA typical (85 µA max), boost feedback divider about 11 µA, gauge, load switch, boost shutdown and supervisor about 6 µA, R10 4 µA; about 80 µA total, 0.6 % of the 10 Ah pack per month. A P-FET battery disconnect was considered (25 Sep) and rejected: it saves about 75 µA but deadlocks recovery of a fully discharged pack, drops USB-only operation and adds an Extended part.

## Boost and switch

- TPS61088: SYS (3.1 to 4.4 V) to 5 V, sized for 1.5 A. Revision C values (sheet 5) are initial, loop not validated. MODE tied to GND (forced PWM, about 1 MHz via R16 150 kΩ to SW): the load never drops below about 0.8 A while running, so PFM would not save anything, and a fixed switching frequency keeps harmonics predictable next to the 1090 MHz and GNSS receivers. Current limit in this mode: about 1.03 MΩ·A / R17.
- TPS22975 load switch: true disconnect of the 5 V rail; a disabled TPS61088 still conducts input to output.
- Sequencing: load switch rise time (CT capacitor) set slower than the boost soft-start so rail inrush does not trip the boost current limit.
- POWER slider C&K JS102011SAQN (JLC C221660) carries control current only. Terminal 2 (middle pad) is the common, verified on the C&K drawing page 4.

## Gauge

MAX17048 on the carrier, VDD from battery, I2C to display TP15/TP16 (3.3 V bus shared with touch and IMU), address 0x36. Pull-ups on the display side.

## Indication

Two 3 mm through-hole LEDs next to the USB port, user-fitted. D5 green (Kingbright L-7104GD) from the 5 V rail through R26 470 Ω: running. D4 is a Kingbright L-115WEGW red/green LED, common cathode on the middle leg (LED_D3.0mm-3: 1 = red anode, 2 = cathode to GND, 3 = green anode). Both anodes are fed from REGN through 1 kΩ (R23 red, R31 green); REGN is only up while the USB input is valid, so a weak or sleeping source shows nothing. Red (charging): Q3 (2N7002) shorts the red anode while STAT is released; its gate is STAT through R32 1 MΩ, with R33 47 kΩ as the STAT pull-up to REGN. STAT low (charging) turns Q3 off and red lights. Green (charged): Q5 (MMBT3904, base from the red anode through R35 10 kΩ) shorts the green anode whenever red is lit; Q4 (2N7002, gate on CHG_CE_N) shorts it while the thermostat has charging disabled, so a hot battery does not show as charged. Result: red while charging, green when complete, dark while the thermostat is open, red and green alternating at 1 Hz on a charger fault (TS, timer, OVP). STAT only drives a gate and the 47 kΩ pull-up, about 0.1 mA. Changed 25 Sep from a common-anode design because 3 mm common-anode bicolour LEDs are not available. STAT and PG are not wired to the display: the display has only three free GPIOs (D11 buzzer, D12/D13 UART to the ADSBee). The ESP reads charging state from the MAX17048 charge-rate register.

## Feeder-fee policy (24 Sep)

Passive values are E24 values with JLCPCB Basic stock (330R, 560R, 5.1k, 30k, 240k, 75k, 160k, 910k; 1u 50V, 2.2u 25V, 4.7u 25V, 10u 25V 0805, 22u 25V 0805). Remaining extended parts: the eight ICs, sliders, buzzer, USB-C, inductor, fuse, about 14 fees per order.

## Removed from revision C

TPS259470 eFuse and its resistors, TUSB320 charge gate (SN74LVC1G08, NPN stages; its TPS70933 LDO returned 25 Sep as U1), BQ25895 and its I2C dependency. TUSB320 itself may return as an ILIM helper (open).

## Reference for re-entry

Revision C sheets 5, 6, 7, 8, 9 (boost, switch, modules, gauge, buzzer/baro) in `archive/rev-c-easyeda/revision-c/sheet-N.svg` are still valid. Sheets 1 to 4 are replaced by the BQ25606 circuit above.
