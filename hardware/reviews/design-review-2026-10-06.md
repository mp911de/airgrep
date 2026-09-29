# Carrier design review, 6 Oct 2026

**Project:** `hardware/kicad/carrier.kicad_pro` (KiCad 10.0.6, 7 sheets, 4-layer PCB 105 x 95 mm)
**State reviewed:** working tree on top of `cf7742e`, with uncommitted changes (LM66100 swapped for LM73100 on U11/U12, R44 to R47 added, U11/U12 placed on the PCB)
**Analysis run:** `analysis/2026-10-06_2137/` (kicad-happy 2.2.1)

## Verdict

**Not ready to order.** The new LM73100 stage is wired correctly and its footprint matches TI's land pattern. U12 has no input capacitor, though, and the production files predate both the LM73100 change and the U8 footprint fix. The board-level blockers in `docs/open-issues.md` are still open (reverse polarity at J3, silkscreen, J3 rating, production export).

## Critical findings

| Severity | Issue | Basis |
|---|---|---|
| BLOCKER | `jlcpcb/` Gerbers, BOM and CPL date from 3 Oct. They have no U11, U12 or R44 to R47 and the old U8 footprint. Export again (the open-issues list covers this; the list of affected parts is now longer). | Raw file: BOM/CPL have no U11/U12; gerber dated 3 Oct |
| BLOCKER | Reverse-polarity protection at J3 is still not in the design ([issue 0002](../../issues/0002-battery-reverse-polarity-protection.md)). | Docs |
| WARNING | **U12 (LM73100) has no input capacitor nearby.** C24 is the only capacitor on RAIL_5V; it is 63 mm away at U7. TI asks for ≥ 0.1 µF ceramic from IN to GND as close as possible. For U11 it is 6.7 mm away. | Datasheet SNOSDC0A p.41 §10.1; PCB JSON |
| WARNING | The power simulation (`run_power.py`) does not model U11/U12. The load-step minimum of 4.82 V at the module terminal loses about 15 to 45 mV to the diode (VFWD 16.4/28.4 mV typ/max, 28 to 45 mΩ), which leaves about 30 to 55 mV above the assumed 4.75 V window. The heat model already includes them. | Datasheet p.6 VFWD; `hardware/sim/power/results.md` |
| WARNING | Prior per-IC findings still open: U4 footprint smaller than the Maxim land pattern, U4 BYP 10+10 µF vs 10+22 µF, U6 has no local 100 nF on VIN (C12 is 32.7 mm away), U5 C12 loop, U7 EP paste about 50 %. | Carried over from `ic-layout-2026-10-06/recheck-57b4e54e`, distances measured again |

## Changes since HEAD and the prior review

| Item | Status |
|---|---|
| U11/U12 LM66100 (SC-70, 1.5 A) → LM73100RPWR (VQFN-HR 2x2, 5.5 A), LCSC C3210761 (Extended, 4058 in stock) | New. Reviewed below |
| R44/R45, R46/R47 1 MΩ/1 MΩ EN/UVLO dividers from RAIL_5V | New |
| U11/U12 footprints on the PCB (HEAD board had none; re-check parity listed them as missing) | New. DRC parity is now 0 |
| U8 footprint now 0.70 x 0.30 mm lands, 1.98 mm rows, 0.80 x 1.38 mm EP (Maxim 90-0065 E) | **Fixed** (u8-fix) |
| U10 traces between pads under the sensor body | **Fixed**: no F.Cu segment inside the body |
| U5 R13 pull-up placement, U9 ground stitching, U9 BUZZ_PWM crossing | Fixed (per the re-check) |
| U4 (5 items), U6 VIN 100 nF, U5 C12, U7 paste/mask, U8 I²C rise time, U10 supply ramp | Still open, unchanged |
| C17 moved 1.4 mm, TP7 moved 1.5 mm | Layout only |

## LM73100 deep review (U11 ADSBee, U12 display)

Topology: `U7 → RAIL_5V (C24 10 µF) → U11/U12 IN → OUT → JP2/JP1 → ADS_5V/ESP_5V (C26/C25 10 µF) → modules`. The diodes keep a module's own USB port from back-feeding RAIL_5V.

