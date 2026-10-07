# ADSBee 1090U: measured geometry

The delivered module was measured with calipers on 2026-10-08. J11, MH12 and MH14 use the measured positions below.

## Physical measurements, 2026-10-08

View: components facing the observer, USB-C down. All readings are mm. Hole spans use inside jaws against the farthest walls. Subtract the two hole radii to obtain centre distance. Rounded PCB corners made some edge readings uncertain. Decimal places are recorded readings, not an accuracy claim.

| Measurement | Reading |
|---|---|
| Bare PCB width | 29.86 to 29.89 |
| Bare PCB height | 47.46 |
| Top-right mounting hole diameter | 2.72 |
| Bottom-left mounting hole diameter | 2.69 |
| Top-right mounting hole gap to right / top PCB edges | 2.10 / 2.15 |
| Top-left mounting hole gap to left PCB edge | 2.17 |
| Bottom-left mounting hole gap to left / bottom PCB edges | 2.18 / 2.14 |
| Header hole diameters, both rows | 0.98 |
| 13-hole row gap to right PCB edge | 1.60 |
| Lowest 13-row hole gap to bottom PCB edge | 8.08, provisional due to corner radius |
| Middle 5-row hole gap to top PCB edge | 1.28 |
| Leftmost 5-row hole gap to left PCB edge | 9.23, provisional due to corner radius |
| Full 13-hole row outer span | 31.46 |
| Full 5-hole row outer span | 11.15 |
| Topmost 13-row hole to rightmost 5-row hole | 11.59 |
| Topmost 13-row hole to leftmost 5-row hole | 20.48 |
| Top-right mounting hole to topmost 13-row hole | 7.03 |
| Top-right mounting hole to leftmost 5-row hole | 18.68 |
| Bottom-left mounting hole to bottommost 13-row hole | 26.80 |
| Bottom-left mounting hole to topmost 13-row hole | 44.99 |
| Large mounting-hole diagonal, independent cross-check | 49.30 |

Use nominal 2.54 mm header pitch. The measured centre spans are 30.48 mm and 10.17 mm, versus nominal 30.48 mm and 10.16 mm. Assume the two header rows are perpendicular. Trilateration from the direct hole spans gives these approximate centres, relative to the topmost 13-row hole (carrier J10 pin 13), X right, Y down:

| Feature | X | Y |
|---|---|---|
| Leftmost 5-row hole, J11 pin 1 by the existing pin map | -18.253 | -6.861 |
| Top-right mounting hole, MH12 | -1.532 | -4.948 |
| Bottom-left mounting hole, MH14 | -24.441 | 35.566 |

The inferred mounting pattern is 22.909 x 40.515 mm. It predicts a 49.248 mm outer mounting-hole diagonal, compared with the independent 49.30 mm reading. This 0.052 mm residual checks consistency, not absolute accuracy. Edge-based header offsets differ from the direct-hole solution by about 0.2 mm. Prefer the direct-hole solution for mating positions.

### Applied carrier positions

J10 pin 1 retains its existing (99.800, 53.600) mm anchor. Applied coordinates are rounded to whole mil:

| Ref | X, Y (mil) | X, Y (mm) |
|---|---|---|
| J11 pin 1 | 3211, 640 | 81.5594, 16.2560 |
| MH12 | 3869, 715 | 98.2726, 18.1610 |
| MH14 | 2967, 2310 | 75.3618, 58.6740 |

Caliper checks do not verify socket mating, rear-shield clearance, connector overhang or the assembled stack. These remain assembly checks in [open-issues.md](../docs/open-issues.md#bench-tests-at-bring-up).

The placement drawing uses the measured maximum width, 29.89 mm, and height, 47.46 mm. Its origin is approximately (-27.80, -39.11) mm from J10 pin 1, using the measured right-edge gap and J11's top-edge gap. Connector overhangs remain estimates.
