# MAX77972 as charger and fuel gauge

Status: open
Created: 2026-10-04

## Motivation

The display cannot see what the charger does. It has no free GPIO for STAT and INOKB, and the MAX17048 reports voltage-based state of charge only. The firmware therefore cannot show why charging has stopped, how much current flows or how warm the pack is.

The MAX77972 keeps the MAX77757's way of working: autonomous, Type-C CC and BC1.2 detection on chip, 4.5 to 13.7 V input, no PD. It adds a ModelGauge m5 fuel gauge with current measurement and an I2C interface. It would replace U4 (MAX77757) and U8 (MAX17048).

This is a proposal for a later revision. It does not block the current board order.

## Options considered

| Option | Runs without a host | Source detection | Package | Result |
|---|---|---|---|---|
| MAX77757 + MAX17048 (current design) | yes | CC and BC1.2 | FC2QFN-24, 0.4 mm | works; no charger state or current on I2C |
| MAX77757 + I2C GPIO expander for STAT, INOKB, THERM_OK | yes | CC and BC1.2 | adds one small part | gives the states, not current or temperature |
| MAX77818 | no, 500 mA input limit and 4 h timer until I2C sets them | none | WLP-72, 0.4 mm | rejected: needs a Type-C controller and firmware in the charge path |
| MAX77840 | no, 500 mA default input limit | BC1.2 only | WLP-81, 0.4 mm | rejected: no CC detection, the BQ25606 problem again ([decisions.md](../docs/decisions.md)) |
| MAX77972 | yes, charge current and voltage by resistors | CC and BC1.2 | WLP-36, 0.5 mm | this issue |

## What is known

Basis: ADI product page and data sheet extracts, plus one third-party field report. The full data sheet has not been read yet; it could not be downloaded when this issue was written. Items marked (field report) come from the third-party source only.

| Item | Value |
|---|---|
| Input | 4.5 to 13.7 V |
| Charge current | up to 3.15 A, set by a resistor on ICHG or by I2C |
| Charge voltage | set by a resistor on VCHG or by I2C |
| Source detection | Type-C CC and BC1.2 on D+/D-, sets the input current limit; no PD |
| Power path | Smart Power Selector, supplement mode |
| Fuel gauge | ModelGauge m5: coulomb counter plus voltage model |
| Current sense | internal, equivalent to 10 mΩ; an external resistor is optional |
| Temperature | thermistor on THM, read by the gauge; JEITA with nine regions, programmable by I2C; step charging |
| Reverse boost (OTG) | present; not used here |
| Package | MAX77972EWX+, 36-bump WLP, 0.5 mm pitch (outline 21-100517) |
| I2C addresses | 0x36 and 0x37 (field report) |
| Charger enable | CHGEN pin (field report) |
| Gauge start-up | needs an init sequence and a cell model after power-on reset; the registers are volatile (field report) |

## Plan

```
USB-C -- TVS -- MAX77972 CHGIN   (CC and D+/D- detection on chip, no PD)
                    |
battery -- fuse -- MAX77972 BATT -- SYS -- TPS61088 boost -- TPS22975 -- 5 V rail
                    |   (gauge and current sense inside the charger)
                    +-- I2C -- display ESP32
NTC -- MAX77972 THM
THERM_OK -- MAX77972 CHGEN   (replaces Q2, Q6, R12)
```

- The charger stays autonomous. Charging, the input current limit and the temperature cut-off must work with the display unplugged and with JP1 removed.
- ICHG is set for 1.8 A and VCHG for 4.20 V with resistors.
- The reverse boost is disabled in hardware, as ENBST is today.
- The thermostat drives CHGEN. The THM pull-down through Q6 is no longer needed; the load enable chain through SW1 stays as it is.
- The display reads state of charge, current, pack temperature and charger state on the existing bus at J8. U8 is removed; the gauge keeps address 0x36.
- The display writes the pack capacity (5 or 10 Ah) and the cell model after a gauge reset. This goes beyond requirement 7 in [requirements.md](../docs/requirements.md), which lets the ESP only read the state of charge. Settle this with the user first.
- D4 keeps its meaning. Whether the part has STAT and INOKB outputs like the MAX77757 is not known yet; without them the LED drive needs a new source.
- The boost, the load switch, the undervoltage latch and the input protection do not change.

