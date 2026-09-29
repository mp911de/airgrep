# ADSBee 1090U: geometry estimated from an image

No dimensioned drawing or board CAD of the ADSBee 1090U is available. Its outline, header positions and mounting holes are therefore estimated from a two-sided render. The carrier sockets J10 and J11 and the holes MH12 and MH14 rest on this estimate.

The estimate is good enough for placement. It is not verified for header mating or drilling. Measure the delivered module before the carrier is ordered.

## Result

Reference view: component and shield side, USB on the left. Origin is the upper left corner of the board bounds, X right, Y down, mm. The 2.54 mm pitch comes from the manufacturer's Rev D schematic.

| Feature | Estimate |
|---|---|
| Board | 48.0 x 30.0 mm (reconstruction gives 48.09 x 30.08) |
| Main header, 1x13 | first pad at (9.4, 27.8), pads advance in +X, span 30.48 mm |
| GNSS header, 1x5 | first pad at (46.8, 9.9), pads advance in +Y, span 10.16 mm |
| Mounting centres | (4.5, 3.9), (44.9, 3.9), (4.5, 26.5), (44.9, 26.5), a 40.4 x 22.6 mm rectangle |
| USB | 3 mm overhang reserved on the left edge |
| Corners, drills | R3.5 corners and M2.5 holes, both assumed |

Header labels, read from the render:

- Main: 5VIN, GND, TX, RX, 3.3V, IOA, IOB, IOC, CS, CLK, MOSI, MISO, GND.
- GNSS: 3V3, TX, RX, PPS, GND.

## Method

The two perpendicular header rows give the scale and the skew on both axes. An affine reconstruction corrects the oblique view, assuming a parallel projection of the board plane. The USB-C shell is only a rough sanity check, because shells and overhangs differ between connector models.

Front and rear view are reconstructed independently:

| Quantity | Front | Rear | Difference |
|---|---|---|---|
| GNSS first pad relative to main first pad, X | 37.18 mm | 37.53 mm | 0.35 mm |
| GNSS first pad relative to main first pad, Y | -17.93 mm | -17.85 mm | 0.08 mm |
| Mounting centres | | | up to about 0.54 mm |

The differences are a consistency check, not a tolerance. Landmarks are picked by hand, and hole depth and perspective are not modelled.

## Limits

- Use the rounded positions with a planning allowance of 0.5 mm or more, and keep 1 mm of extra clearance around the outline.
- Two rigid header rows have to mate at once. Their relative offset is the critical dimension.
- Do not snap offsets to a 2.54 mm grid because the pitch happens to be 2.54 mm.
- Board height, rear shield projection, pin mating depth and coax plug height cannot be taken from the image. The rear shield must clear the carrier.

## On the carrier

The module sits portrait at the top right, rotated a quarter turn from the reference view. Its outline covers about x 72 to 102, y 15 to 63 on the carrier, with the USB edge at the bottom.

| Carrier feature | Position (X, Y) | From the estimate |
|---|---|---|
| J10 pin 1 (main header) | (99.8, 53.6) | anchor |
| J11 pin 1 (GNSS header) | (81.9, 16.2) | offset (37.4, -17.9) in the module view, as estimated |

The holes MH12 and MH14 do not use the rectangle estimated here. Their pattern and their distance from the estimated centres are in [docs/mechanical.md](../docs/mechanical.md#mounting).

## Files

- [adsbee-estimate.json](adsbee-estimate.json): the picked pixel landmarks, both raw reconstructions and the rounded model.
- [../sources/adsbee-two-sided-reference.png](../sources/adsbee-two-sided-reference.png): the render.

The script that produced the JSON is not in the repository.
