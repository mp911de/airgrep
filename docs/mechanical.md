# Mechanical

## Baseline: placement B (decided 24 Sep 2026)

Derived from placement A (`mechanical/placement-A.png`), re-arranged after the power review. Front view, Y down. 115 × 90 mm, may grow a few mm for clearance; the 100 × 60 × 11 mm battery behind the board sets the minimum.

```
 top edge: GNSS SMA   868 SMA   1090 SMA   (3 NPTH bulkhead mounts)
 ┌──────────────────────────────────────┐
 │ GNSS        DISPLAY        ADSBee    │  ADSBee landscape 48 × 30,
 │ 30 × 38     26 × 51        48 × 30   │  its USB-C toward the right edge
 │             (centred)                │
 │ gauge                      POWER ZONE│  charger, boost, load switch, fuse
 │ buzzer                               │
 │ MUTE ◄      test pads      ▼ ▼ ▼     │  bottom right: POWER slider, USB-C, LED
 └──────────────────────────────────────┘
```

Hard rules:

1. All RF (GNSS, ADSBee, three bulkheads) at the top; both switching converters at the bottom right, as far from GNSS as the board allows (target 40 mm or more).
2. POWER slider, USB-C receptacle and the bi-colour charge/power LED grouped at the bottom right, all facing down (aircraft side clearance is often poor). USB-C is Hanbo MC-107DMS (LCSC C3039295, 16P USB 2.0, 5 A, HRO TYPE-C-31-M-12 land pattern), mounted so the shell front extends 1.2 mm past the board edge; the front shell tabs stay 0.55 mm inside the edge. Place the board edge about 0.5 mm inside the case wall; with a 1.5 to 2 mm wall the receptacle face ends 0.3 to 1.3 mm behind the outer surface, which a USB-C plug reaches. Chamfer the opening.
3. MUTE slider on the left side, next to the buzzer, for ergonomics.
4. Display centred, glass to the front, four header groups on 8.51 mm sockets, spacers take touch force.
5. Service shunts JP_PWR_ESP and JP_PWR_ADS reachable without unplugging a module; silk: "REMOVE BEFORE USB".
6. Test pads for VBUS, SYS, VBAT, 5 V, EN, THERM_OK. Silk labels BAT, NTC, THERM at J3, J2, J4.
7. Battery connector and pack fuse reachable after removing the rear cover; no pin or screw tip against the pouch.
8. The three antenna NPTH are field-test mounts only (no enclosure yet). Keep each at least 20 mm sideways from the GNSS SMA axis (x = 19.2) so an SMA adapter or short lead fits; ADSBee uses two U.FL pigtails from its upper-left corner.

Board is now 105 × 95 mm (drawing `mechanical/placement-B.png`, coordinates `placement-B.json`, generator `draw_placement_b.py`).

Coordinate rule for CAD: the drawing is Y-down front view. KiCad is also Y-down, so coordinates transfer directly. EasyEDA Pro was Y-up; that caused the revision C mirror.

## Sockets and assembly split (decided 24 Sep)

- Seven module sockets: through-hole female headers, 2.54 mm, single row, 8.5 mm height (Samtec SSQ or equivalent), one height for all modules. User solders.
- Battery, NTC and thermostat connectors: through-hole, user solders.
- Everything else SMD, JLCPCB assembled.

## Module standoffs (decided 24 Sep)

Each module is held by its socket(s) plus 8.5 mm threaded standoffs through the carrier, screwed from the carrier underside, module screwed from the top. Carrier holes:

| Module | Holes in carrier | Size | Note |
|---|---|---|---|
| GNSS | 2, top corners (6.54, 17.54) and (31.94, 17.54) | 3.2 mm, M3 | opposite the J5 socket |
| Display | 4, its own pattern (47.5/67.5, 41.7/64.2) | 3.2 mm, M3 | display holes are 2.5 mm: widen them to 3.2 mm at assembly, or use M2.5 screws into M3 standoffs. J6 support socket kept for the top-left corner |
| ADSBee | 4, (75.9/98.5, 18.1/58.5) | 3.2 mm, M3 | ADSBee holes are M2.5 (image estimate); widen at assembly. Verify positions with the real board before fab |

All 15 carrier holes are 3.2 mm (M3): five case mounts at the edges and ten module standoffs. Module holes smaller than M3 are widened at assembly. Antenna test mounts stay 6.5 mm. Decided 24 Sep.

## Stack (top to bottom)

Front cover with screen and button openings, independent of the display. Modules on 8.51 mm sockets. Carrier PCB. Clearance plus battery tray and insulating barrier. Battery, supported by the case. Rear cover.

## Not yet verified

Display header stack height versus its rear controller and microSD slot. ADSBee header positions and rear shield clearance. Coax bend radius from module SMA/U.FL to bulkheads. Barometer vent if fitted. Wi-Fi antenna keepouts for ADSBee and ESP32. GNSS active antenna bias current versus the ADSBee 200 mA 3.3 V output.
