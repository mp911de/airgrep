# Technical data: airgrep carrier

Headline numbers for the carrier board with its modules. Other documents link here instead of repeating them. The design behind the numbers is in [power.md](power.md), [mechanical.md](mechanical.md) and [interfaces.md](interfaces.md).

The sections follow what the EU rules ask for: radio, charging, battery, safety information. This is not a declaration of conformity.

Each number names its basis where it matters: *data sheet*, *simulated* (with the results file), *estimate* (needs a bench measurement) or *not verified*.

## Product

Portable traffic-awareness receiver for gliders and general aviation. It receives ADS-B on 1090 MHz, takes its position from GNSS and shows traffic on a 1.47 inch touch display.

It is an awareness aid, not certified avionics. It must not be the primary means of collision avoidance or navigation. The receiver firmware does not yet support FLARM (868 MHz) reception.

| Item | Value |
|---|---|
| Carrier board | 105 x 95 x 1.6 mm, 4 layers; stackup in [power.md](power.md#stackup) |
| Display module | Seeed XIAO 1.47" IPS Touch Display (ESP32-S3), SKU 100069905 |
| ADS-B receiver | ADSBee 1090U (RP2040, ESP32-S3 for Wi-Fi, TI CC1312 sub-GHz) |
| GNSS receiver | u-blox MAX-M10S on the carrier, U.FL antenna connector J13, active or passive antenna (SW5), CR2032 and pack backup |
| Barometer | Bosch BMP580 on the carrier |
| Audio | piezo buzzer, 4 kHz; maintained MUTE slider read by firmware |
| Controls | independent POWER slider; MUTE and Wi-Fi sliders on TCA9534 I2C inputs; six spare inputs on J12; display touch and two display buttons |
| Indicators | one green LED, one red/green LED |
| Weight, enclosure dimensions | to be determined |

Indicator states:

| LED | State | Meaning |
|---|---|---|
| green | on | device running |
| red/green | red on, green blinking | USB present, charging |
| red/green | red on, green on | USB present, charged |
| red/green | red on, green off | USB present, not charging (temperature, thermostat or timer) |
| red/green | off | no USB |

## Radio equipment

| Function | Module | Band | Direction | Basis |
|---|---|---|---|---|
| ADS-B | ADSBee 1090U | 1090 MHz | receive only | data sheet |
| Sub-GHz (UAT 978 MHz, 868 MHz) | ADSBee 1090U, TI CC1312 | 868 / 978 MHz | receive only in this product | the data sheet names a tunable sub-GHz transceiver for UAT with an RF in/out connector; 868 MHz use and "the firmware does not transmit" are not verified |
| GNSS | u-blox MAX-M10S | 1561 to 1605 MHz (B1I 1561.098, L1 and E1 1575.42, L1OF 1598 to 1605) | receive only | data sheet |
| Wi-Fi (traffic output to EFB apps, configuration) | ADSBee ESP32-S3 | 2.4 GHz, IEEE 802.11 | transmit and receive, switchable | data sheet; the b/g/n modes are not verified |
| Wi-Fi and Bluetooth LE 5.0 | display ESP32-S3 | 2.4 GHz | to be disabled by the display firmware in normal use | data sheet for the radio; the firmware is not written yet |

Not verified: that both ESP32-S3 modules are pre-certified parts. Maximum transmit power and the radio test reports (EN 300 328) have to come from the module vendors' certificates before sale. The combination still needs the product-level assessment.

## Power supply and charging

Values are from the MAX77757 data sheet unless marked. The design is in [power.md](power.md).

| Item | Value |
|---|---|
| Charging port | USB Type-C receptacle (Hanbo MC-107DMS), USB 2.0, sink only |
| Input voltage | 5 V |
| Input voltage, tolerated | 4.5 to 13.7 V operating, 16 V withstand; 12 V TVS diode on the input |
| Input current | the lower of the USB-C advertisement and BC1.2 detection, see the table below |
| Charging power | 2.5 to 15 W |
| Fast-charging protocols | none: no USB Power Delivery, no Quick Charge. The device never requests more than 5 V |
| Charge current | 1.8 A constant current (IFAST 12.4 kΩ) |
| Charge voltage | 4.20 V |
| Charge termination | 75 mA |
| Charge safety timer | 6 h |
| Operating while charging | supported. The charger supplies the system first and charges with the remainder |

Input current limit by source:

| Source | Limit |
|---|---|
| PC port, default-current USB-C | 0.5 A |
| 1.5 A USB-C source, USB-A charger with BC1.2 | 1.5 A |
| USB-A charger with Apple signature | 2.4 A |
| 3 A USB-C source | 3.0 A |

### Charge temperature window

Battery temperature, measured by the 100 kΩ NTC on the pack (JEITA). Simulated in `hardware/sim/thermal/results.md`.

| Battery temperature | Charging | Basis |
|---|---|---|
| below about -3 °C | off | nominal; -5.5 to -0.6 °C over tolerances |
| about -3 to 5 °C | half current | nominal |
| about 5 to 31 °C | full current, 4.20 V | nominal |
| about 31 to 42 °C | full current, 4.05 V | nominal |
| above about 42 °C | off | nominal; 39.4 to 45.0 °C over tolerances, so at most 45 °C |

### Protection

| Protection | Value | Basis |
|---|---|---|
| Battery fuse | 5 A (Littelfuse 0466005.NRHF) | data sheet |
| Input over-voltage lockout | 13.7 V | data sheet |
| Battery discharge cut-off in the charger | 6 A | data sheet |
| Over-temperature cut-off | thermostat on the battery, 60 °C ± 5 K. It switches the device off and blocks charging | data sheet |
| Low-battery cut-off | SYS below 2.9 V switches the 5 V rail off, see below | data sheet (TLV840) |
| Pack protection | disconnects at 2.5 V ± 0.1 V | cell data sheet |
| Other | ESD diodes on the USB lines, charger thermal foldback, load switch on the 5 V rail | |

### Low-battery cut-off voltage

The supervisor measures SYS on the board, not the cell. Under load, SYS sits below the cell's open-circuit voltage by the drop across pack, harness, fuse and charger switch. So the cut-off seen at the cell depends on the load:

| Load on the 5 V rail | Battery current | Cell open-circuit voltage at cut-off | Basis |
|---|---|---|---|
| 0.8 A | about 1.55 A | about 3.2 V with a typical path | simulated, `hardware/sim/power/results.md` |
| 0.8 A | about 1.55 A | up to about 3.3 V with an aged pack and a long harness | simulated, same file |
| typical, 1.22 W | about 0.45 A | about 3.0 V | estimate, not simulated |

After a cut-off the device stays off until the POWER slider is cycled or USB is plugged in. The path resistance is an assumption to measure ([open-issues.md](open-issues.md)).

## Battery

User-replaceable lithium-ion polymer pack behind the rear cover, JST-PH 2-pin connector, polarity marked on the board. Two packs are qualified. The values are from the pack datasheets.

| Item | Soldered 333288 | Soldered 333290 |
|---|---|---|
| Reichelt order code | AKKU SOLD333288 | AKKU SOLD333290 |
| Cell | FE105573 | FE1160100 |
| Nominal voltage | 3.7 V | 3.7 V |
| Rated capacity | 5000 mAh, 18.5 Wh | 10000 mAh, 37 Wh |
| Maximum charge current | 5 A | 10 A |
| Maximum discharge current | 5 A | 10 A |
| Charge temperature | 0 to 45 °C | 0 to 45 °C |
| Discharge temperature | -10 to 60 °C | -10 to 60 °C |
| Size | 73 x 55 x 10.1 mm | 100 x 60 x 11 mm |
| Built-in protection | over-charge, over-discharge, short circuit | same |
| Transport | UN38.3 tested (vendor report) | same |

The user tapes two temperature sensors to the pack:

| Sensor | Part | Connector |
|---|---|---|
| NTC | Variohm ENTC-EB-100K4262-02 (100 kΩ, B 4262 K, ±0.2 °C) | J2 |
| Thermostat | Limitor LIM-P-82-60 (60 °C ± 5 K, normally closed) | J4 |

## Operating conditions

| Item | Value | Basis |
|---|---|---|
| Operating temperature | -10 to +50 °C | cell discharge rating and display module; to verify, see the limits below |
| Charging temperature | see [Charge temperature window](#charge-temperature-window) | |
| Storage, up to 6 months | -20 to +35 °C | cell data sheet |
| Storage, longer than 3 months | dry and cool, cell at 3.75 to 3.95 V | cell data sheet |
| Ingress protection | none (no IP rating) | |

### Limits of the operating range

The -10 to +50 °C range holds for the electronics on an open bench. In a closed case the heat model shows tighter limits (simulated, `hardware/sim/heat/max/results.md`). The model assumes a 115 x 105 x 40 mm printed case with a 5 cm² inlet and a 5 cm² outlet, a 50 % open grille and 35 mm vertical vent separation. The real enclosure is not designed yet.

| Supply | Load | Ambient | Pack temperature | Consequence |
|---|---|---|---|---|
| battery | typical, 1.22 W | 25 °C | 33 °C | below the thermostat's opening range |
| battery | typical, 1.22 W | 50 °C | 58 °C | inside the thermostat's opening range (55 to 65 °C) |
| battery | 0.8 A | 25 °C | 50 to 51 °C | below the thermostat's lowest opening point |
| battery | 1.0 A | 25 °C | 57 to 58 °C | above the thermostat's lowest opening point |
| battery | 0.8 A | 50 °C | 76 to 77 °C | the thermostat opens and the device switches off |
| USB, 3 A source, charging | 0.8 A | 25 °C | 61 to 64 °C | exceeds the nominal 42.1 °C charge-stop threshold |
| USB, 3 A source, charging | device off | 25 °C | 34 to 36 °C | below the nominal charge-stop threshold |

The selected vents do not support full load at 50 °C ambient or continuous charging while running at 0.8 A and 25 °C ambient in this model. JEITA and thermostat shutdown are not simulated, so temperatures above their thresholds are hypothetical steady states. Enclosure validation remains in [open-issues.md](open-issues.md). On the open bench at 25 °C the charger delivers the full 1.8 A with the device running.

## Power, runtime and charging

This block is generated by `tools/power_budget.py` from `specs/power_params.json`. Edit the parameters and run the script again. Do not edit the block by hand.

<!-- BEGIN GENERATED: tools/power_budget.py -->
### Usage matrix and runtime

Runtime from a full charge to the supervisor cut-off, with 95 % of the rated capacity usable. The aged column assumes 80 % remaining capacity. The display's Wi-Fi is off in all cases; the display talks to the ADSBee over UART.

The boost efficiency is a budget estimate from the TI data sheet curve, derated for about 1 MHz. The heat model simulates the board at about 720 kHz and gives about 1 point more at the typical load (`hardware/sim/heat/max/results.md`).

**Backlight 100 %**

| Scenario | 5 V rail | Boost eff. | Battery power | 5000 mAh (new / aged) | 10000 mAh (new / aged) |
|---|---|---|---|---|---|
| Wi-Fi off, GNSS fix | 246 mA, 1.24 W | 90 % | 1.39 W (374 mA at 3.7 V) | 12.7 h / 10.2 h | 25.4 h / 20.3 h |
| Wi-Fi off, GNSS no fix | 248 mA, 1.25 W | 90 % | 1.40 W (377 mA at 3.7 V) | 12.6 h / 10.1 h | 25.2 h / 20.1 h |
| Wi-Fi on, GNSS fix | 526 mA, 2.66 W | 93 % | 2.91 W (786 mA at 3.7 V) | 6.0 h / 4.8 h | 12.1 h / 9.7 h |
| Wi-Fi on, GNSS no fix | 528 mA, 2.67 W | 93 % | 2.92 W (789 mA at 3.7 V) | 6.0 h / 4.8 h | 12.0 h / 9.6 h |

**Backlight 50 %**

| Scenario | 5 V rail | Boost eff. | Battery power | 5000 mAh (new / aged) | 10000 mAh (new / aged) |
|---|---|---|---|---|---|
| Wi-Fi off, GNSS fix | 231 mA, 1.17 W | 90 % | 1.30 W (352 mA at 3.7 V) | 13.5 h / 10.8 h | 27.0 h / 21.6 h |
| Wi-Fi off, GNSS no fix | 233 mA, 1.18 W | 90 % | 1.31 W (355 mA at 3.7 V) | 13.4 h / 10.7 h | 26.8 h / 21.4 h |
| Wi-Fi on, GNSS fix | 511 mA, 2.58 W | 93 % | 2.82 W (763 mA at 3.7 V) | 6.2 h / 5.0 h | 12.5 h / 10.0 h |
| Wi-Fi on, GNSS no fix | 513 mA, 2.59 W | 93 % | 2.83 W (766 mA at 3.7 V) | 6.2 h / 5.0 h | 12.4 h / 9.9 h |

### Component breakdown (backlight 100 %, Wi-Fi off, GNSS fix)

| Load on the 5 V rail | Current | Source |
|---|---|---|
| ADSBee 1090 core (RP2040, receiver, sub-GHz MCU) | 120.0 mA | ADSBee 1090 data sheet 2026-05-26, Quick Specs: 120 mA Wi-Fi disabled, about 400 mA Wi-Fi enabled at 5 V |
| Display: XIAO ESP32-S3 Plus MCU, Wi-Fi off | 45.0 mA | estimate: ESP32-S3 dual core at 160 to 240 MHz without radio, 40 to 50 mA (Espressif ESP32-S3 data sheet, active mode); onboard LDO passes current 1:1 from 5 V |
| Display: 1.47 in IPS panel and touch controller | 8.0 mA | estimate: LCD controller about 5 mA, capacitive touch about 2 to 3 mA; Seeed publishes no figure |
| Display: backlight at 100 % | 30.0 mA | estimate: 1.47 in IPS backlight, 2 white LEDs at about 15 mA; Seeed publishes no figure |
| Carrier: green power LED D5 | 6.4 mA | (5.06 V - about 2.0 V LED forward voltage) / 470 Ω (R26) |
| Carrier: BMP580, buzzer driver idle, I2C pull-ups | 0.1 mA | BMP580 normal mode below 0.01 mA; 74LVC1G17 below 0.01 mA; display I2C pull-ups about 0.05 mA |
| Carrier: control inputs, both sliders low | 0.1 mA | TCA9534: at most 4 uA standby at 3.6 V plus 2 x 33 uA in 100 kOhm pulls at 3.3 V; rounded to 0.1 mA for polling. Each additional grounded spare input adds 33 uA. Estimate, through display LDO. |
| Carrier: buzzer, average | 0.2 mA | PKMCS0909 driven from 3.3 V through 150 Ω: about 20 mA while beeping, assumed 1 % duty |
| GNSS: MAX-M10S, fix LED, active antenna (tracking) | 35.8 mA | u-blox MAX-M10S data sheet UBX-20035208 R02, Table 13, GPS+GAL+GLO: tracking 7.0 + 2.3 mA (VCC + V_IO), acquisition 9.0 + 2.3 mA at 3.0 V; Abracon AANE-AP-0164 LNA, 15 to 25 mA at 3.0 to 5.0 V, maximum used (datasheets/abracon-aane-ap-0164.pdf). Set 0 for a passive antenna. |
| **Total at 5.06 V** | **246 mA** | |

Wi-Fi on raises the ADSBee from 120 mA to 400 mA. Without a fix the GNSS receiver stays in acquisition and draws 2.0 mA more, below 1 % of the total.

### Standby (POWER slider off, no USB)

Typical and maximum from the data sheets; the two resistor paths are computed from the cell voltage. The pack's own protection IC and cell self-discharge come on top.

carrier-max-charger, MAX77757:

| Source | Typ at 3.7 V | Max at 3.7 V |
|---|---|---|
| Boost feedback divider R14 + R15 through the idle boost | 10.5 µA | 11.2 µA |
| R10 thermostat pull-down 1 MΩ | 3.7 µA | 3.7 µA |
| MAX17048 hibernate | 3.0 µA | 5.0 µA |
| TPS61088 shutdown | 1.0 µA | 3.0 µA |
| TPS22975 VBIAS shutdown | 1.0 µA | 2.3 µA |
| TLV840 supervisor | 0.1 µA | 0.3 µA |
| MAX77757 battery-only | 30.0 µA | 50.0 µA |
| **Total** | **49 µA** | **76 µA** |

| Cell voltage | 3.3 V | 3.7 V | 4.2 V |
|---|---|---|---|
| Typ / max | 48 / 74 µA | 49 / 76 µA | 51 / 78 µA |

- 5000 mAh: 0.72 % per month typical, 1.10 % maximum. A full pack would last about 12 years on the circuit alone.
- 10000 mAh: 0.36 % per month typical, 0.55 % maximum. A full pack would last about 23 years on the circuit alone.

carrier-ti-charger, BQ25798:

| Source | Typ at 3.7 V | Max at 3.7 V |
|---|---|---|
| Boost feedback divider R14 + R15 through the idle boost | 10.5 µA | 11.2 µA |
| R10 thermostat pull-down 1 MΩ | 3.7 µA | 3.7 µA |
| MAX17048 hibernate | 3.0 µA | 5.0 µA |
| TPS61088 shutdown | 1.0 µA | 3.0 µA |
| TPS22975 VBIAS shutdown | 1.0 µA | 2.3 µA |
| TLV840 supervisor | 0.1 µA | 0.3 µA |
| BQ25798 battery-only, ADC off | 17.0 µA | 24.0 µA |
| U14, Q11, Q12, TS divider, LED pull-ups | 0.0 µA | 0.0 µA |
| **Total** | **36 µA** | **50 µA** |

| Cell voltage | 3.3 V | 3.7 V | 4.2 V |
|---|---|---|---|
| Typ / max | 35 / 48 µA | 36 / 50 µA | 38 / 52 µA |

- 5000 mAh: 0.53 % per month typical, 0.72 % maximum. A full pack would last about 16 years on the circuit alone.
- 10000 mAh: 0.26 % per month typical, 0.36 % maximum. A full pack would last about 31 years on the circuit alone.

### Charging time from empty

The charge current is limited by the charger setting and by the input limit of the source. The charger powers the system first; only the rest charges the cell. "On" is the typical scenario: Wi-Fi off, GNSS fix, backlight 50 %, 1.30 W.

carrier-max-charger, MAX77757: 1.8 A, charger efficiency 90 %. 1.8 A from the IFAST resistors, no host.

| Source | Input limit | 5000 mAh off / on | 10000 mAh off / on |
|---|---|---|---|
| USB-C charger 3 A | 3.00 A | 2 h 58 min (1.80 A) / 2 h 58 min (1.80 A) | 5 h 55 min (1.80 A) / 5 h 55 min (1.80 A) |
| USB-A charger, Apple 2.4 A signature | 2.40 A | 2 h 58 min (1.80 A) / 2 h 58 min (1.80 A) | 5 h 55 min (1.80 A) / 5 h 55 min (1.80 A) |
| USB-C charger 1.5 A, or USB-A charger with BC1.2 DCP | 1.50 A | 2 h 58 min (1.80 A) / 3 h 29 min (1.47 A) | 5 h 55 min (1.80 A) / 6 h 58 min (1.47 A) |
| PC USB 2.0 port (SDP), or default-current USB-C | 0.50 A | 7 h 35 min (0.61 A) / 17 h 11 min (0.26 A) | 15 h 11 min (0.61 A) / 34 h 23 min (0.26 A) |

carrier-ti-charger, BQ25798: 1.8 A, charger efficiency 94 %. 1.8 A set by the ESP32-S3 over I2C, 1 A in default mode without a host.

| Source | Input limit | 5000 mAh off / on | 10000 mAh off / on |
|---|---|---|---|
| USB-C charger 3 A | 3.00 A | 2 h 58 min (1.80 A) / 2 h 58 min (1.80 A) | 5 h 55 min (1.80 A) / 5 h 55 min (1.80 A) |
| USB-A charger, Apple 2.4 A signature | 2.40 A | 2 h 58 min (1.80 A) / 2 h 58 min (1.80 A) | 5 h 55 min (1.80 A) / 5 h 55 min (1.80 A) |
| USB-C charger 1.5 A, or USB-A charger with BC1.2 DCP | 1.50 A | 2 h 58 min (1.80 A) / 3 h 20 min (1.55 A) | 5 h 55 min (1.80 A) / 6 h 40 min (1.55 A) |
| PC USB 2.0 port (SDP), or default-current USB-C | 0.50 A | 7 h 17 min (0.64 A) / 15 h 36 min (0.28 A) | 14 h 35 min (0.64 A) / 31 h 13 min (0.28 A) |

carrier-ti-charger, BQ25798 (default mode, 1 A, no host): 1 A, charger efficiency 94 %. 1.8 A set by the ESP32-S3 over I2C, 1 A in default mode without a host.

| Source | Input limit | 5000 mAh off / on | 10000 mAh off / on |
|---|---|---|---|
| USB-C charger 3 A | 3.00 A | 4 h 51 min (1.00 A) / 4 h 51 min (1.00 A) | 9 h 42 min (1.00 A) / 9 h 42 min (1.00 A) |
| USB-A charger, Apple 2.4 A signature | 2.40 A | 4 h 51 min (1.00 A) / 4 h 51 min (1.00 A) | 9 h 42 min (1.00 A) / 9 h 42 min (1.00 A) |
| USB-C charger 1.5 A, or USB-A charger with BC1.2 DCP | 1.50 A | 4 h 51 min (1.00 A) / 4 h 51 min (1.00 A) | 9 h 42 min (1.00 A) / 9 h 42 min (1.00 A) |
| PC USB 2.0 port (SDP), or default-current USB-C | 0.50 A | 7 h 17 min (0.64 A) / 15 h 36 min (0.28 A) | 14 h 35 min (0.64 A) / 31 h 13 min (0.28 A) |

With Wi-Fi on the system needs more than a PC port can supply. The battery then supplements the input and slowly discharges.

Parameters: `specs/power_params.json`. Regenerate with `python3 tools/power_budget.py`.
<!-- END GENERATED: tools/power_budget.py -->

### Accuracy of these figures

- The ADSBee currents are the vendor's quick-spec values. The Wi-Fi value is an average with the access point active. It varies with traffic and client count.
- The display figures (MCU, panel, backlight) are estimates. Seeed publishes no current for this board.
- The most useful measurement is the display alone at 5 V, then one full-system measurement per usage scenario.
- The runtime tables cover 0.23 A (Wi-Fi off) to 0.51 A (Wi-Fi on) at 5 V. They do not show runtime at the 0.8 A design load from [requirements.md](requirements.md).

## Compliance checklist (not a declaration)

What a CE-marked product of this kind has to address, for planning. The list is not verified against the legal texts or by a test house.

| Topic | EU act | Notes for this product |
|---|---|---|
| Radio, EMC and safety | Radio Equipment Directive 2014/53/EU | Wi-Fi transmitters make RED apply. Harmonised standards: EN 62368-1 (safety), EN 301 489-1/-17/-19 (EMC), EN 300 328 (2.4 GHz), EN 303 413 (GNSS receiver) |
| Cybersecurity of connected radio equipment | RED delegated act (EU) 2022/30, EN 18031-1 | applies because the device joins or opens Wi-Fi networks: no default passwords, secure updates, configuration protection |
| Common charger | Directive (EU) 2022/2380 (amends RED) | USB-C port: done. The packaging must state the charging power range (2.5 to 15 W) and that USB PD is not supported, with the pictogram |
| Batteries | Battery Regulation (EU) 2023/1542 | user-replaceable pack: done. Still needed: capacity and chemistry marking, collection symbol, safety and disposal information in the manual |
| Hazardous substances | RoHS 2011/65/EU | JLCPCB assembly with RoHS parts. Hand-fitted parts must be RoHS too |
| Disposal | WEEE 2012/19/EU | crossed-out bin symbol, producer registration in the sales country |
| Product safety and traceability | General Product Safety Regulation (EU) 2023/988 | manufacturer name and address, type and batch identification, warnings and instructions in the user's language |

## Safety information for users

- Charge at room temperature. The device does not charge below about -3 °C or above about 42 °C at the battery.
- Do not operate with a swollen, damaged or wet battery. Replace it only with one of the qualified packs and observe the polarity marking.
- Do not cover the device in direct sunlight. Above 60 °C at the battery the thermostat switches it off.
- The traffic display is a situational-awareness aid. Keep a visual lookout.
