# Requirements

Settled with the user. Change only with the user. Resulting figures are in [specs.md](specs.md); the design is in [power.md](power.md), [interfaces.md](interfaces.md) and [mechanical.md](mechanical.md).

## Function

1. Receive 1090 MHz ADS-B and 868 MHz FLARM, receive only, with an ADSBee 1090U. FLARM decoding is not yet shown to work ([open-issues.md](open-issues.md)).
2. GNSS with Galileo and PPS: SparkFun GPS-18037 (u-blox MAX-M10S), UART to the ADSBee.
3. Local display: Seeed 100069905, 1.47 inch ESP32-S3 Plus touch display. It gets traffic and status from the ADSBee over a direct UART and the battery state of charge over I2C.
4. Wi-Fi stays available as an alternative link to the display. The ESP can switch it with AT commands.
5. Wi-Fi feed to an EFB (GDL90).
6. Quiet piezo feedback for ground use, with a hardware MUTE slider. No firmware mute.

## Power

1. Battery: 1S LiPo pack, 3.7 V, with built-in protection, behind the PCB. The reference pack is 10 Ah, 100 x 60 x 11 mm.
2. Load: 5 V at 0.8 A nominal and 1 A expected peak. Up to 1.5 A is short-term headroom, not a continuous requirement. Peak duration and repetition are still to be measured.
3. Charging through a carrier-mounted USB-C receptacle at 5 V only. The device never requests a higher voltage. Overvoltage on the input is cut off in hardware.
4. The USB source supplies what it can and the battery supplies the remainder. Sources: Type-C at 1.5 A or 3 A, USB-A chargers at 1 to 2.4 A through an A-to-C cable, PC ports at 0.5 A.
5. Fast charge at 1.8 A nominal. With the device on, the cell gets what remains after the system load.
6. No battery current flows back into the USB port.
7. The charger is standalone. The ESP only reads the state of charge from the gauge.
8. One maintained POWER slider turns the whole device on and off, also with USB present. Charging continues with the device off. The slider carries control current only.
9. Battery temperature: an NTC on the pack lets the charger suspend charging when the pack is too cold or too hot. The hot cut-off is at most 45 °C across all tolerances. The cold cut-off may sit a few degrees below the pack's 0 °C charge rating.
10. Over-temperature: a separate thermostat contact on the pack cuts charging and all loads above 60 °C nominal, without firmware.
11. Fuel gauge MAX17048 on the carrier, I2C to the display.
12. Undervoltage: the loads switch off in hardware before the pack is deeply discharged ([threshold](specs.md#low-battery-cut-off-voltage)).
13. The device runs from USB with an empty or missing battery while the thermostat connection is closed. Removing the complete battery harness, including J4, prevents USB-only operation.

## Manufacturing

1. JLCPCB fabricates the board and assembles all SMD parts. The user solders module sockets and harness connectors.
2. Modules stay removable. Two removable shunts isolate the display and ADSBee 5 V feeds for USB servicing.
3. Antennas: SMA bulkhead connections in the enclosure for GNSS, 1090 MHz and sub-GHz ([parts](bom-hand-sourced.md)).
4. Enclosure: custom 3D print, designed after the PCB.