| Check | Result | Basis |
|---|---|---|
| Pin map 1 EN/UVLO, 2 OVLO, 3 PG, 4 PGTH, 5 IN, 6 OUT, 7 DVDT, 8 GND, 9 IMON, 10 DNC | Matches the symbol, schematic and PCB pads | **Datasheet-verified**: Fig. 5-1 / Table 5-1, p.3 |
| Footprint: side pads 0.6 x 0.25/0.30 at ±0.9 mm, corner stubs 0.25 x 0.65 at ±0.725/±0.875, IN/OUT 0.3 x 2.4 at ±0.25 | Matches RPW0010A land pattern on every dimension | **Datasheet-verified**: p.50 |
| Paste: footprint pastes 100 % of each pad | TI stencil splits IN/OUT into two windows at 82 % | Datasheet p.51. Suggestion |
| EN/UVLO: 1 MΩ/1 MΩ → 2.53 V at 5.06 V; UVLO rising 2.4 V | Inside the 5 V pin limit. The datasheet recommends a ≥ 350 kΩ pull-up above 5 V, so the divider is appropriate. The 2.4 V UVLO sits below the internal 2.53 V UVP and adds nothing. 5 µA leakage in total | Datasheet p.4 note 2, p.6 |
| OVLO to GND | Always enabled (active-low enable at 0 V, same as the EC test condition) | Datasheet p.3, p.6 |
| IMON to GND, DNC open, DVDT open, PG open, PGTH to GND | Allowed. DVDT open gives the fastest slew (about 12 V/ms at 2.7 V, 28 V/ms at 12 V), but RAIL_5V already rises over the U7 17 ms ramp | Datasheet p.3, p.7 |
| Reverse blocking with the carrier off and a module on USB | Blocked; unpowered reverse leakage is in the µA range | Datasheet p.6 |
| Input capacitor | **Missing at U12** (see Critical findings) | Datasheet p.41 |
| Output capacitor ≥ 1 µF at OUT | None at OUT; C25/C26 sit behind the shunts. Acceptable with the shunt fitted; optional 1 µF | Datasheet p.41 |
| IN/OUT copper | 0.30 mm traces and one 0.3 mm via each; TI asks for power-path copper sized for 2x full load. Losses are 4 to 36 mW (heat sim), so this is margin only. Widen ADS_5V_D (ADSBee, up to 1 A peaks) | Datasheet p.41; PCB JSON |
| Thermal | 23 mW typ / 36 mW max per pair at the worst case in `sim/heat/results.md`. No concern | Project heat sim |

`analysis/deep_review.json`: 4 findings passed the citation gate, 0 quarantined.

## Analyzer results and triage

| Analyzer | Run | Result |
|---|---|---|
| `analyze_schematic.py` | yes | 113 components, 98 nets; count and U11/U12/R44 to R47 pin nets checked against the raw files |
| `analyze_pcb.py --full --proximity` | yes | Footprint diff vs HEAD matches the schematic diff |
| KiCad DRC (with parity) and ERC, `kicad-cli` 10.0.6 | yes | **0 violations, 0 unconnected, 0 parity; ERC 0** |
| `cross_analysis.py` | yes | 5 findings, triaged below |
| `analyze_emc.py` | yes | 58 findings, triaged below |
| SPICE (ngspice, with PCB parasitics) | yes | 27 pass, 1 warn (false positive) |
| `analyze_thermal.py` | yes, no output | Skipped every part: no MPNs or extraction cache. The project's own `run_heat.py` covers this |
| `analyze_gerbers.py` | yes | Layer set complete and aligned, but the files are stale (see blockers) |
| Lifecycle audit | no | The schematic uses LCSC numbers, not MPNs (2 of 41 lines have an MPN). All 41 BOM lines have an LCSC number. A spot check found U11/U12 in stock |

### False positives / reviewer overrides

