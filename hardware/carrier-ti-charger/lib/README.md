# Local library sources

`VSON-8_3.3x3.3mm_CSD17575Q3` derives from KiCad's `Package_SON:VSON-8_3.3x3.3mm_P0.65mm_NexFET`, licensed under [CC BY-SA 4.0 with the KiCad libraries exception](https://www.kicad.org/libraries/license/). The source and gate pads use 0.65 mm pitch and 0.48 mm width to satisfy the board's 6 mil pad clearance; the silkscreen is removed. Pinout and land geometry were checked against `datasheets/csd17575q3.pdf`, pages 1 and 9.

`X2QFN-12_L1.6-W1.6-P0.40-TL` (U15, FUSB303BTMX) comes from the LCSC/EasyEDA library. Its 0.2 x 0.505 mm pads match onsemi's 0.2 x 0.5 mm recommended land pattern. The corner silkscreen and inner pin-1 dot are removed because they sat within 5 mil of the pads; the outer pin-1 dot moved to 0.2 mm from pad 1. The courtyard is ±1.15 mm.

`carrier.3dshapes/R_0402_1005Metric.step`, `R_0603_1608Metric.step` and `R_0805_2012Metric.step` are copied unchanged from KiCad's `Resistor_SMD.3dshapes`, licensed under [CC BY-SA 4.0 with the KiCad libraries exception](https://www.kicad.org/libraries/license/). Every resistor on the board references them through `${KIPRJMOD}`.

`R_0805_2012Metric_NoSilk` derives from KiCad's `Resistor_SMD:R_0805_2012Metric`, licensed under [CC BY-SA 4.0 with the KiCad libraries exception](https://www.kicad.org/libraries/license/). Silkscreen graphics are removed; pads and courtyard retain the standard dimensions.

`carrier.3dshapes/IND-SMD_L4.2-W4.2-H3.2.step` for L1 and L2 is copied unchanged from the JLCEDA/EasyEDA Official Library, linked to [LCSC C5370906](https://www.lcsc.com/product-detail/C5370906.html), model UUID `9970d003d7fd4869a8b6b8cbcc773af1`. Copyright and related rights belong to JLCEDA/EasyEDA. The library model spans about 4.28 x 4.28 x 3.2 mm including terminals; it uses a 90 degree Z rotation to align its terminals with the footprint pads and a 0.1 mm Z offset to place its bottom on the board.

`carrier.3dshapes/SW-SMD_K3-1280S-F1.step` for SW1-SW3 is copied unchanged from the [JLCEDA/EasyEDA Official Library](https://lceda.cn/), linked to [LCSC C92658](https://www.lcsc.com/product-detail/C92658.html), model UUID `ba464178ae33423b83adb82f1ac4f72a`. Copyright and related rights belong to JLCEDA/EasyEDA. The supplier names the file K3-1280S-K1, but the model has the F1 drawing's 9.1 x 3.5 mm body, 2 mm actuator extension and 6.8 mm peg spacing. The shared footprint and board use offset (0, -3.075, 1) mm and Z rotation 90 degrees. Alignment is checked against the current footprint; the supplier model is a visualization asset, not manufacturing acceptance.
