# Technical data: airgrep carrier

## Product

Portable traffic-awareness receiver for gliders and general aviation. It receives ADS-B on 1090 MHz, takes its position from GNSS and shows traffic on a 1.47 inch touch display.

It is an awareness aid, not certified avionics. It must not be the primary means of collision avoidance or navigation. FLARM (868 MHz) reception remains a hardware-validation item. software progress is tracked in [firmware-plan.md](firmware-plan.md).

| Item                         | Value                                                                                                                                                                                         |
|------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Carrier board                | 105 x 95 x 1.6 mm, 4 layers, JLCPCB stackup in the KiCad board setup                                                                                                                          |
| Display module               | Seeed XIAO 1.47" IPS Touch Display (ESP32-S3), SKU 100069905                                                                                                                                  |
| ADS-B receiver               | ADSBee 1090U (RP2040, ESP32-S3 for Wi-Fi, TI CC1312 sub-GHz)                                                                                                                                  |
| GNSS receiver                | u-blox MAX-M10S on the carrier, U.FL antenna connector J7, active or passive antenna (SW5), CR2032 and pack backup                                                                            |
| Barometer                    | Bosch BMP580 on the carrier                                                                                                                                                                   |
| Audio                        | piezo buzzer, 4 kHz. maintained MUTE slider read by firmware                                                                                                                                  |
| Controls                     | independent POWER slider. MUTE and Wi-Fi sliders on TCA9534 I2C inputs. J10 provides two resets and backlight control for an optional external display. display touch and two display buttons |
| Indicators                   | one green LED, one red/green LED                                                                                                                                                              |
| Weight, enclosure dimensions | to be determined                                                                                                                                                                              |

Indicator states:

| LED       | State                  | Meaning                                                                       |
|-----------|------------------------|-------------------------------------------------------------------------------|
| green     | on                     | device running                                                                |
| red/green | red on, green on       | USB present, charging                                                         |
| red/green | red on, green blinking | USB present, charging fault                                                   |
| red/green | red on, green off      | USB present, charged or charging disabled. poll charger status to distinguish |
| red/green | off                    | no USB                                                                        |

## Radio equipment

| Function                                          | Module                  | Band                                                                  |
|---------------------------------------------------|-------------------------|-----------------------------------------------------------------------|
| ADS-B                                             | ADSBee 1090U            | 1090 MHz                                                              |
| Sub-GHz (UAT 978 MHz, 868 MHz)                    | ADSBee 1090U, TI CC1312 | 868 / 978 MHz                                                         |
| GNSS                                              | u-blox MAX-M10S         | 1561 to 1605 MHz (B1I 1561.098, L1 and E1 1575.42, L1OF 1598 to 1605) |
| Wi-Fi (traffic output to EFB apps, configuration) | ADSBee ESP32-S3         | 2.4 GHz, IEEE 802.11                                                  |
| Wi-Fi and Bluetooth LE 5.0                        | display ESP32-S3        | 2.4 GHz                                                               |

## Power supply and charging