- **KO-001 (48, high):** FINE_*/VIA_U4 are net-class rule areas with no restrictions. MHKEEP areas contain their own holes. "No fill" allows footprints. None is a keepout breach. The FINE_U10 areas copied to U11/U12 correctly relax the 12 mil spacing at the 0.45 mm pitch.
- **RT-001 (4):** DVDT/DNC on U11/U12 are two overlapping pad primitives with one number (the L-shaped corners). KiCad reports them as connected.
- **PU-001:** U11/U12 PG has no pull-up because it is intentionally unused.
- **UC-003:** no external 5.1 kΩ on CC1/CC2. The MAX77757 integrates the UFP pull-down (RPD_UFP 5.1 kΩ ±10 %, datasheet p.8).
- **RS-001:** BOOST_VCC, CHG_VDD are IC-internal LDO outputs; PWR_EN is driven by U5/R13.
- **SS-001:** MPN coverage. The project sources by LCSC number, and coverage is 100 %.
- **SU-001 (adjacent signal layers):** In1 is a solid GND plane and In2/B.Cu are GND pours. The analyzer misreads the stackup.
- **DC-002 U12:** real (above).
- **SPICE R8/C34 "snubber" warn:** this is a slow RC (τ 0.34 s), not a snubber. The regulator-feedback check used Vref 0.6 V. With the TPS61088's 1.204 V (datasheet), R14/R15 give 5.06 V, matching `power_params.json`.
- **PM-002 SW1/SW2/J1 edge overhang:** intended edge-mounted parts.
- **DFM-001/002 annular 0.076 mm:** the VIA_U4 18/12 mil vias are deliberate and inside JLCPCB multilayer limits (rule comment in `carrier.kicad_dru`).
- **GP-001/RP-001/RP-002/PS-002 (plane gaps, layer transitions):** affected nets are DC rails, LEDs, UART, I²C at 100 kHz and the USB CC/D± lines, which carry only BC1.2/Type-C detection to U4 and no USB data. Low risk. The GND "islands" are bridged by the solid In1 plane (DRC 0 unconnected).
- **VP-001 U7:9 / L1:1:** known. The bring-up checklist already covers the U6/U7 joint inspection.

## Documentation drift (from this change)

- `docs/power.md` and `docs/interfaces.md` still describe `RAIL_5V → JP1/JP2 → module` and never mention U11/U12. The path-resistance table in `power.md` (RAIL_5V "U7 to JP2") is now U7 to U11 plus ESP_5V_D/ADS_5V_D.
- `datasheets/README.md` and `manifest.json` list neither `lm73100.pdf` nor `lm66100.pdf`. The LM66100 PDF is still in the folder but no longer used.
- `run_heat.py` takes RAIL_5V resistance only to U11 pin 5; the 63 mm branch to U12 is not in the copper loss.

## Recommendations, in order

1. Add a 0.1 to 1 µF 0603 at U12 IN-GND (and preferably U11), close to pins 5/8.
2. Widen ADS_5V_D (and the U11 RAIL_5V stub) to ≥ 0.6 mm; optionally add 1 µF at each OUT.
3. Add the diode drop to `run_power.py` and re-run the load-step and pulse cases against 4.75 V.
4. Update `power.md`, `interfaces.md`, `datasheets/README.md` and the manifest for the LM73100.
5. Optional: set the U11/U12 paste to the TI split windows (82 %).
6. Then the existing pre-order list: J3 reverse polarity, silkscreen, fill/DRC, export with `fix_cpl.py`, schematic PDF.

## Not performed / limits

- Lifecycle/temperature audit: the BOM uses LCSC numbers, not MPNs. Only U11/U12 were looked up.
- Datasheet pin re-verification of U4 to U10 was not repeated. The 6 Oct per-IC review and re-check are the basis, and I re-measured only distances and footprints.
- No datasheets exist for L1, D3, D4, D5, BZ1 or the thermostat (`datasheets/README.md`); those parts are consistency-checked only.
- `analyze_thermal.py` produced no estimates; thermal conclusions rest on the project's `run_heat.py`.
- Module-side geometry (ADSBee) still rests on the image estimate; this review does not change that.
