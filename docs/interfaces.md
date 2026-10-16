# Module interfaces

Connectors on the carrier and what each pin carries. Positions and mounting are in [mechanical.md](mechanical.md), the hand-fitted parts in [bom-hand-sourced.md](bom-hand-sourced.md).

All module sockets are 2.54 mm single-row female headers. Their height is given in [mechanical.md](mechanical.md#assembly-split).

## Carrier connectors

| Ref | Function | Part | Fitted by |
|---|---|---|---|
| J1 | USB-C charging input | Hanbo MC-107DMS, 16P USB 2.0 (LCSC C3039295) | JLCPCB |
| J2 | pack NTC | JST B2B-PH-K-S, 2-pin | hand |
| J3 | battery | JST B2B-PH-K-S, 2-pin, pin 1 is + | hand |
| J4 | pack thermostat | JST B2B-PH-K-S, 2-pin | hand |
| J6, J7, J8, J9 | display sockets | 1x8, 1x7, 1x4, 1x2 | hand |
| J10, J11 | ADSBee sockets | 1x13, 1x5 | hand |
| JP1 | 5 V to the display | 1x2 pin header with jumper | hand |
| JP2 | 5 V to the ADSBee | 1x2 pin header with jumper | hand |
| SW1 | POWER slider | HRO K3-1280S-F1 (LCSC C92658) | JLCPCB |
| SW2 | MUTE slider | HRO K3-1280S-F1 (LCSC C92658) | JLCPCB |
| SW3 | Wi-Fi slider | HRO K3-1280S-F1 (LCSC C92658) | JLCPCB |
| J12 | six spare control inputs and two grounds | 2x4, 2.54 mm pin header | hand |
| BZ1 | piezo buzzer | Murata PKMCS0909E4000-R1 (LCSC C910763) | JLCPCB |

Notes:

- J3 carries the full battery current. JST rates the PH series at 2 A per contact with AWG 24 wire ([data sheet](../datasheets/jst-ph-series.pdf)). The battery currents are in [power.md](power.md#connector-and-fuse); the peaks above 2 A are an open item in [open-issues.md](open-issues.md).
- JP1 and JP2 connect RAIL_5V to ESP_5V and ADS_5V. Remove the jumper before powering that module from its own USB port.
- SW4 is the hand-fitted RESET button below BZ1: QON to GND, about 10 s hold. It power-cycles the system rails after firmware enables SFET_PRESENT; the charger and gauge stay powered.
- SW1 switches SW_EN between THERM_OK and GND. SW2 and SW3 are logic inputs; neither carries buzzer or radio power.
- BZ1 is driven directly by U9 (SN74LVC1G17) through R28, 150 Ω, on BUZZ_LIM. D11 remains the PWM source; firmware implements mute.

## Display: Seeed 100069905

1.47-inch touch display with an ESP32-S3 Plus. Its four pad groups are 0.9 mm drilled test points. The table lists them in carrier pin order.

| Carrier | Module pads | Carrier use |
|---|---|---|
| J6, 1x8 | TP3 MTDI (D14), TP4 MTDO (D13), TP5 EN, TP6 GND, TP7 MTMS (D15), TP8 MTCK (D12), TP9 USB_DN, TP10 USB_DP | support only; pin 4 is GND |
| J7, 1x7 | TP11 VBUS, TP12 GND, TP17 3V3, TP18 GND, TP19 D11, TP20 D12, TP21 D13 | see below |
| J8, 1x4 | TP13 GND, TP14 3V3, TP15 SDA, TP16 SCL | I2C |
| J9, 1x2 | TP22 USR1, TP23 USR2 | support only |

J7 signals:

| Pin | Net | Use |
|---|---|---|
| 1 | ESP_5V | 5 V from JP1 into the module's VBUS |
| 2, 4 | GND | |
| 3 | ESP_3V3 | 3.3 V out, supplies the barometer, buzzer driver, control expander and pull-up footprints |
| 5 | BUZZ_PWM | D11, buzzer PWM |
| 6 | ESP_UART_RX | D12, from ADSBee COMMS TX through R29, 1 kΩ |
| 7 | ESP_UART_TX | D13, to ADSBee COMMS RX through R30, 1 kΩ |

- D11 to D13 are the only free GPIOs the module exposes. Charger STAT and INOKB are therefore not wired to the display. The firmware reads the charging state from the fuel gauge.
- D14 and D15 on J6 are used inside the module (IMU interrupt, button B). The carrier leaves them unconnected.
- The I2C bus on J8 serves U13 (TCA9534, 0x20), U15 (FUSB303B, 0x21), U8 (MAX17048, 0x36), U10 (BMP580, 0x46) and U4 (BQ25798, 0x6B; its INT pin ends on TP8 and is polled). The display shares it with AXS5106L touch (0x63) and LSM6DS3 IMU (0x6A), as documented by [Seeed](https://wiki.seeedstudio.com/function_1.47_inch_touch_display_esp32s3/). These addresses do not conflict.
- R19 and R20 are 1.8 kΩ. In parallel with the display module's 5.1 kΩ pull-ups, the effective range is 1.284 to 1.373 kΩ including tolerances. This supports the 200 pF, 400 kHz design envelope for every participant with published limits; the AXS5106L limits remain unpublished. Calculations are in [I2C bus requirements](i2c-bus-requirements.md).

## Physical control inputs and GNSS control

U13 is TI TCA9534PWR (C783615), powered by ESP_3V3 with C40, 100 nF. A0, A1 and A2 are grounded for address 0x20. INT is unconnected; firmware polls over the shared 400 kHz bus. Leave all eight channels configured as inputs and polarity inversion disabled. Read the input-port register, 0x00. P2 and P3 control the GNSS receiver; see below.

| U13 pin | Net | Connection | Raw low | Raw high |
|---|---|---|---|---|
| P0 | BUZZ_ENABLE | SW2 common, pin 2 | mute | buzzer permitted |
| P1 | WIFI_ENABLE | SW3 common, pin 2 | Wi-Fi off | Wi-Fi on |
| P2 | GNSS_RESET_N | U16 RESET_N, pin 9 | GNSS in reset | run |
| P3 | GNSS_SAFEBOOT_N | U16 SAFEBOOT_N, pin 18 | safeboot at start-up | run; follows TIMEPULSE once running |
| P4 | CTRL_P4 | J12 pin 3 | contact closed | open |
| P5 | CTRL_P5 | J12 pin 4 | contact closed | open |
| P6 | CTRL_P6 | J12 pin 5 | contact closed | open |
| P7 | SW_ON_N | POWER slider through Q11 (J12 pin 6 unused) | slider on | slider off |

J12 pins 1, 2 and 6 are unconnected, pins 7 and 8 are GND. R48, R49 and R52 to R55 are 100 kΩ pull-ups to ESP_3V3 for P0, P1 and P4 to P7. P2 and P3 have no carrier pull-up; U16 pulls both up internally to +GNSS_3V3. Each grounded input draws 33 µA nominal at 3.3 V. SW2 and SW3 connect common to GND through pin 3; pin 1 is unconnected. Both maintained positions are therefore defined without switching directly between power rails. The switch footprint numbers are our convention; the manufacturer drawing does not number the terminals.

J12 is for short enclosure wiring and dry contacts to GND. It is not a powered peripheral port. Leave unused contacts open; their pull-ups define their levels. Fit the header by hand; it is DNP and excluded from JLCPCB assembly exports.

GNSS control rules:

- Drive P2 and P3 low only. Set their output-port bits to 0 once and switch the direction bit: output pulls low, input releases. Never drive them high: +GNSS_3V3 is switched by the ADSBee, and a high output into an unpowered receiver exceeds the V_IO + 0.3 V pin limit.
- Reset: hold P2 low for at least 1 ms.
- Safeboot: hold P3 low while releasing P2, then release P3. Safeboot is for firmware recovery only.
- Keep P3 an input in normal operation. The receiver connects SAFEBOOT_N to TIMEPULSE through 1 kΩ, so P3 reads the PPS pulse.

Both sliders determine requested state from startup. Keep PWM silent until the first valid mute read, debounce changes, and retain both last known states if a read fails. The touchscreen displays state without overriding the sliders. Low battery must not turn Wi-Fi off while SW3 requests it. Shared peripheral-failure indication and the UART command protocol remain firmware work.

## GNSS: MAX-M10S (U16)

The receiver is on the carrier, sheet `gnss.kicad_sch`. +GNSS_3V3 comes from the ADSBee's switched 3V3 output on J11 pin 1.

| U16 pin | Net | Connection |
|---|---|---|
| 2 TXD | GNSS_TXO | ADSBee GNSS RX, J11 pin 3 |
| 3 RXD | GNSS_RXI | ADSBee GNSS TX, J11 pin 2 |
| 4 TIMEPULSE | GNSS_PPS | ADSBee PPS, J11 pin 4, and the fix LED switch Q16 |
| 6 V_BCKP | GNSS_V_BCKP | backup supply, below |
| 9 RESET_N | GNSS_RESET_N | U13 P2 |
| 11 RF_IN | GNSS_RF_IN | antenna, below |
| 13 LNA_EN, 14 VCC_RF, 16, 17 | GNSS_ANT_* | antenna supervisor, below |
| 18 SAFEBOOT_N | GNSS_SAFEBOOT_N | U13 P3 |
| 5 EXTINT, 15 VIO_SEL | not connected | |

- Antenna: U.FL J13 with ESD diode D11, bias-T L3/C59 and DC block C58 (u-blox UBX-20053088 reference values). RF_IN is matched to 50 Ω inside the module. The trace is 15 mil coplanar waveguide with an 8 mil gap to the pour; the RF net class and the "RF CPWG gap to pour" rule in `carrier.kicad_dru` set it.
- Antenna supply: SW5 on feeds VCC_RF through Q14 and R82 (10 Ω) for an active antenna. Q15 switches it from LNA_EN. U17 detects an open antenna, R82 limits a short. SW5 off is for a passive antenna, which the receiver reports as open.
- Backup: U18 (TPS7A02, 25 nA) makes 3.3 V from the pack, BT1 (CR2032) covers a disconnected pack. D12 and D13 OR them into V_BCKP, so neither source charges the other.
- Fix LED: D14 through R86 and Q16, lit for each PPS pulse. The receiver sends PPS only with a fix; the pulse length is CFG-TP-LEN_LOCK_TP1, 100 ms by default. Cut JP4 to disable the LED.
- Firmware (ADSBee) after a cold start without backup: CFG-I2C-ENABLED 0, CFG-HW-ANT_CFG_SHORTDET 1, CFG-HW-ANT_SUP_SHORT_PIN 3, CFG-HW-ANT_CFG_OPENDET 1, CFG-HW-ANT_SUP_OPEN_PIN 2, CFG-HW-ANT_CFG_PWRDOWN 1, CFG-HW-ANT_CFG_RECOVER 1.

## ADSBee 1090U

Pin names follow the manufacturer's Rev D schematic. Header positions use the delivered module's caliper measurements, see [adsbee-estimate.md](../mechanical/adsbee-estimate.md).

Main header (module J5, 1x13) on carrier socket J10:

| Pin | Module | Carrier net |
|---|---|---|
| 1 | 5VIN | ADS_5V, from JP2 |
| 2, 13 | GND | GND |
| 3 | TX | ADS_COMMS_TX, to the display through R29 |
| 4 | RX | ADS_COMMS_RX, from the display through R30 |
| 5 to 12 | 3.3V, IOA, IOB, IOC, CS, CLK, MOSI, MISO | not connected |

GNSS header (module J4, 1x5) on carrier socket J11:

| Pin | Module | Carrier net |
|---|---|---|
| 1 | 3V3 | GNSS_3V3, switched output, 200 mA |
| 2 | TX | GNSS_RXI |
| 3 | RX | GNSS_TXO |
| 4 | PPS | GNSS_PPS |
| 5 | GND | GND |

- COMMS_UART uses 3.3 V, 115200 baud, 8E1 (8 data bits, even parity, 1 stop bit), no flow control. The custom ADSBee `airgrep` build profile accepts AT commands and returns replies on this port alongside CSBEE traffic. GNSS uses its separate UART. Provisioning and error handling are in [firmware.md](firmware.md#adsbee-uart).
- Keep the module's 5VIN reverse-polarity bypass jumper open.

## Silkscreen labels

- Module sockets J6 to J8, J10 and J11 carry a short label per pin. J9 has none.
- Labels use the module's own view: RX and TX at J7 are the display's, at J10 and J11 the ADSBee's.
- J2 and J3 print NTC and BAT on the top side, with + and - at J3. The bottom side prints NTC, BAT and THERM.
- Test points print VBAT, VSYS, PWR_EN, THERM, USB_VBUS, GND and 5V (TP1 to TP7).
- JP2 prints ADS 5V. SW1, SW2 and SW3 print POWER, MUTE and WIFI with their references. J12 prints INPUTS; its square pad is pin 1. The remaining labels are listed in [open-issues.md](open-issues.md).
