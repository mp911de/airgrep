# I2C bus requirements

3.3 V, up to 400 kHz. Devices and datasheets are in the [address table](interfaces.md#i2c-address-map).

| Limit                                           | Requirement                                                             |
|-------------------------------------------------|-------------------------------------------------------------------------|
| Total capacitance, including display and wiring | At most 190 pF with the Plus, 200 pF with the original display          |
| SDA/SCL rise time                               | At most 250 ns, set by FUSB303B                                         |
| Low level                                       | At most 0.4 V, using the published 3 mA sink guarantee at 3.6 V pull-up |
| Effective parallel pull-up                      | At least 1.067 kΩ                                                       |

| Pull-ups         | Value                | Fitting                              |
|------------------|----------------------|--------------------------------------|
| R19/R20          | 1.5 kΩ, 1%           | Fitted, LCSC C22843 |
| Original display | 5.1 kΩ, assumed ±10% | On the display; absent with the Plus |
| R90/R91          | 5.1 kΩ               | DNP, optional 0805 hand-solder pads  |

## Capacitance estimate

Carrier traces are outer-layer microstrip over the inner ground planes, about 0.09 pF/mm. Pin values are datasheet maxima, 10 pF where the datasheet gives none.

| Contribution                                | SDA    | SCL   |
|---------------------------------------------|--------|-------|
| Carrier traces, vias, J3/J9 pads            | 16 pF  | 16 pF |
| MAX17048 U8                                 | 60 pF  | 60 pF |
| TCA9534 U13                                 | 9.5 pF | 8 pF  |
| FUSB303B U15                                | 5 pF   | 5 pF  |
| BQ25798 U4, BMP580 U10, unspecified         | 20 pF  | 20 pF |
| Controller module                           | 10 pF  | 10 pF |
| Original display: AXS5106L, LSM6DS3, wiring | 40 pF  | 40 pF |

Rise times:

* 154 ns with the Plus
* 162 ns with the original display

## Bench acceptance

Measure rise time and low level with the actual controller, display and wiring.

Limits: [NXP UM10204](../datasheets/um10204.pdf), [FUSB303B](../datasheets/fusb303b.pdf).
