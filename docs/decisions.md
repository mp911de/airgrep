# Design decisions

The active target is `hardware/carrier-ti-charger/`.

## Charger and power path

- USB-C stays at 5 V. FUSB303B supplies CC advertisement over the shared I2C bus. BC1.2 remains in the charger.
- TPS61088 and TPS22975 provide a switched 5 V receiver rail. A disabled boost still conducts, so U7 is required. The selected inductors and timing components are in the schematic; candidate integrated-disconnect boosts are not fitted designs.
- TPS259474L feeds isolate the modules, block reverse current and cut overvoltage. Fault recovery may require cycling the affected input. 
- POWER controls enable signals, not battery current. With USB present, the display remains available in charger mode while the receiver rail is off.

## Independent protection and recovery

- The pack NTC controls charging through TS without firmware. A separate thermostat disables charging through CE and removes receiver power.
- TLV840 monitors SYS so USB-only operation remains possible. The undervoltage latch prevents repeated restarts when the unloaded pack voltage rebounds. POWER or USB clears it.
- Q13 and SW4 provide a QON system reset without opening the case.

## Interfaces and mechanics

- Wired UART keeps traffic and Wi-Fi control independent of the radio link.
- Maintained MUTE and Wi-Fi sliders define requested state.
- MAX-M10S is soldered on the carrier.
- The display and alternative XIAO Plus share carrier signals. Fit one controller only.
- Modules remain socketed for the prototype.
- Four copper layers provide a ground reference, power distribution and heat spreading.
