# Module interfaces

Connectors on the carrier and what each pin carries. Positions and mounting are in [mechanical.md](mechanical.md), the hand-fitted parts in [bom-hand-sourced.md](bom-hand-sourced.md).

All module sockets are 2.54 mm single-row female headers. Their height is given in [mechanical.md](mechanical.md#assembly-split).

## Carrier connectors

Designators run by physical rows, top to bottom and left to right. The four adjacent display/Plus headers start J1 to J4; JP jumpers use a separate sequence.

| Ref | Function | Part | Fitted by |
|---|---|---|---|
| J1 | display power/UART/PWM and spare | 1x7 socket, 2.54 mm | hand |
| J2 | XIAO Plus power/UART/PWM and spare | 1x7 header, 2.54 mm | hand |
| J3 | XIAO Plus I2C and spares | 1x7 header, 2.54 mm | hand |
| J4 | display support and shared spares | 1x8 socket, 2.54 mm | hand |
| J5 | ADSBee GNSS | 1x5 socket, 2.54 mm | hand |
| J6 | ADSBee main | 1x13 socket, 2.54 mm | hand |
| J7 | GNSS antenna | U.FL | JLCPCB |
| J8 | display USR1/USR2 support | 1x2 socket, 2.54 mm | hand |
| J9 | display I2C | 1x4 socket, 2.54 mm | hand |
| J10 | physical control inputs | 2x4 header, 2.54 mm | hand |
| J11 | battery | JST B2B-PH-K-S, pin 1 is + | hand |
| J12 | pack NTC | JST B2B-PH-K-S, 2-pin | hand |
| J13 | pack thermostat | JST B2B-PH-K-S, 2-pin | hand |
| J14 | USB-C charging input | Hanbo MC-107DMS, 16P USB 2.0, C3039295 | JLCPCB |
| JP1 | 5 V to display/Plus | 1x2 header with shunt | hand |
| JP2 | GNSS PPS LED enable | solder jumper, cut to disable | PCB |
| JP3 | 5 V to ADSBee | 1x2 header with shunt | hand |
| SW1 | POWER slider | HRO K3-1280S-F1, C92658 | JLCPCB |
| SW2 | MUTE slider | HRO K3-1280S-F1, C92658 | JLCPCB |
| SW3 | Wi-Fi slider | HRO K3-1280S-F1, C92658 | JLCPCB |
| BZ1 | piezo buzzer | Murata PKMCS0909E4000-R1, C910763 | JLCPCB |

Notes:

- J11 carries the full battery current. JST rates the PH series at 2 A per contact with AWG 24 wire ([data sheet](../datasheets/jst-ph-series.pdf)). The battery currents are in [power.md](power.md#connector-and-fuse); the peaks above 2 A are an open item in [open-issues.md](open-issues.md).
- JP1 and JP3 connect RAIL_5V to ESP_5V and ADS_5V. Remove the jumper before powering that module from its own USB port.
- SW4 is the hand-fitted RESET button below BZ1: QON to GND, about 10 s hold. It power-cycles the system rails after firmware enables SFET_PRESENT; the charger and gauge stay powered.
- SW1 switches SW_EN between THERM_OK and GND. SW2 and SW3 are logic inputs; neither carries buzzer or radio power.
- BZ1 is driven by U9 (SN74LVC1G17) through R28, 1 kΩ, and C66, 1 µF, on BUZZ_AC; R87, 10 kΩ across BZ1, bleeds the DC off because Murata forbids DC on the piezo. D11 remains the PWM source; firmware implements mute.

## Display: Seeed 100069905

1.47-inch touch display with an ESP32-S3 Plus. Its four pad groups are 0.9 mm drilled test points. The table lists them in carrier pin order.

| Carrier | Module pads | Carrier use |
|---|---|---|
| J4, 1x8 | TP3 MTDI (D14), TP4 MTDO (D13), TP5 EN, TP6 GND, TP7 MTMS (D15), TP8 MTCK (D12), TP9 USB_DN, TP10 USB_DP | pins 1, 2, 3, 4 and 7 connect to the same-numbered J3 pins; pin 4 has no carrier GND connection |
| J1, 1x7 | TP11 VBUS, TP12 GND, TP17 3V3, TP18 GND, TP19 D11, TP20 D12, TP21 D13 | see below |
| J9, 1x4 | TP13 GND, TP14 3V3, TP15 SDA, TP16 SCL | I2C |
| J8, 1x2 | TP22 USR1, TP23 USR2 | support only |

J1 signals:

| Pin | Net | Use |
|---|---|---|
| 1 | ESP_5V | 5 V from JP1 into the module's VBUS |
| 2 | GND | |
| 4 | XIAO_D10 | spare Plus GPIO through J2.4; module GND pad, isolated from carrier GND |
| 3 | ESP_3V3 | 3.3 V out, supplies the barometer, buzzer driver, control expander and pull-up footprints |
| 5 | BUZZ_PWM | D11, buzzer PWM |
| 6 | ESP_UART_RX | D12, from ADSBee COMMS TX through R29, 1 kΩ |
| 7 | ESP_UART_TX | D13, to ADSBee COMMS RX through R30, 1 kΩ |

- D11 to D13 are the only free GPIOs the module exposes. Charger STAT and INT are not wired to the display GPIOs. Firmware polls BQ25798 for charging status and MAX17048 for state of charge.
- D14 and D15 on J4 are used inside the module (IMU interrupt, button B). D14 also reaches the otherwise vacant J3.1 socket; D15 is unconnected on the carrier.
- Pull-ups, electrical limits and bench acceptance are in [I2C bus requirements](i2c-bus-requirements.md).

## I2C address map

The shared 3.3 V bus connects through J9. Addresses below are 7-bit and do not conflict. The touch controller and IMU are present only with the original display, as documented by [Seeed](https://wiki.seeedstudio.com/function_1.47_inch_touch_display_esp32s3/).

| Address | Device | Location | Function | Datasheet |
|---|---|---|---|---|
| `0x20` | TCA9534PWR | Carrier U13 | Physical controls and GNSS control | [TI TCA9534](../datasheets/tca9534.pdf) |
| `0x21` | FUSB303B | Carrier U15 | USB-C CC detection | [onsemi FUSB303B](../datasheets/fusb303b.pdf) |
| `0x36` | MAX17048 | Carrier U8 | Battery fuel gauge | [ADI MAX17048/MAX17049](../datasheets/max17048.pdf) |
| `0x46` | BMP580 | Carrier U10 | Barometer | [Bosch BMP580](../datasheets/bst-bmp580-ds004.pdf) |
| `0x63` | AXS5106L | Display module | Touch controller | [AXS5106L](../datasheets/axs5106l.pdf) |
| `0x6A` | LSM6DS3 | Display module | IMU | [ST LSM6DS3](../datasheets/lsm6ds3.pdf) |
| `0x6B` | BQ25798 | Carrier U4 | Charger | [TI BQ25798](../datasheets/bq25798.pdf) |

BQ25798 INT ends on TP8; firmware polls its status over I2C.

## Alternative controller: XIAO ESP32-S3 Plus

J2 and J3 accept the Plus instead of the display module. Fit one controller only; both sockets share power, I2C and the UART/buzzer nets. The Plus is flipped so its power pins lie beside J1. Header pitch is 100 mil; the fitted row spacing is 579 mil against the module's nominal 600 mil, relying on pin compliance. Physical fit remains unverified. Leave its battery pads disconnected and remove JP1 before connecting its USB port.

| Carrier pin | Plus pin | Carrier net |
|---|---|---|
| J2.1, J2.2, J2.3 | 5V, GND, 3V3 | ESP_5V, GND, ESP_3V3 |
| J2.4 | D10 / GPIO9 | XIAO_D10, also J1.4 |
| J2.5 | D9 / GPIO8 | BUZZ_PWM |
| J2.6 | D8 / GPIO7 | ESP_UART_RX, from ADSBee TX through R29 |
| J2.7 | D7 / GPIO44 | ESP_UART_TX, to ADSBee RX through R30 |
| J3.5 | D4 / GPIO5 | I2C_SDA |
| J3.6 | D5 / GPIO6 | I2C_SCL |

J2.4 connects to J1.4 as XIAO_D10. The other five spare Plus pins also reach the display socket through short top-layer connections, for header reuse:

| J3 pin | J4 pin | Plus pin / net | Display pad on J4 |
|---|---|---|---|
| 1 | 1 | D0 / XIAO_D0 | MTDI, D14 |
| 2 | 2 | D1 / XIAO_D1 | MTDO, D13 |
| 3 | 3 | D2 / XIAO_D2 | EN |
| 4 | 4 | D3 / XIAO_D3 | GND on the display only |
| 7 | 7 | D6 / XIAO_D6 | USB_DN |

J3.5/J3.6 remain SDA/SCL and do not connect to J4.5/J4.6. J4.4 and J1.4 have no carrier GND connection; the display retains ground through J1.2 and J9.1. With the display fitted, its internal ground connections ground J4.4 and J1.4, so never fit the Plus simultaneously. These alternate connections do not make J4/J1 general-purpose expansion ports while the display is fitted.

Firmware maps UART1 RX to GPIO7 and TX to GPIO44, and buzzer PWM to GPIO8. D7's printed RX label describes its default function; the GPIO matrix permits TX here. The carrier supports the Plus without extra pull-ups; see [I2C bus requirements](i2c-bus-requirements.md) before fitting optional R90/R91.

BMP580 U10 pin 7 (INT) is intentionally unconnected. Poll the sensor over I2C; configure its unused output as described in [firmware.md](firmware.md#i2c-bus).

## Physical control inputs and GNSS control

U13 is TI TCA9534PWR (C783615), powered by ESP_3V3 with C40, 100 nF. A0, A1 and A2 are grounded for address 0x20. INT is unconnected; firmware polls over the shared 400 kHz bus. P0/P1/P4-P7 are inputs. P2/P3 provide low-only GNSS control. Configuration and sequencing are in [firmware.md](firmware.md#gnss-control).

| U13 pin | Net | Connection | Raw low | Raw high |
|---|---|---|---|---|
| P0 | BUZZ_ENABLE | SW2 common, pin 2 | mute | buzzer permitted |
| P1 | WIFI_ENABLE | SW3 common, pin 2 | Wi-Fi off | Wi-Fi on |
| P2 | GNSS_RESET_N | U16 RESET_N, pin 9 | GNSS in reset | run |
| P3 | GNSS_SAFEBOOT_N | U16 SAFEBOOT_N, pin 18 | safeboot at start-up | released; isolated from TIMEPULSE by D16 |
| P4 | CTRL_P4 | J10 pin 3 | contact closed | open |
| P5 | CTRL_P5 | J10 pin 4 | contact closed | open |
| P6 | CTRL_P6 | J10 pin 5 | contact closed | open |
| P7 | SW_ON_N | POWER slider through Q11 (J10 pin 6 unused) | slider on | slider off |

J10 pins 1, 2 and 6 are unconnected, pins 7 and 8 are GND. R48, R49 and R52 to R55 are 100 kΩ pull-ups to ESP_3V3 for P0, P1 and P4 to P7. R88 and R89 pull P2 and P3 up to ESP_3V3 on the expander side; D15 and D16 block that bias from U16, which pulls RESET_N and SAFEBOOT_N up internally to +GNSS_3V3. Each grounded input draws 33 µA nominal at 3.3 V. SW2 and SW3 connect common to GND through pin 3; pin 1 is unconnected. Both maintained positions are therefore defined without switching directly between power rails. The switch footprint numbers are our convention; the manufacturer drawing does not number the terminals.

J10 is for short enclosure wiring and dry contacts to GND. It is not a powered peripheral port. Leave unused contacts open; their pull-ups define their levels. Fit the header by hand; it is DNP and excluded from JLCPCB assembly exports.

D15/D16 isolate the expander pull-ups from the switched GNSS rail. Firmware must pull reset and safeboot low or release them, as specified in [GNSS control](firmware.md#gnss-control). Slider behavior is specified in [physical controls](firmware.md#physical-controls).

## GNSS: MAX-M10S (U16)

The receiver is on the carrier, sheet `gnss.kicad_sch`. +GNSS_3V3 comes from the ADSBee's switched 3V3 output on J5 pin 1.

| U16 pin | Net | Connection |
|---|---|---|
| 2 TXD | GNSS_TXO | ADSBee GNSS RX, J5 pin 3 |
| 3 RXD | GNSS_RXI | ADSBee GNSS TX, J5 pin 2 |
| 4 TIMEPULSE | GNSS_PPS | ADSBee PPS, J5 pin 4, and the fix LED switch Q16 |
| 6 V_BCKP | GNSS_V_BCKP | backup supply, below |
| 9 RESET_N | GNSS_RESET_N | U13 P2 |
| 11 RF_IN | GNSS_RF_IN | antenna, below |
| 13 LNA_EN, 14 VCC_RF, 16, 17 | GNSS_ANT_* | antenna supervisor, below |
| 18 SAFEBOOT_N | GNSS_SAFEBOOT_N | U13 P3 |
| 5 EXTINT, 15 VIO_SEL | not connected | |

- Antenna: U.FL J7 with ESD diode D11, bias-T L3/C59 and DC block C58 (u-blox UBX-20053088 reference values). RF_IN is matched to 50 Ω inside the module. The trace is 15 mil coplanar waveguide with an 8 mil gap to the pour; the RF net class and the "RF CPWG gap to pour" rule in `carrier.kicad_dru` set it.
- Antenna supply: SW5 on feeds VCC_RF through Q14 and R82 (10 Ω) for an active antenna. Q15 switches it from LNA_EN. U17 detects an open antenna, R82 limits a short. SW5 off is for a passive antenna, which the receiver reports as open.
- Backup: U18 (TPS7A02, 25 nA) makes 3.3 V from the pack, BT1 (CR2032) covers a disconnected pack. D12 and D13 OR them into V_BCKP, so neither source charges the other.
- Fix LED: D14 (KT-0603W white, C2290) through R86 and Q16, lit for each PPS pulse. The receiver sends PPS only with a fix; the pulse length is CFG-TP-LEN_LOCK_TP1, 100 ms by default. Cut JP2 to disable the LED.
- Firmware (ADSBee) after a cold start without backup: CFG-I2C-ENABLED 0, CFG-HW-ANT_CFG_SHORTDET 1, CFG-HW-ANT_SUP_SHORT_PIN 3, CFG-HW-ANT_CFG_OPENDET 1, CFG-HW-ANT_SUP_OPEN_PIN 2, CFG-HW-ANT_CFG_PWRDOWN 1, CFG-HW-ANT_CFG_RECOVER 1.

## ADSBee 1090U

Pin names follow the manufacturer's Rev D schematic. Header positions use the delivered module's caliper measurements, see [adsbee-estimate.md](../mechanical/adsbee-estimate.md).

Main header (module J5, 1x13) on carrier socket J6:

| Pin | Module | Carrier net |
|---|---|---|
| 1 | 5VIN | ADS_5V, from JP3 |
| 2, 13 | GND | GND |
| 3 | TX | ADS_COMMS_TX, to the display through R29 |
| 4 | RX | ADS_COMMS_RX, from the display through R30 |
| 5 to 12 | 3.3V, IOA, IOB, IOC, CS, CLK, MOSI, MISO | not connected |

GNSS header (module J4, 1x5) on carrier socket J5:

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

- Module sockets J1, J4, J9, J6 and J5 carry a short label per pin. J8 has none.
- Labels use the module's own view: RX and TX at J1 are the display's, at J6 and J5 the ADSBee's.
- J12 and J11 print NTC and BAT on the top side, with + and - at J11. The bottom side prints NTC, BAT and THERM.
- Test points print VBAT, VSYS, PWR_EN, THERM, USB_VBUS, GND and 5V (TP1 to TP7).
- JP3 prints ADS 5V. SW1, SW2 and SW3 print POWER, MUTE and WIFI with their references. J10 prints INPUTS; its square pad is pin 1. The remaining labels are listed in [open-issues.md](open-issues.md).

## Connector reference history

TI board annotation changed on 2026-10-18; dated reviews and the frozen MAX board retain the old references. Electrical nets, pads, footprint positions and routed copper did not change.

| Previous TI ref | Current TI ref |
|---|---|
| J7 | J1 |
| J14 | J2 |
| J15 | J3 |
| J6 | J4 |
| J11 | J5 |
| J10 | J6 |
| J13 | J7 |
| J9 | J8 |
| J8 | J9 |
| J12 | J10 |
| J3 | J11 |
| J2 | J12 |
| J4 | J13 |
| J1 | J14 |
| JP1 | JP1 |
| JP4 | JP2 |
| JP2 | JP3 |
