# Mechanical

Board outline, placement, mounting and the stack behind the display. Headline figures are in [specs.md](specs.md), connectors and pin maps in [interfaces.md](interfaces.md), hand-fitted parts in [bom-hand-sourced.md](bom-hand-sourced.md).

## Placement

![Carrier placement, front view](../mechanical/placement.png)

The drawing is generated from `hardware/carrier-ti-charger/carrier.kicad_pcb`, which is the source for every position. Regenerate it after moving parts, from the repository root, with KiCad's Python:

```
tools/kpy mechanical/draw_placement.py
```

The board is 105 x 95 mm. The 100 x 60 x 11 mm battery behind it sets the minimum size.

Placement rules:

- RF sits at the top: the GNSS module, the ADSBee and the three antenna mounts.
- Both switching converters sit toward the bottom right. Read current separation from the PCB before checking RF clearance.
- POWER slider SW1, LEDs D2 and D3 and the USB-C receptacle J14 line up on the bottom edge and face down. Side clearance in an aircraft is often poor.
- MUTE slider SW2 is on the left edge, next to the buzzer. Wi-Fi slider SW3 is above it on the same edge.
- The display sits in the middle column, glass to the front.
- The pack connectors J12, J11, J13 and the pack fuse F1 are at the bottom, left of centre, reachable with the rear cover off.
- Shunt JP1 is left of the display and stays reachable with the display fitted. JP3 lies under the lower right corner of the ADSBee outline.
- The antenna mounts are MH6 (GNSS), MH7 (868 MHz) and MH8 (1090 MHz). MH6 is 20.6 mm from the GNSS SMA axis at x = 19.3, so an SMA adapter or a short lead fits.
- The ADSBee is portrait, measured 29.86 to 29.89 x 47.46 mm. Its own USB-C faces down, above the boost converter. Its U.FL connectors are at the upper left corner.

Module outlines in the drawing follow the socket positions. U16 is the soldered MAX-M10S; ADSBee dimensions use [adsbee-estimate.md](../mechanical/adsbee-estimate.md), with approximate edge offsets. The display outline comes from manufacturer CAD (`mechanical/source-geometry.json`).

### Control sliders

SW1 to SW3 use HRO K3-1280S-F1, C92658, with the local `carrier:SW-SMD_K3-1280S-F1` footprint. It follows the [manufacturer drawing](../datasheets/hro-k3-1280s.pdf): 2.50 mm terminal pitch, 1.20 x 2.45 mm solder lands, two 0.90 mm holes spaced 6.80 mm, and 3.075 mm from the terminal-row centre to the peg line. The body is 9.10 x 3.50 mm in that drawing, 3.50 mm high, with a 2 mm F1 actuator and 2 mm travel.

Footprint origins are the middle terminal. Read switch positions and rotations from the PCB; allow full actuator travel and housing tolerance in the case. SW1-SW3 share the model documented in [local library sources](../hardware/carrier-ti-charger/lib/README.md). Supplier CAD has inconsistent F2/K1 names and peg offsets. Check an actual F1 sample and the JLCPCB placement preview before ordering.

### USB-C receptacle in the case

J14 is mounted so that the shell front extends 1.2 mm past the board edge. The front shell tabs stay about 0.5 mm inside the edge.

Place the board edge about 0.5 mm inside the case wall. With a 1.5 to 2 mm wall the receptacle face ends 0.3 to 1.3 mm behind the outer surface, which a USB-C plug reaches. Chamfer the opening.

## Assembly split

- JLCPCB assembles all SMD parts. They are on the top side.
- Hand-soldered through-hole parts: module sockets J1, J4, J5, J6, J8, J9, pack connectors J11, J12, J13, shunt headers JP1 and JP3, expansion header J10, LEDs D2 and D3. The parts are listed in [bom-hand-sourced.md](bom-hand-sourced.md).
- All six module sockets are 2.54 mm single-row female headers of one height, 8.5 mm.

## Mounting

All mounting hardware is M2.5. The carrier has four plated 2.7 mm case holes (KiCad stock `MountingHole_2.7mm_M2.5_Pad_Via`), three non-plated 2.7 mm module holes (`MountingHole_2.7mm_M2.5`) and three plated 6.5 mm antenna holes (`MountingHole_6.5mm_Pad`).

| Use               | References |
|-------------------|------------|
| Case              | MH1 to MH4 |
| Display           | MH13       |
| ADSBee            | MH12, MH14 |
| Antenna bulkheads | MH6ANTGNSS, MH7ADS1090, MH8ADS980 |

Hole centres are defined by the PCB, rather than copied here.

Notes on the module holes:

- Display: the other three module holes, at (67.53, 64.17), (67.53, 41.67) and (47.53, 41.67), are not drilled. The display rests on its four socket groups and the one standoff. The module CAD has 2.5 mm holes.
- ADSBee: MH12 and MH14 use the measured diagonal pair, rounded to a 22.9108 x 40.5130 mm pattern (902 x 1595 mil). The [measurement record](../mechanical/adsbee-estimate.md#physical-measurements-2026-10-08) includes the independent diagonal cross-check.
- Module standoffs are 8.5 mm nominal to match the sockets. Case standoff height depends on the enclosure.

Use `carrier.kicad_dru` and the current PCB for mounting-hole clearances. The former removable GNSS board's caliper measurements do not apply to U16.

## Insulator board

A plain board between the carrier and the battery keeps the through-hole pin tails off the pack. It is a separate KiCad project, `hardware/insulator/insulator.kicad_pcb`, generated from the carrier by `tools/make_insulator.py`.

| Property          | Value                                                                                      |
|-------------------|--------------------------------------------------------------------------------------------|
| Outline           | same as the carrier                                                                        |
| Material          | 0.8 mm FR-4, no copper                                                                     |
| Case holes        | MH1 to MH4, copied from the carrier                                                        |
| Antenna clearance | 11 mm holes at the three antenna bulkheads for the nuts and pigtails                       |
| Cable slot        | 15 x 7.5 mm with round ends, position derived by the generator from the carrier connectors |
| Silkscreen        | none on the carrier side; OSHW logo, BATTERY SIDE and CABLES on the battery side            |

Order it as a separate board in the same JLCPCB cart. It cannot share the four-layer panel.

The standoffs between carrier and insulator must clear the longest pin tail. Trim the hand-soldered tails to about 1.5 mm and use M2.5 spacers of at least 3 mm.

J12, J11 and J13 may go on either side of the carrier; both sides are labelled ([interfaces.md](interfaces.md#silkscreen-labels)). The prototype keeps them on top, with the leads around the board edge. On the back they need a larger gap: about 6 mm for the PH housing plus the mated plug. Verify slot size and position against the harness; SLOT_W and SLOT_H in the generator set its size.

## Stack

From front to back:

1. Front cover with openings for the screen and the buttons, independent of the display.
2. Modules on their sockets.
3. Carrier board.
4. Spacers and insulator board.
5. Battery, supported by the case.
6. Rear cover.

## Enclosure vents

Provide four 8 cm² vents: two lower inlets and two upper outlets, 16 cm² per group. These are geometric areas before the grille; the thermal model assumes 50 % free area and 35 mm vertical separation between the openings. Keep the internal airflow path clear of the battery, boards and wiring.

The model assumes a 115 x 105 x 40 mm enclosure with 2 mm printed walls. Geometry and cooling remain to be verified in the actual enclosure.

## Open checks

Mechanical checks that still need the delivered modules or the enclosure are listed in [open-issues.md](open-issues.md).