## Open data sheet checks

These decide whether the plan holds. Add the PDF to `datasheets/` with a manifest entry and a text extraction, then answer each one.

- [ ] Behaviour with no host: default input current limit per detected source, default JEITA thresholds, default thermistor pull-up and beta. The hot cut-off must stay at or below 45 °C over all tolerances with the Variohm 100 kΩ thermistor.
- [ ] ICHG and VCHG resistor tables; a value for 1.8 A and 4.20 V.
- [ ] CHGEN polarity and levels against THERM_OK (VSYS level, 1 MΩ pull-down).
- [ ] Fast-charge timer options. The MAX77757's 6 h is tight for the 10 Ah pack at 1.8 A.
- [ ] Dead-battery behaviour on CC with an empty or missing pack.
- [ ] Status outputs for D4.
- [ ] Battery quiescent current with the gauge running, and whether a ship mode exists and how it is entered. The MAX17048 takes 3 µA in hibernate; a coulomb-counting gauge takes more.
- [ ] What the gauge does without a host after a reset, and whether its configuration survives while the pack stays connected.
- [ ] I2C pins with the bus unpowered. The pull-ups are on the display's 3.3 V, which is off with the slider off.
- [ ] Address clash with the display's touch controller and IMU on 0x37.
- [ ] Switching frequency, inductor range, switch resistances, junction-to-board figures and the thermal regulation temperature.
- [ ] SYS minimum voltage against the 2.9 V supervisor and the boost input range.

## Impact

| Area | Change |
|---|---|
| Schematic | charger sheet redrawn; gauge removed from its sheet; Q2, Q6, R12 removed if CHGEN works as planned |
| Layout | new charger area; the 36-bump WLP needs a check of JLCPCB's via and track rules on the four-layer stackup |
| Heatsink | open again; the WLP has no flat lid like the FC2QFN and conducts through its bumps |
| Simulation | `run_heat.py` loss inputs for the charger, `run_thermal.py` thresholds, `thermostat_trip.cir` |
| Power budget | standby table in `specs/power_params.json` |
| Firmware | gauge driver for ModelGauge m5 (16-bit registers), gauge init, charger state on the display |
| Sourcing | no LCSC number found yet; the part may need global sourcing or consignment |
| Documents | [power.md](../docs/power.md), [specs.md](../docs/specs.md), [interfaces.md](../docs/interfaces.md), [decisions.md](../docs/decisions.md) |

## Related issues

- [0001: battery disconnect switch](0001-battery-disconnect-switch.md). A switch in the battery lead removes the gauge's supply with the slider off. The coulomb count and the learned state are then lost at every power-off, which costs most of what the m5 gauge adds. Either keep the gauge on the pack side, or use the charger's own battery switch if a ship mode can be entered without firmware.
- [0002: reverse-polarity protection](0002-battery-reverse-polarity-protection.md). The MOSFET pair sits between pack and BATT. The gauge then measures the voltage behind it, 44 mV low at 1.8 A.

## Completion checks

- [ ] Read the data sheet and close the checks above. Drop the issue if the no-host behaviour does not meet the requirements.
- [ ] Confirm sourcing and JLCPCB assembly of the WLP package.
- [ ] Agree the change to requirement 7 with the user.
- [ ] Redraw the charger sheet and the layout; run ERC, DRC and schematic parity.
- [ ] Rerun the heat and thermal simulations and update the documents listed above.
- [ ] Bench: the charger checks in [open-issues.md](../docs/open-issues.md), repeated with the display unplugged.
