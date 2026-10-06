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
| J5 | GNSS socket | 1x8 | hand |
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
- The I2C bus on J8 serves U13 (TCA9534, 0x20), U8 (MAX17048, 0x36) and U10 (BMP581, 0x46). The display shares it with AXS5106L touch (0x63) and LSM6DS3 IMU (0x6A), as documented by [Seeed](https://wiki.seeedstudio.com/function_1.47_inch_touch_display_esp32s3/). These addresses do not conflict.
- The I2C pull-ups are the module's own, 5.1 kΩ. R19 and R20 (4.7 kΩ) on the carrier are not fitted. The firmware therefore runs the bus at 100 kHz.

## Physical control inputs

U13 is TI TCA9534PWR (C783615), powered by ESP_3V3 with C40, 100 nF. A0, A1 and A2 are grounded for address 0x20. INT is unconnected; firmware polls over the existing 100 kHz bus. Leave all eight channels configured as inputs and polarity inversion disabled. Read the input-port register, 0x00.

| U13 input | Net | Connection | Raw low | Raw high |
|---|---|---|---|---|
| P0 | BUZZ_ENABLE | SW2 common, pin 2 | mute | buzzer permitted |
| P1 | WIFI_ENABLE | SW3 common, pin 2 | Wi-Fi off | Wi-Fi on |
| P2 | CTRL_P2 | J12 pin 1 | contact closed | open |
| P3 | CTRL_P3 | J12 pin 2 | contact closed | open |
| P4 | CTRL_P4 | J12 pin 3 | contact closed | open |
| P5 | CTRL_P5 | J12 pin 4 | contact closed | open |
| P6 | CTRL_P6 | J12 pin 5 | contact closed | open |
| P7 | CTRL_P7 | J12 pin 6 | contact closed | open |

J12 pins 7 and 8 are GND. R48 to R55 are 100 kΩ pull-ups to ESP_3V3, one per input in P0 to P7 order. Each grounded input draws 33 µA nominal at 3.3 V. SW2 and SW3 connect common to GND through pin 3; pin 1 is unconnected. Both maintained positions are therefore defined without switching directly between power rails. The switch footprint numbers are our convention; the manufacturer drawing does not number the terminals.

J12 is for short enclosure wiring and dry contacts to GND. It is not a powered peripheral port. Leave unused contacts open; their pull-ups define their levels. Fit the header by hand; it is DNP and excluded from JLCPCB assembly exports.

Both sliders determine requested state from startup. Keep PWM silent until the first valid mute read, debounce changes, and retain both last known states if a read fails. The touchscreen displays state without overriding the sliders. Low battery must not turn Wi-Fi off while SW3 requests it. Shared peripheral-failure indication and the UART command protocol remain firmware work.

## GNSS: SparkFun GPS-18037 (MAX-M10S)

The carrier uses the module's 1x8 header J5, on carrier socket J5.

| Pin | Module | Carrier net |
|---|---|---|
| 1 | GND | GND |
| 2 | 3.3V | GNSS_3V3, from the ADSBee |
| 3 | !SAFE | not connected |
| 4 | TXO | GNSS_TXO, to ADSBee GNSS RX |
| 5 | RXI | GNSS_RXI, from ADSBee GNSS TX |
| 6 | PPS | GNSS_PPS, to ADSBee PPS |
| 7 | INT | not connected |
| 8 | !RESET | not connected |

## ADSBee 1090U

Pin names follow the manufacturer's Rev D schematic. The header positions are an image estimate, see [adsbee-estimate.md](../mechanical/adsbee-estimate.md).

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

- COMMS_UART runs at 3.3 V, 115200 baud by default, and carries traffic output to the display without Wi-Fi. Stock firmware has not demonstrated AT command intake on this port. The planned custom ADSBee firmware must add wired Wi-Fi commands and responses alongside traffic output; see [physical controls](../workflows/physical-controls.md).
- Keep the module's 5VIN reverse-polarity bypass jumper open.

## Silkscreen labels

- Module sockets J5 to J8, J10 and J11 carry a short label per pin. J9 has none.
- Labels use the module's own view: RX and TX at J7 are the display's, at J10 and J11 the ADSBee's.
- J2 and J3 print NTC and BAT on the top side, with + and - at J3. The bottom side prints NTC, BAT and THERM.
- Test points print VBAT, VSYS, PWR_EN, THERM, USB_VBUS, GND and 5V (TP1 to TP7).
- JP2 prints ADS 5V. SW1, SW2 and SW3 print POWER, MUTE and WIFI with their references. J12 prints INPUTS; its square pad is pin 1. The remaining labels are listed in [open-issues.md](open-issues.md).
