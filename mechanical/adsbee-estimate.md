# ADSBee 1090U: preliminary image-derived geometry

**Suitable for placement studies only. Not verified for drilling or header mating.** User explicitly requested estimating geometry from the supplied two-sided render so the preliminary design can proceed.

## Result

Reference view: shield/component side, USB on the left. Origin is the upper-left corner of the rectangular board bounds. X increases rightward, Y downward. Coordinates below are rounded estimates in mm; the 2.54 mm pitch is sourced from the manufacturer's Rev D schematic.

| Feature | Preliminary geometry |
|---|---|
| PCB bounds | **48.0 x 30.0 mm**; image reconstruction gives 48.09 x 30.08 |
| Main 1x13 header | First pad **(9.4, 27.8)**; remaining pads advance +X at 2.54 mm pitch; span 30.48 mm |
| GNSS 1x5 header | First pad **(46.8, 9.9)**; remaining pads advance +Y at 2.54 mm pitch; span 10.16 mm |
| Mounting centres | **(4.5,3.9), (44.9,3.9), (4.5,26.5), (44.9,26.5)**; inferred rectangle 40.4 x 22.6 mm |
| USB / corners / drills | 3 mm USB overhang allocation; R3.5 corner placeholder; nominal M2.5 holes. These are reservations, not verified manufacturing dimensions. |

Main header labels in physical order: 5VIN, GND, TX, RX, 3.3V, IOA, IOB, IOC, CS, CLK, MOSI, MISO, GND. GNSS header labels: 3V3, TX, RX, PPS, GND. These are read from the render; physical ordinals must be reconciled with schematic numbering before electrical footprint release.

## Method and uncertainty

Two perpendicular header rows provide both axis scales and skew. This is stronger than scaling against a generic USB-C shell: shell, anchors and overhang vary between connector models. An affine reconstruction corrects the oblique CAD view, assuming the PCB plane is approximately parallel-projected.

Front view GNSS-first-pad offset from main-first-pad: (37.18,-17.93) mm. Rear view, independently reconstructed: (37.53,-17.85) mm. Their difference is about 0.35 mm in X and 0.08 mm in Y. Some corresponding mounting centres differ by about 0.54 mm. Manual pixel selection, rendered hole depth and perspective are not fully modelled. These discrepancies are a consistency check, not an accuracy certificate.

Use the rounded positions with a provisional +/-0.5 mm-or-larger offset allowance; reserve at least 1 mm extra mechanical clearance around the estimated board outline. None of this is precise enough to commit a carrier with two simultaneously mating rigid header rows. Verify their relative offset and the mounting holes against hardware, a dimensioned drawing or a 1:1 test template before fabrication. Do not snap offsets to a guessed grid merely because the pin pitch is 2.54 mm.

USB-C is only a gross sanity check here, not a second dimensional reference. Neither board height, rear shield projection, pin mating depth nor coax plug height can be established from this image; the rear shield in particular must clear the carrier.

## Placement impact

The 48 x 30 mm estimated board fits the accepted **35 x 50 mm allocation when rotated**. Placement drawing now shows its estimated holes and both headers. USB faces the bottom edge of the receiver allocation; the 3 mm overhang reservation reaches beyond that allocation and has only about 2 mm to the gauge reservation. Programming-cable access and actual connector envelope remain unresolved; a USB plug must not be assumed to fit that gap.

Reproduce with `python3 scripts/estimate_adsbee.py`, then `python3 scripts/draw_placement.py`. [JSON](adsbee-estimate.json) retains all manually selected pixel landmarks, raw reconstructions and the rounded model. [Original supplied render](../sources/adsbee-two-sided-reference.png) is preserved unchanged.
