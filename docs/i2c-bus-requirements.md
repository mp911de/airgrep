# I2C bus requirements

The active TI carrier uses a 3.3 V, 400 kHz I2C bus. R19 and R20 are 1.8 kΩ, 1%; the display adds 5.1 kΩ pull-ups, conservatively treated as ±10%. The estimated current capacitance is 145 pF and the layout design ceiling is 200 pF.

`Rp(min) = (Vpu(max) - VOL(max)) / IOL(min)`

`Rp(max) = tr(max) / (0.8473 × Cbus)`

| Case | Clock | Payload ceiling | Allowed or fitted pull-up | Worst rise time | Result |
|---|---:|---:|---:|---:|---|
| Known participants, 145 pF estimate | 400 kHz | 44.4 kB/s | 1.067 to 2.034 kΩ | 169 ns fitted | Pass |
| Known participants, 200 pF ceiling | 400 kHz | 44.4 kB/s | 1.067 to 1.475 kΩ | 233 ns fitted | Pass, 17 ns margin |
| R19/R20 1.8 kΩ plus display 5.1 kΩ | 400 kHz | 44.4 kB/s | 1.284 to 1.373 kΩ | 217 to 233 ns at 200 pF | Fitted range |
| FUSB303B device limit | 400 kHz | 44.4 kB/s | 160 Ω minimum from 20 mA IOL | 250 ns maximum | Pass |
| AXS5106L touch controller | Unknown | Unknown | IOL and pin capacitance unpublished | Unknown | Not provable from data sheet |

The 1.067 kΩ lower bound uses the weakest published 3 mA sink guarantee among the known participants at 3.6 V pull-up and 0.4 V low. The 1.475 kΩ upper bound uses the FUSB303B 250 ns Fast-mode rise-time limit at 200 pF. The fitted worst-high value is `1.818 kΩ || 5.61 kΩ = 1.373 kΩ`.

The electrical design therefore has datasheet margin at 400 kHz for every participant with published limits. Whole-bus compliance remains unproven because the AXS5106L data sheet omits I/O sink-current and capacitance limits; this is an explicit prototype measurement item, not a reason to reduce the designed bandwidth.

Sources: [NXP UM10204](../datasheets/um10204.pdf), [onsemi FUSB303B](../datasheets/fusb303b.pdf), and each participant data sheet listed in [the datasheet index](../datasheets/README.md).
