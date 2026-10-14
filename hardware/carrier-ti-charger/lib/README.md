# Local library sources

`VSON-8_3.3x3.3mm_CSD17575Q3` derives from KiCad's `Package_SON:VSON-8_3.3x3.3mm_P0.65mm_NexFET`, licensed under [CC BY-SA 4.0 with the KiCad libraries exception](https://www.kicad.org/libraries/license/). The source and gate pads use 0.65 mm pitch and 0.48 mm width to satisfy the board's 6 mil pad clearance; the silkscreen is removed. Pinout and land geometry were checked against `datasheets/csd17575q3.pdf`, pages 1 and 9.

`X2QFN-12_L1.6-W1.6-P0.40-TL` (U15, TUSB320L) comes from the LCSC/EasyEDA library. The corner silkscreen and the inner pin-1 dot are removed because they sat within 5 mil of the pads; the outer pin-1 dot moved to 0.2 mm from pad 1. The courtyard is widened to ±1.15 mm, since the original ±0.8 mm sat inside the pads.

`carrier.3dshapes/R_0402_1005Metric.step`, `R_0603_1608Metric.step` and `R_0805_2012Metric.step` are copied unchanged from KiCad's `Resistor_SMD.3dshapes`, licensed under [CC BY-SA 4.0 with the KiCad libraries exception](https://www.kicad.org/libraries/license/). Every resistor on the board references them through `${KIPRJMOD}`.

`R_0805_2012Metric_NoSilk` derives from KiCad's `Resistor_SMD:R_0805_2012Metric`, licensed under [CC BY-SA 4.0 with the KiCad libraries exception](https://www.kicad.org/libraries/license/). Silkscreen graphics are removed; pads and courtyard retain the standard dimensions.
