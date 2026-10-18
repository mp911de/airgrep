# I2C bus requirements

3.3 V, up to 400 kHz. Devices and datasheets are in the [address table](interfaces.md#i2c-address-map).

| Limit | Requirement |
|---|---|
| Total capacitance, including display and wiring | At most 200 pF |
| SDA/SCL rise time | At most 250 ns, set by FUSB303B |
| Low level | At most 0.4 V, using the published 3 mA sink guarantee at 3.6 V pull-up |
| Effective parallel pull-up at 200 pF | 1.067 to 1.475 kΩ |

| Pull-ups | Value | Fitting |
|---|---|---|
| R19/R20 | 1.43 kΩ, 1% | Fitted |
| Original display | 5.1 kΩ, assumed ±10% | On the display; absent with the Plus |
| R90/R91 | 5.1 kΩ | DNP, optional 0805 hand-solder pads |

Calculated worst rise time at 200 pF: **245 ns with the Plus**, **195 ns with the original display**, including initial resistor tolerance.

## Bench acceptance

Measure rise time and low level with the actual controller, display and wiring. Temperature drift and the AXS5106L's unpublished sink-current/capacitance limits remain unverified. Leave R90/R91 empty initially; before fitting any extra pull-ups, recalculate the parallel resistance. The original display configuration is already close to the minimum resistance.

Limits: [NXP UM10204](../datasheets/um10204.pdf), [FUSB303B](../datasheets/fusb303b.pdf).
