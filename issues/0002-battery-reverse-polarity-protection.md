# Reverse-polarity protection at J3

Status: planned
Created: 2026-10-04

## Decision

Use TI CSD25404Q3 P-channel MOSFETs for a charging-compatible reverse-polarity protection stage at J3. The selected arrangement is two back-to-back devices in the positive battery path, after F1 and before all VBAT circuitry. Retain F1 for overcurrent protection. No reversed-polarity LED.

The MOSFET selection is settled; polarity detection, gate drive and the complete circuit remain to be designed and validated. This protection is not yet in the schematic or PCB.

## Selected part

- TI CSD25404Q3, 20 V P-channel, SON package in the 3 x 3 mm class; use the manufacturer's land pattern.
- LCSC [C2865523](https://www.lcsc.com/product-detail/C2865523.html). JLCPCB assembly availability and part class remain to be checked.
- Maximum on-resistance: 12.1 mΩ at VGS = -2.5 V and 25 °C.
- Typical gate charge: 10.8 nC at VGS = -4.5 V, VDS = -10 V and ID = -10 A.
- Reference: [TI datasheet](https://www.ti.com/lit/ds/symlink/csd25404q3.pdf).

Two devices give 24.2 mΩ using the 25 °C maximum resistance rating. Calculated conduction losses, excluding control circuitry and PCB copper:

| Battery current | Voltage drop | Loss in both MOSFETs |
|---|---|---|
| 1.8 A | 44 mV | 78 mW |
| 2 A | 48 mV | 97 mW |
| 3 A | 73 mV | 218 mW |

Resistance rises with temperature. These are calculations, not simulation or bench results.

## Circuit requirements

- Allow charging and discharging with a correctly connected pack.
- Isolate a reversed pack with USB absent, already connected, or connected after the pack.
- Detect polarity on the connector side; charger-generated voltage must not enable a reversed pack.
- Provide adequate gate drive near the discharge cut-off and active turn-off for reversed-pack hot-plug. Check gate and drain voltage limits, transient response and safe operating area.
- Preserve USB-only operation and define recovery for a deeply discharged pack or an open pack PCM. A back-to-back pair has no unconditional body-diode charging path.
- Keep the gauge and other VBAT branches behind the protection. Account for the extra resistance in charging, gauge readings, undervoltage and heat calculations.
- Coordinate with [0001: off-state battery disconnect](0001-battery-disconnect-switch.md). If a stage serves both functions, valid polarity must take precedence over slider and USB turn-on requests. Do not reuse that issue's direct gate pulls or turn-on capacitor without reviewing fault turn-off behavior.

## Completion checks

- [ ] Design polarity detection and gate drive, including startup and depleted-pack recovery.
- [ ] Verify normal and reversed insertion for both USB connection orders, charger operation, and battery discharge.
- [ ] Check transient voltages and MOSFET pulse stress; verify fault response on a current-limited bench setup.
- [ ] Update schematic, PCB, power/heat models and affected documentation; run ERC and DRC and regenerate production files.
