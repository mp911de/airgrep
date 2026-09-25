# Module interfaces

All headers 2.54 mm pitch. Carrier side: Samtec SSQ sockets (8.51 mm), hand soldered. Coordinates are in `mechanical/source-geometry.json` and `mechanical/adsbee-estimate.json`.

## Display: Seeed 100069905 (1.47-inch ESP32-S3 Plus)

Four separate groups, seen from the controller (back) side. Front view is mirrored in X. Drill 0.9 mm.

| Group | Pads | Carrier use |
|---|---|---|
| 1x8 upper left | TP3 MTDI (D14 = IMU_INT), TP4 MTDO (D13), TP5 EN, TP6 GND, TP7 MTMS (D15 = BTN_B), TP8 MTCK (D12), TP9 USB_DN, TP10 USB_DP | support only, GND connected. D14 and D15 are used internally (IMU interrupt, button B), never drive them. |
| 1x7 upper right | TP11 VBUS, TP12 GND, TP17 3V3, TP18 GND, TP19 D11, TP20 D12, TP21 D13 | VBUS from shunt, GND, 3V3 out for gauge pull-ups, D11 = buzzer PWM, D12 = ESP UART RX from ADSBee COMMS TX (R29 1 kΩ), D13 = ESP UART TX to ADSBee COMMS RX (R30 1 kΩ). D11 to D13 are the only free exposed GPIOs (Seeed pin map, 24 Sep). |
| 1x4 lower left | TP13 GND, TP14 3V3, TP15 SDA, TP16 SCL | I2C to gauge (MAX17048, 0x36) and barometer (BMP581, 0x46). The BMP581 sits just left of this header. |
| 1x2 lower right | TP22 USR1, TP23 USR2 | support only |

Touch and IMU already use SDA D4 / SCL D5. Buttons are D19 / D15 on board, no carrier wiring. Charger STAT and PG are not wired to the display (no free pin); charging state comes from the MAX17048 charge-rate register.
Mounting holes 2.5 mm, pattern 20 × 22.5 mm at the button end. Assembly 26.4 × 51.4 × 12.6 mm, PCB 25 × 50 mm.

## GNSS: SparkFun GPS-18037 (MAX-M10S)

Board 38.10 × 30.48 mm, SMA on one edge, 3.302 mm holes at 33.02 × 25.40 mm. Use J5 (1x8), J3 (1x4) optional support.

| J5 | Net | Carrier |
|---|---|---|
| 1 | GND | GND |
| 2 | 3.3V | ADSBee GNSS 3V3 out |
| 3 | !SAFE | NC |
| 4 | TXO | ADSBee GNSS RX |
| 5 | RXI | ADSBee GNSS TX |
| 6 | PPS | ADSBee PPS |
| 7 | INT | NC |
| 8 | !RESET | NC |

## ADSBee 1090U (Rev D schematic, geometry image-estimated)

Board about 48 × 30 mm, M2.5 holes. Fit check against real hardware required before drilling.

- J5 main 1x13: 5VIN, GND, TX, RX, 3.3V, IOA, IOB, IOC, CS, CLK, MOSI, MISO, GND. Carrier uses 5VIN (from shunt), both GND, and TX/RX = COMMS_UART (3.3 V, 115200 default, AT commands and GDL90/MAVLink/CSBEE output) to the display ESP32 via R29/R30. Wi-Fi then becomes optional; the ESP can also send AT commands to the ADSBee. Decided 24 Sep.
- J4 GNSS 1x5: 3V3 (200 mA switched out), TX, RX, PPS, GND. All five to the GNSS J5 as above.
- Keep the 5VIN reverse-polarity bypass jumper open. USB-C on the module stays accessible for programming.

## Carrier connectors

| Ref | Function | Part |
|---|---|---|
| J_USB (J1) | charging input | USB-C receptacle, 16P USB 2.0, Hanbo MC-107DMS (LCSC C3039295) |
| J_BAT | battery | pack connector, MPN pending, rated for 5 A |
| J_NTC | pack NTC | 2-pin locking |
| J_THERM | pack thermostat | 2-pin locking |
| JP_PWR_ESP, JP_PWR_ADS | service shunts | Samtec TSW-102 + SNT-100 shunt |
| SW_POWER, SW_MUTE | sliders | C&K JS102011SAQN |
| BZ1 | piezo | Murata PKMCS0909E4000-R1 (C910763, 9 × 9 mm, 4 kHz, 65 dB), driven by SN74LVC1G17 via 150 Ω, mute slider selects PWM or GND. Replaces CPT-9019S (6 in stock). |

## Silkscreen labels

Every module header carries a short label per pin, printed perpendicular to the row. Labels use the module's own view: RX/TX on J7 are the display ESP's RX/TX, on J10/J11 the ADSBee's. J2, J3, J4 print NTC, BAT, THERM instead of their reference; only BAT has pin marks (+/−). Test points print their signal name (VBAT, VSYS, PWR_EN, THERM, VBUS, GND, 5V) alternating above and below the row. JP1/JP2 keep their reference plus ESP 5V / ADS 5V.
