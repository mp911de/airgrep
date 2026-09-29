# Portable ADS-B / FLARM receiver carrier PCB

Status 29 Sep 2026: requirements and power architecture settled (standalone MAX77757 charger with on-chip USB-C and BC1.2 current detection; replaced the BQ25606 on 29 Sep, see docs/open-issues.md item 0), KiCad schematic entered (7 sheets, ERC 0 errors), all part connections checked against datasheets, power budget and runtimes in docs/specs.md (tools/power_budget.py), hand-sourced parts in docs/bom-hand-sourced.md. PCB 105 × 95 mm, 2 layers, fully routed 25 Sep: boost converter re-placed for short loops, power paths hand-drawn (VSYS/VBAT/BAT_IN 1.0 to 1.2 mm, charger and boost switch nodes and boost output 1.0 mm, VBUS 1.0 mm with one bottom-layer run), signals autorouted, GND pour on both layers with stitching, thermal vias under U6 (12), U7 (2). The charger area was re-netted for the MAX77757 on 29 Sep and is not yet re-routed: 35 unconnected items, no other DRC errors (drc-routed.json is the 25 Sep report). Board and schematic nets match. Silkscreen tidied 25 Sep (`carrier-silkscreen.png`). Not yet done: JLCPCB BOM/CPL/Gerber export. No order.

## Where things are

| Path | Content |
|---|---|
| `docs/requirements.md` | What the device must do. Read first. |
| `docs/power.md` | Power architecture: charger, boost, switch, thermostat, gauge |
| `docs/interfaces.md` | Pinouts of the plug-in modules and carrier connectors |
| `docs/bom-hand-sourced.md` | Parts to buy and hand-fit (modules, battery, sensors, THT parts, harness, antennas, mechanics) with order links |
| `docs/specs.md` | Technical data for users and resellers: radio, charging, battery, power budget and runtimes, EU compliance checklist |
| `specs/power_params.json`, `tools/power_budget.py` | Power model inputs and the script that regenerates the power, runtime and charging tables in `docs/specs.md` |
| `docs/mechanical.md` | Board outline, module placement, battery, antennas, enclosure |
| `docs/open-issues.md` | Everything not yet verified |
| `hardware/kicad/` | KiCad 10 project `carrier.kicad_pro`; sheets: usb_input, charger, battery_control, boost_switch, modules, gauge_status, buzzer_baro; `lib/` holds JLCPCB-derived symbols, footprints and 3D models; `carrier-schematic.pdf` is the schematic print, `carrier-top.png` / `carrier-bottom.png` the routed board, `carrier-silkscreen.png` the printed silkscreen, `drc-routed.json` the DRC report |
| `mechanical/` | Placement B drawing (`placement-B.png`, generator `draw_placement_b.py`), placement A for history, extracted module geometry (JSON) |
| `datasheets/` | Manufacturer PDFs plus `.txt` extractions, `manifest.json` has URLs and hashes |
| `sources/` | Manufacturer CAD: display KiCad project, GNSS and gauge Eagle boards, ADSBee schematic text, Rider CAD |
| `archive/` | Superseded work. Do not build on it. |

## Archive contents

- `archive/rev-ab-rider/`: revision A/B. LiPo Rider Plus based, 4-sheet schematic, 45 parts, EasyEDA project, old notes, original handoff.
- `archive/rev-c-easyeda/`: revision C. BQ25895 based, 9-sheet schematic, 100 parts. The schematic passed EasyEDA DRC. All its PCB files are vertically mirrored (Y-down coordinates used on EasyEDA's Y-up canvas) and are unusable. Sheet SVGs, `netlist.json` and `bom-review.csv` under `revision-c/` remain the reference for re-entering the non-power sheets.

## Tools

KiCad 10.0.6 with MCP bridge (used from Claude Code). EasyEDA Pro 3.2.149 installed but no longer used. Python 3.14 with pypdf.
