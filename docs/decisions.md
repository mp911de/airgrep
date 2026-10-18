# Design decisions

The active target is `hardware/carrier-ti-charger/`. Its PCB and schematics define the implementation. The MAX77757 board is frozen; earlier choices remain in git and its hardware files.

## Charger and power path

- BQ25798 replaces MAX77757 because the ESP can set charge current and thermal regulation and read status. It still charges at 1 A without host firmware. Host operation targets 1.8 A; watchdog recovery is specified in [firmware.md](firmware.md#charger-host-control-bq25798-i2c-0x6b).
- USB-C stays at 5 V. FUSB303B supplies CC advertisement over the shared I2C bus; BC1.2 remains in the charger. The hardware input clamp remains conservative until firmware confirms a 3 A source.
- TPS61088 and TPS22975 provide a switched 5 V receiver rail. A disabled boost still conducts, so U7 is required. The selected inductors and timing components are in the schematic; candidate integrated-disconnect boosts are not fitted designs.
- TPS259474L feeds isolate the modules, block reverse current and cut overvoltage. Fault recovery may require cycling the affected input. Static divider calculations do not establish transient protection; see [power.md](power.md#module-feeds).
- POWER controls enable signals, not battery current. With USB present, the display remains available in charger mode while the receiver rail is off.

## Independent protection and recovery

- The pack NTC controls charging through TS without firmware. A separate thermostat disables charging through CE and removes receiver power.
- TLV840 monitors SYS so USB-only operation remains possible. The undervoltage latch prevents repeated restarts when the unloaded pack voltage rebounds. POWER or USB clears it.
- Q13 and SW4 provide a QON system reset without opening the case. Firmware must enable SFET_PRESENT. The charger and fuel gauge remain powered, so this reset cannot recover every peripheral fault.
- Reverse-polarity protection is selected but not implemented, see [issue 0002](../issues/0002-battery-reverse-polarity-protection.md). Off-state battery isolation is a separate proposal in [issue 0001](../issues/0001-battery-disconnect-switch.md).

## Interfaces and mechanics

- Wired UART keeps traffic and Wi-Fi control independent of the radio link. [firmware.md](firmware.md#adsbee-uart) owns framing and command handling.
- Maintained MUTE and Wi-Fi sliders define requested state. [firmware.md](firmware.md#physical-controls) owns startup and read-failure behavior; [interfaces.md](interfaces.md) owns pin maps.
- MAX-M10S is soldered on the carrier. The removable SparkFun GNSS board is no longer part of this target.
- The display and alternative XIAO Plus share carrier signals. Fit one controller only; the mechanical fit of the Plus remains unverified.
- Modules remain socketed for the prototype. ADSBee mating positions use delivered-module measurements; [mechanical.md](mechanical.md) owns assembly and enclosure assumptions.
- Four copper layers provide a ground reference, power distribution and heat spreading. The selected top heatsink and enclosure vents require physical validation; no bottom-side spreader is part of the design.