Values follow the BQ25798 default profile and fitted hardware. Host implementation is described in [firmware.md](firmware.md#charger-host-control-bq25798-i2c-0x6b).

| Item                     | Value                                                                                                    |
|--------------------------|----------------------------------------------------------------------------------------------------------|
| Charging port            | USB-C, Hanbo MC-107DMS, sink only                                                                        |
| Input                    | 5 V only. No PD or Quick Charge negotiation                                                              |
| Input current            | Firmware applies the lower of CC and BC1.2. ILIM_HIZ sets about 1.5 A until a 3 A CC source is confirmed |
| Charge current           | 1 A to 1.8 A                                                                                             |
| Charge voltage           | 4.20 V                                                                                                   |
| Termination current      | 200 mA                                                                                                   |
| Fast-charge safety timer | 12 h default, host-programmable                                                                          |
| System priority          | System load first, battery supplements a weak source                                                     |
| Charger watchdog         | 40 s                                                                                                     |

### Charge temperature window

| Battery temperature | Charging                                   |
|---------------------|--------------------------------------------|
| below 2.7 °C        | suspended                                  |
| 2.7 to 8.5 °C       | 20 % of programmed current                 |
| 8.5 to 31.8 °C      | programmed current, 4.20 V                 |
| 31.8 to 42.6 °C     | programmed current, VREG reduced by 400 mV |
| above 42.6 °C       | suspended                                  |

### Protection

| Protection                             | Implementation                                                                        |
|----------------------------------------|---------------------------------------------------------------------------------------|
| Battery                                | F1 5 A fuse and the pack's own protection circuit                                     |
| Module overvoltage and reverse current | TPS259474L U11/U12/U14                                                                |
| Over-temperature                       | 60 °C ±5 K thermostat stops receiver power and charging. USB display power can remain |
| Low battery                            | TLV840 at 2.9 V SYS, followed by a latch                                              |
| Charger                                | hardware TS monitoring, thermal regulation and input-current control                  |

### Low-battery cut-off voltage

The supervisor measures SYS on the board, not the cell. Under load, SYS sits below the cell's open-circuit voltage by the drop across pack, harness, fuse and charger switch. So the cut-off seen at the cell depends on the load:

| Load on the 5 V rail | Battery current | Cell open-circuit voltage at cut-off                   | Basis                                         |
|----------------------|-----------------|--------------------------------------------------------|-----------------------------------------------|
| 0.8 A                | about 1.55 A    | about 3.2 V with a typical path                        | simulated, `hardware/sim/power/results_ti.md` |
| 0.8 A                | about 1.55 A    | up to about 3.3 V with an aged pack and a long harness | simulated, same file                          |
| typical, 1.22 W      | about 0.45 A    | about 3.0 V                                            | estimate, not simulated                       |

After a cut-off the device stays off until the POWER slider is cycled or USB is plugged in. The path resistance is an assumption to measure ([open-issues.md](open-issues.md)).

## Battery

User-replaceable lithium-ion polymer pack behind the rear cover, JST-PH 2-pin connector, polarity marked on the board.

## Operating conditions

| Item                          | Value                                                       | 
|-------------------------------|-------------------------------------------------------------|
| Operating temperature         | -10 to +50 °C                                               |
| Charging temperature          | see [Charge temperature window](#charge-temperature-window) |
| Storage, up to 6 months       | -20 to +35 °C                                               |
| Storage, longer than 3 months | dry and cool, cell at 3.75 to 3.95 V                        |
| Ingress protection            | none (no IP rating)                                         |

### Limits of the operating range

The operating range is a target bounded by component ratings, not an assembled-device qualification. The thermal model assumes a 115 x 105 x 40 mm case, four 8 cm² vents, 50 % open grilles and 35 mm vertical separation. The case is not designed yet.

[TI heat results](../hardware/sim/heat/ti/results.md) describe the recorded PCB snapshot. Charging while operating can reach the TS stop. high ambient temperature can reach the thermostat trip. Validate cooling, pack temperature and charging in the actual enclosure. Old MAX-board results do not describe the active board.

## Power, runtime and charging

This block is generated by `tools/power_budget.py` from `specs/power_params.json`. Edit the parameters and run the script again. Do not edit the block by hand.

<!-- BEGIN GENERATED: tools/power_budget.py -->

### Usage matrix and runtime

Runtime from a full charge to the supervisor cut-off, with 95 % of the rated capacity usable. The aged column assumes 80 % remaining capacity. The display's Wi-Fi is off in all cases. the display talks to the ADSBee over UART.

The boost efficiency is a budget estimate from the TI data sheet curve, derated for about 1 MHz. The board switches at about 720 kHz. this budget curve retains its conservative frequency allowance. Board-specific losses are in `hardware/sim/heat/ti/results.md`.

**Backlight 100 %**

| Scenario               | 5 V rail       | Boost eff. | Battery power            | 5000 mAh (new / aged) | 10000 mAh (new / aged) |
|------------------------|----------------|------------|--------------------------|-----------------------|------------------------|
| Wi-Fi off, GNSS fix    | 245 mA, 1.24 W | 90 %       | 1.38 W (374 mA at 3.7 V) | 12.7 h / 10.2 h       | 25.4 h / 20.3 h        |
| Wi-Fi off, GNSS no fix | 247 mA, 1.25 W | 90 %       | 1.40 W (377 mA at 3.7 V) | 12.6 h / 10.1 h       | 25.2 h / 20.2 h        |
| Wi-Fi on, GNSS fix     | 525 mA, 2.66 W | 93 %       | 2.91 W (785 mA at 3.7 V) | 6.0 h / 4.8 h         | 12.1 h / 9.7 h         |
| Wi-Fi on, GNSS no fix  | 527 mA, 2.67 W | 93 %       | 2.92 W (788 mA at 3.7 V) | 6.0 h / 4.8 h         | 12.1 h / 9.6 h         |

**Backlight 50 %**

| Scenario               | 5 V rail       | Boost eff. | Battery power            | 5000 mAh (new / aged) | 10000 mAh (new / aged) |
|------------------------|----------------|------------|--------------------------|-----------------------|------------------------|
| Wi-Fi off, GNSS fix    | 230 mA, 1.17 W | 90 %       | 1.30 W (352 mA at 3.7 V) | 13.5 h / 10.8 h       | 27.0 h / 21.6 h        |
| Wi-Fi off, GNSS no fix | 232 mA, 1.18 W | 90 %       | 1.31 W (355 mA at 3.7 V) | 13.4 h / 10.7 h       | 26.8 h / 21.4 h        |
| Wi-Fi on, GNSS fix     | 510 mA, 2.58 W | 93 %       | 2.82 W (763 mA at 3.7 V) | 6.2 h / 5.0 h         | 12.5 h / 10.0 h        |
| Wi-Fi on, GNSS no fix  | 512 mA, 2.59 W | 93 %       | 2.83 W (766 mA at 3.7 V) | 6.2 h / 5.0 h         | 12.4 h / 9.9 h         |

### Component breakdown (backlight 100 %, Wi-Fi off, GNSS fix)

| Load on the 5 V rail                                         | Current    | Source                                                                                                                                                                                                                                                                                                                                                                                                                                                                |
|--------------------------------------------------------------|------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| ADSBee 1090 core (RP2040, receiver, sub-GHz MCU)             | 120.0 mA   | ADSBee 1090 data sheet 2026-05-26, Quick Specs: 120 mA Wi-Fi disabled, about 400 mA Wi-Fi enabled at 5 V                                                                                                                                                                                                                                                                                                                                                              |
| Display: XIAO ESP32-S3 Plus MCU, Wi-Fi off                   | 45.0 mA    | estimate: ESP32-S3 dual core at 160 to 240 MHz without radio, 40 to 50 mA (Espressif ESP32-S3 data sheet, active mode). onboard LDO passes current 1:1 from 5 V                                                                                                                                                                                                                                                                                                       |
| Display: 1.47 in IPS panel and touch controller              | 8.0 mA     | estimate: LCD controller about 5 mA, capacitive touch about 2 to 3 mA. Seeed publishes no figure                                                                                                                                                                                                                                                                                                                                                                      |
| Display: backlight at 100 %                                  | 30.0 mA    | estimate: 1.47 in IPS backlight, 2 white LEDs at about 15 mA. Seeed publishes no figure                                                                                                                                                                                                                                                                                                                                                                               |
| Carrier: green power LED D3                                  | 6.4 mA     | (5.06 V - about 2.0 V LED forward voltage) / 470 Ω (R26)                                                                                                                                                                                                                                                                                                                                                                                                              |
| Carrier: BMP580, buzzer driver idle, I2C pull-ups            | 0.1 mA     | BMP580 normal mode below 0.01 mA. 74LVC1G17 below 0.01 mA. display I2C pull-ups about 0.05 mA                                                                                                                                                                                                                                                                                                                                                                         |
| Carrier: control inputs, both sliders low                    | 0.1 mA     | TCA9534: at most 4 uA standby at 3.6 V plus 2 x 33 uA in 100 kOhm pulls at 3.3 V. rounded to 0.1 mA for polling. Each additional grounded spare input adds 33 uA. Estimate, through display LDO.                                                                                                                                                                                                                                                                      |
| Carrier: buzzer, average                                     | 0.0 mA     | Carrier R28 1 kΩ and C66 1 µF AC-couple BZ1, with R87 10 kΩ bleed. Conservative 3.3 mA driven-current bound at 3.3 V, 1 % duty gives about 0.03 mA average. Estimate.                                                                                                                                                                                                                                                                                                 |
| GNSS: MAX-M10S, fix LED allowance, active antenna (tracking) | 35.8 mA    | u-blox MAX-M10S data sheet UBX-20035208 R02, Table 13, GPS+GAL+GLO: tracking 7.0 + 2.3 mA (VCC + V_IO), acquisition 9.0 + 2.3 mA at 3.0 V. Unmeasured average allowance for carrier PPS/fix LED D10 through R86 150 Ω and Q16. This is not a SparkFun breakout LED. verify pulse width, LED voltage and current on hardware.. Abracon AANE-AP-0164 LNA, 15 to 25 mA at 3.0 to 5.0 V, maximum used (datasheets/abracon-aane-ap-0164.pdf). Set 0 for a passive antenna. |
| **Total at 5.06 V**                                          | **245 mA** |                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |

Wi-Fi on raises the ADSBee from 120 mA to 400 mA. Without a fix the GNSS receiver stays in acquisition and draws 2.0 mA more, below 1 % of the total.

### Standby (POWER slider off, no USB)

Typical and maximum from the data sheets. the two resistor paths are computed from the cell voltage. The GNSS backup supply and unbudgeted board leakage are omitted. this is a partial budget. The pack's own protection IC and cell self-discharge come on top.

carrier-ti-charger, BQ25798:

| Source                                                  | Typ at 3.7 V | Max at 3.7 V |
|---------------------------------------------------------|--------------|--------------|
| Boost feedback divider R14 + R15 through the idle boost | 10.5 µA      | 11.2 µA      |
| R10 thermostat pull-down 1 MΩ                           | 3.7 µA       | 3.7 µA       |
| MAX17048 hibernate                                      | 3.0 µA       | 5.0 µA       |
| TPS61088 shutdown                                       | 1.0 µA       | 3.0 µA       |
| TPS22975 VBIAS shutdown                                 | 1.0 µA       | 2.3 µA       |
| TLV840 supervisor                                       | 0.1 µA       | 0.3 µA       |
| BQ25798 battery-only, ADC off                           | 17.0 µA      | 24.0 µA      |
| U14, Q11, Q12, TS divider, LED pull-ups                 | 0.0 µA       | 0.0 µA       |
| **Total**                                               | **36 µA**    | **50 µA**    |

| Cell voltage | 3.3 V      | 3.7 V      | 4.2 V      |
|--------------|------------|------------|------------|
| Typ / max    | 35 / 48 µA | 36 / 50 µA | 38 / 52 µA |

- 5000 mAh: 0.53 % per month typical, 0.72 % maximum for the budgeted paths.
- 10000 mAh: 0.26 % per month typical, 0.36 % maximum for the budgeted paths.

### Charging time from empty

The charge current is limited by the charger setting and by the input limit of the source. The charger powers the system first. only the rest charges the cell. The off column omits charger-mode display consumption, so those times are optimistic. "On" is the typical scenario: Wi-Fi off, GNSS fix, backlight 50 %, 1.30 W.

carrier-ti-charger, BQ25798: 1.8 A, charger efficiency 94 %.

| Source                                               | Input limit | 5000 mAh off / on                          | 10000 mAh off / on                          |
|------------------------------------------------------|-------------|--------------------------------------------|---------------------------------------------|
| USB-C charger 3 A                                    | 3.00 A      | 2 h 58 min (1.80 A) / 2 h 58 min (1.80 A)  | 5 h 55 min (1.80 A) / 5 h 55 min (1.80 A)   |
| USB-A charger, Apple 2.4 A signature (hardware set)  | 1.50 A      | 2 h 58 min (1.80 A) / 3 h 20 min (1.55 A)  | 5 h 55 min (1.80 A) / 6 h 40 min (1.55 A)   |
| USB-C charger 1.5 A, or USB-A charger with BC1.2 DCP | 1.50 A      | 2 h 58 min (1.80 A) / 3 h 20 min (1.55 A)  | 5 h 55 min (1.80 A) / 6 h 40 min (1.55 A)   |
| PC USB 2.0 port (SDP), or default-current USB-C      | 0.50 A      | 7 h 17 min (0.64 A) / 15 h 35 min (0.28 A) | 14 h 35 min (0.64 A) / 31 h 11 min (0.28 A) |

carrier-ti-charger, BQ25798 (default mode, 1 A, no host): 1 A, charger efficiency 94 %.

| Source                                               | Input limit | 5000 mAh off / on                          | 10000 mAh off / on                          |
|------------------------------------------------------|-------------|--------------------------------------------|---------------------------------------------|
| USB-C charger 3 A                                    | 3.00 A      | 4 h 51 min (1.00 A) / 4 h 51 min (1.00 A)  | 9 h 42 min (1.00 A) / 9 h 42 min (1.00 A)   |
| USB-A charger, Apple 2.4 A signature (hardware set)  | 1.50 A      | 4 h 51 min (1.00 A) / 4 h 51 min (1.00 A)  | 9 h 42 min (1.00 A) / 9 h 42 min (1.00 A)   |
| USB-C charger 1.5 A, or USB-A charger with BC1.2 DCP | 1.50 A      | 4 h 51 min (1.00 A) / 4 h 51 min (1.00 A)  | 9 h 42 min (1.00 A) / 9 h 42 min (1.00 A)   |
| PC USB 2.0 port (SDP), or default-current USB-C      | 0.50 A      | 7 h 17 min (0.64 A) / 15 h 35 min (0.28 A) | 14 h 35 min (0.64 A) / 31 h 11 min (0.28 A) |

With Wi-Fi on the system needs more than a PC port can supply. The battery then supplements the input and slowly discharges.

Parameters: `specs/power_params.json`. Regenerate with `python3 tools/power_budget.py`.
<!-- END GENERATED: tools/power_budget.py -->
