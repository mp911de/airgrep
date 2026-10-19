# Requirements

## Function

1. Receive 1090 MHz ADS-B and 868 MHz FLARM, receive only, with an ADSBee 1090U.
2. GNSS with Galileo and PPS: u-blox MAX-M10S on the carrier, UART and PPS to the ADSBee.
3. Local display: gets traffic and status from the ADSBee over a direct UART and the battery state of charge over I2C.
4. Wi-Fi stays available as an alternative link to the display. A maintained Wi-Fi slider controls the ADSBee radio, primarily for the EFB feed and to save battery power. The ESP applies its position through AT commands on the existing UART in the custom ADSBee firmware.
5. Wi-Fi feed to an EFB (GDL90).
6. Quiet piezo feedback for ground use, with a maintained MUTE slider.
7. Low battery does not override the Wi-Fi slider. Show a low-battery warning, automatic backlight dimming may remain.

## Power

1. Battery: 1S LiPo pack, 3.7 V, with built-in protection, behind the PCB. The reference pack is 10 Ah, 100 x 60 x 11 mm.
2. Load: 5 V at 0.8 A nominal and 1 A expected peak. Up to 1.5 A is short-term headroom, not a continuous requirement. Peak duration and repetition are still to be measured.
3. Charging through a carrier-mounted USB-C receptacle at 5 V only. The device never requests a higher voltage. Overvoltage on the input is cut off in hardware.
4. The USB source supplies what it can and the battery supplies the remainder. Sources: Type-C at 1.5 A or 3 A, USB-A chargers at 1 to 2.4 A through an A-to-C cable, PC ports at 0.5 A.
5. Fast charge at 1.8 A nominal. With the device on, the cell gets what remains after the system load.

## Manufacturing

1. JLCPCB fabricates the board and assembles all SMD parts. The user solders module sockets and harness connectors.
2. Modules stay removable. A shunt per 5 V feed lets a module stay unpowered in its socket.
3. Antennas: SMA bulkhead connections in the enclosure for GNSS, 1090 MHz and sub-GHz.
4. Enclosure: custom 3D print, designed after the PCB.
