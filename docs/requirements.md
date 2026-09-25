# Requirements

Settled with the user, 23 to 24 Sep 2026. Change only with the user.

## Function

1. Receive 1090 MHz ADS-B and 868 MHz FLARM (receive only) with an ADSBee 1090U. FLARM decoding is not yet demonstrated by the ADSBee firmware.
2. GNSS with Galileo and PPS: SparkFun GPS-18037 (u-blox MAX-M10S), UART to ADSBee.
3. Local display: Seeed 100069905, 1.47-inch ESP32-S3 Plus touch display. Gets traffic/status from the ADSBee over a direct UART (COMMS_UART) and battery state of charge over I2C; Wi-Fi remains available as an alternative and can be switched from the ESP via AT commands.
4. Wi-Fi feed to an EFB (GDL90).
5. Quiet piezo feedback for ground use, with a hardware MUTE slider. No firmware mute.

## Power

1. Battery: 1S LiPo, 3.7 V, 10 Ah, 100 × 60 × 11 mm, behind the PCB. Exact SKU and protection still to be recorded.
2. Load: 5 V, 0.8 to 1 A continuous. Design margin 1.5 A (decided 24 Sep, replaces 2.5 A).
3. Charging via a carrier-mounted USB-C receptacle at 5 V only. Never request a higher voltage. Overvoltage on the input must be cut off in hardware.
4. USB source supplies what it can; the battery supplies the remainder. Sources: Type-C 1.5 A / 3 A, USB-A chargers 1 to 2.4 A through an A-to-C cable, PC ports 0.5 A.
5. Charge current is whatever is left after the system load, limited by the source.
6. No battery current may flow back into the USB port.
7. The charger is standalone. The ESP has no role in charging. It only reads state of charge from the gauge.
8. One maintained POWER slider turns the whole device on and off, including when USB is present. Charging continues with the device off. The slider carries control current only.
9. Battery temperature: NTC thermistor on the pack lets the charger suspend charging below 0 °C and above 60 °C. A separate thermostat contact on the pack cuts charging and all loads above 60 °C nominal, without firmware.
11. Undervoltage: loads switch off when SYS drops below about 3.1 V, in hardware. Device still runs from USB with an empty or missing battery.
10. Fuel gauge MAX17048 on the carrier, I2C to the display.

## Manufacturing

1. JLCPCB fabricates and assembles all SMD parts. User solders module sockets and harness connectors.
2. Modules stay removable. Two removable shunts isolate the display and ADSBee 5 V feeds for USB servicing.
3. Antennas: three panel SMA bulkheads (GNSS, 1090, 868), models chosen later.
4. Enclosure: custom 3D print, designed after the PCB.
