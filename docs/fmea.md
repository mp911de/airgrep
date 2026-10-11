# Design FMEA: carrier board and battery harness

Failure modes and effects of the rev A carrier (working tree of 8 Oct 2026) with its pack, NTC and thermostat harness. The design is in [power.md](power.md), [interfaces.md](interfaces.md) and [mechanical.md](mechanical.md). The ranked result is at the end: [Top 10 risks](#top-10-risks).

## Basis

Each row names its basis:

| Tag | Meaning |
|---|---|
| [S] | Checked in the schematic netlist (`kicad-cli` export of 8 Oct) or on the board file |
| [D] | Checked in the data sheet text in `datasheets/pdf2text/` |
| [Sim] | Taken from `hardware/sim/*/results.md` |
| [Doc] | Taken from the project docs, not checked again |
| [A] | Assumption. Not verified |

Owner context: the cockpit stays above 0 °C, so cold charging is out of scope. Sun-heated enclosures above 50 °C are realistic. Module sockets are a rev A choice. Rev B may solder the modules or integrate the GNSS chip.

Not in scope: module internals, antennas, firmware logic beyond the mitigations named here, the enclosure material. Note that a PLA enclosure softens near 55 to 60 °C [A].

## Scales

Cockpit effect, the worst effect on the person:

| Code | Effect |
|---|---|
| F | Fire, smoke or cell venting |
| B | Burn or hot surface, cell damage that can lead to swelling |
| SL | Loss of traffic awareness without warning. The display looks normal or shows an empty sky |
| IL | Loss of function with indication: dark screen, warning, LED |
| N | Nuisance only: no charging, shorter runtime, ground-only fault |

| Score | Severity (S) | Likelihood (L) | Detectability (D) |
|---|---|---|---|
| 1 | No noticeable effect | Remote. Needs a rare random part failure | Design detects and acts, or the device does not start |
| 2 | N: nuisance, loss of charging | Low. Random failure of a stressed part, or an assembly fault that AOI usually finds | Visible in normal use before it matters (LED, dark screen) |
| 3 | IL: loss of function in flight with indication | Moderate. Plausible handling or assembly error over the life of a few units | Found by a bring-up test already listed in [open-issues.md](open-issues.md) |
| 4 | SL or B: silent loss of traffic awareness, burn, cell damage | High. Expected at least once over the device life | Found only by a dedicated test that is not planned yet |
| 5 | F: fire, smoke, venting | Near certain under realistic use | Not detectable before the harm |

RPN = S x L x D. "Rev" says where the mitigation fits: **A** = no board change (test, firmware, assembly, harness, user instruction). **B** = board change.

## 1. USB input: J1, D3, C1, FB1, C5

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| U1 | J1 VBUS to GND short (debris, bent contact) | Source limits or shuts down. No charging. Battery operation continues | N | Source protection. D4 red stays off [Doc] | 2 | 3 | 2 | 12 | Inspect the port. Dust cap on the bulkhead | A |
| U2 | D3, C1 or C5 short | VBUS shorted at the source. A plain USB-A charger may push its full current into the part. FB1 carries it (4 A rating) [S][Doc] | N | Source limit. No red LED | 2 | 1 | 2 | 4 | None needed | |
| U3 | D3 open | No surge clamp. U4 withstands 16 V [D] | N | None, latent | 2 | 1 | 5 | 10 | None needed | |
| U4 | Faulty source above 5 V | U4 locks out at 13.7 V and withstands 16 V. D3 clamps 19.9 V at 20.8 A, above 16 V [D][Doc] | N | U4 OVLO | 3 | 1 | 5 | 15 | Accept. SMAJ10A is still above 16 V at full surge | |
| U5 | FB1 open | No USB power, no charging | N | No red LED with a source [Doc] | 2 | 1 | 2 | 4 | None needed | |
| U6 | FB1 shorted or fitted as 0 Ω | Conducted ripple 18 dB over the CISPR 32 limit without FB1 and C1 [Sim]. Possible desense while charging [A] | SL partial | None | 3 | 1 | 5 | 15 | AOI. Bench ripple check at J1 (listed) | A |

## 2. Charger U4 with L1, BYP and SYS capacitors

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| C1 | U4 loses charge-voltage control (internal high-side or QBAT short, LX to BYP bridge) | Cell charged past 4.20 V | F | Pack PCM overcharge cut. The pack data sheet asks for 4.25 ± 0.025 V [D]. The fitted PCM is not measured [A] | 5 | 1 | 5 | 25 | Check termination at 4.20 V on every board with a bench supply in place of the pack. Test one sample pack for its PCM overcharge cut | A |
| | | | | | | | | | VBAT over-voltage detector at about 4.30 V that pulls THM low through Q6. Covers control faults, not a shorted switch | B |
| C2 | IFAST: R6 or R37 shorted | 6.2 kΩ gives about 1 A (Table 3: 6.65 kΩ = 1 A) [D][S]. Longer charge, timer expiry | N | LED pattern | 2 | 2 | 3 | 12 | Measure charge current at bring-up | A |
| C3 | IFAST open (R6 or R37 tombstoned or missing) | Behaviour with IFAST open is not in the data sheet [A]. Worst case 3.15 A charge, which is still inside the pack's 5 A rating [D]. More U4 heat | N | Thermal regulation at 130 °C [D] | 3 | 2 | 3 | 18 | AOI. Measure charge current at bring-up | A |
| C4 | Thermal regulation in the closed case while running (by design) | Charge current drops. U4 junction 118 to 127 °C at 1.8 A with sink [Sim] | N | TREG 130 °C, thermal shutdown [D] | 2 | 4 | 2 | 16 | Charge with the device off (U4 at 62 °C [Sim]) | A |
| C5 | Safety timer expires on the 10 Ah pack | Timer is 6 h ± 20 %, so 4.8 h minimum [D]. A full charge at 1.8 A takes 5 h 55 min [Doc]. Pack stops part-full. No automatic retry [D] | IL | D4 red on, green off. Gauge SOC | 2 | 4 | 2 | 16 | Firmware: show "charge incomplete" when USB is present, SOC < 95 % and charge rate is zero. User: replug to restart | A |
| | | | | | | | | | Longer timer option or higher IFAST (issue 0003) | B |
| C6 | Weak source (PC port) with Wi-Fi on | Battery supplements and discharges while plugged in [Doc] | N | Gauge charge rate | 2 | 3 | 3 | 18 | Firmware: "USB source too weak" | A |
| C7 | ENBST (pin 8) open joint | Pin floats. Internal pull-down is not stated [A]. Reverse boost may drive 5.1 V onto VBUS. Q9 then inhibits the undervoltage latch | IL | Pin 9 next to it is GND, so a bridge is harmless [S] | 3 | 1 | 4 | 12 | X-ray U4 (see A1) | A |
| C8 | BYP to CHGIN bridge (pins 22, 23) | QCHGIN bypassed: no OVLO, no reverse blocking. Battery voltage appears on VBUS, Q9 inhibits the latch, battery current can flow into a connected host [S][D] | IL | None | 3 | 2 | 4 | 24 | X-ray U4 (see A1) | A |
| C9 | L1 open | No buck, no charging. Battery operation continues | N | No charging LED | 2 | 1 | 2 | 4 | None needed | |
| C10 | C6 or C33 (BYP) short | CHGIN switch drives into the short up to the source limit. U4 hot under the sink, then thermal shutdown [D] | N | TSHDN [D] | 3 | 1 | 2 | 6 | None needed | |
| C11 | Hard short on SYS (6 A or more) | QBAT opens after 6 ms and retries every 0.15 s. With USB the buck limits to 200 mA while SYS is low [D] | IL | QBAT OCP [D] | 3 | 1 | 2 | 6 | None needed | |
| C12 | Soft short below 5 to 6 A on VBAT, VSYS or BOOST_5V (cracked MLCC: C9 to C14, C11, C27, C18 to C20) | Nothing trips. F1 carries 100 % for at least 4 h and opens at 200 % within 5 s [D]. QBAT trips at 6 A only. The POWER slider does not isolate the pack: QBAT stays closed and a disabled TPS61088 conducts through its high-side body diode [D][Doc] | F | None below 6 A. Placement verified: all these MLCCs are at least 7.7 mm from the edge and 9 mm from a hole [S] | 5 | 1 | 5 | 25 | Torque-limited screws, no board flex at assembly | A |
| | | | | | | | | | Battery disconnect controlled by the slider ([issue 0001](../issues/0001-battery-disconnect-switch.md)) plus a battery-side current limit near 3 A. Flex-termination MLCCs on VBAT and VSYS | B |

## 2b. Charger U4 on carrier-ti-charger: BQ25798

Rows C1 to C12 describe the MAX77757 board. The BQ25798 variant changes these:

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| T1 | Host never configures the charger (display absent, firmware fault) | Default mode: 1 A, 4.2 V, TREG 120 °C, 12 h timer [D]. Charging continues, slower | N | WD_STAT readable, STAT LED | 1 | 3 | 2 | 6 | By design; the default is the safe state | |
| T2 | Host stops writing (crash, reset) | Watchdog expiry (40 s default) resets every register to default [D]: 1 A, TREG 120 °C, JEITA back to defaults | N | WD_STAT, INT pulse | 1 | 3 | 3 | 9 | Firmware re-applies the profile on every WD_STAT; never disables the watchdog | A |
| T3 | Host writes a wrong profile (ICHG above 1.8 A, VREG above 4.2 V, TREG 120 °C) | More heat; VREG up to 4.99 V on a 1S setting [D] | F | Pack PCM overcharge cut at about 4.25 V [D] | 4 | 2 | 3 | 24 | Firmware clamps writes; verify by reading back; keep VREG at the 4.2 V default. Pack PCM is the last line | A |
| T4 | Pack above 45 °C in default mode | T5 hot stop at 43.0 °C nominal, 44.2 °C worst case (RT1 80.6 kΩ, RT2 768 kΩ) suspends charging in default mode too; T3 at 32.2 °C lowers VREG by 400 mV [Sim]. Pack rated to 45 °C [D] | N | TS thresholds | 3 | 2 | 3 | 18 | Host sets JEITA_VSET = suspend, or the 71.5 kΩ / 412 kΩ divider puts T5 at 45 °C (`docs/power.md`) | B |
| T5 | PROG (R56) open or shorted | Open: cell count undefined at POR [A]; shorted: 0 Ω is no table entry [D]. Charger may configure for the wrong frequency or cell count | F | VREG default follows the cell count; pack PCM | 4 | 1 | 3 | 12 | Measure VREG and VSYSMIN at bring-up; X-ray not needed, 0603 | A |
| T6 | ILIM_HIZ divider (R57/R58) open at the top | Pin below 0.75 V: converter stops, HIZ [D]. No charging, battery runs the system | IL | STAT, PG_STAT | 2 | 1 | 2 | 4 | None needed | |
| T7 | CE pulled high by Q2 leakage (above 1.3 V) | Charging disabled with the thermostat closed | IL | CHG_STAT | 2 | 1 | 3 | 6 | 100 kΩ R12 keeps CE at 0.1 V at the 25 °C leakage maximum; 5 µA at 55 °C reaches 0.5 V, still below VIH [Sim] | |
| T8 | C-to-C cable from a PD adapter | D+/D- open: "unknown adapter", input limit 3 A [D], capped by ILIM_HIZ at about 3 A; a 500 mA-limited source then sags to VINDPM | N | VINDPM loop | 2 | 3 | 2 | 12 | AICL holds the input at 4.2 V; bench test | A |
| T9 | Unrouted copper paths in the heat model | The model assumes 5 mΩ per unrouted net until the board is routed [Sim] | - | report line | - | - | - | - | Rerun after routing | A |

Package: VQFN-HR 29 at 0.4 mm pitch with no exposed pad; the GND pin is a 0.2 x 1 mm pad. A1 (bridges) applies with the TI land pattern; A3 (heatsink stack short) does not apply, the plate sits on the solder mask through an insulating pad.

## 3. Battery path: pack, J3, harness, F1, gauge U8

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| B1 | Pack lead reversed at J3 (pack with the other JST-PH pin convention, or re-crimped lead) | -4.2 V on VBAT. Current flows through the U4 BATT and U8 VDD protection structures. F1 opens only at high current. The PCM may not trip below its short threshold [S][A] | F | Silkscreen + and BAT. Pre-power check in open-issues [Doc] | 5 | 3 | 3 | 45 | Check every pack lead with a meter before first connection. Mark pack plug and J3 with the same colour. Buy packs from one source | A |
| | | | | | | | | | Reverse-polarity stage ([issue 0002](../issues/0002-battery-reverse-polarity-protection.md)) | B |
| B2 | Battery plug into J2 (NTC header). J2, J3 and J4 are the same JST-PH 2-pin part. J2 sits 7.3 mm beside J3 [S] | Pack + on THM (pin 1). THM absolute maximum is 2.2 V [D]. U4 damaged. Pack current flows into the THM pin structure without a fuse [A] | F | None | 4 | 3 | 4 | 48 | Colour-code and label plugs and headers. Cut sensor leads so short that their plugs cannot reach J3, and the pack lead so that it cannot reach J2 | A |
| | | | | | | | | | Different connector for the sensors, for example one 4-pin header for NTC and thermostat, or a different series | B |
| B3 | Battery plug into J4 (thermostat header) | Pack + on THERM_FEED (R43 to VSYS), pack - on THERM_OK. The pack floats against GND. Low current [S][A] | N | Device does not start | 2 | 2 | 1 | 4 | As B2 | A |
| B4 | NTC and thermostat plugs swapped (J2 and J4) | The NTC in J4 reads as a closed contact at all temperatures: THERM_OK at least 0.97 x VSYS at 60 °C (NTC about 22 kΩ) [S][A]. The thermostat in J2 holds THM low, so no charging. The 60 °C load cut-off is lost | B | Charging never works: red on, green off [Doc] | 4 | 3 | 2 | 24 | As B2. Bring-up: thermostat trip test (listed) | A |
| B5 | Harness chafe: pack leads around the board edge [Doc] wear through, BAT+ to BAT- upstream of F1 | Only the PCM protects. A soft short below the PCM trip heats the wire | F | PCM. Thresholds not known [A] | 5 | 2 | 4 | 40 | Route the leads through a grommet or slot, no sharp edges, strain relief at the pack. Inspect at each pack change | A |
| | | | | | | | | | J3 on the board underside next to the pack, short leads (slot in the insulator already foreseen) | B |
| B6 | J3 contact overload or poor crimp | JST-PH is rated 2 A [D]. Battery current is about 1.9 A at the 1 A peak and about 3 A at the 1.5 A headroom [Doc]. Charging puts 1.8 A through J3 for hours. Contact heat | B | None. Earlier undervoltage trip at most | 4 | 2 | 4 | 32 | Bring-up: J3 temperature at 1 A load and while charging. Inspect the pack crimps | A |
| | | | | | | | | | JST-XH or two contacts per pole (open-issues option) | B |
| B7 | Pack internal fault (internal short, swelling) | Outside board control. Nothing disconnects the pack | F | Thermostat cuts loads, NTC stops charging. Both only sense | 5 | 1 | 5 | 25 | Qualified packs only. Inspect for swelling. Leave a vent path behind the rear cover. User instruction | A |
| B8 | PCM failed short (latent) | No effect alone. Removes the last layer for C1, B1, B5, C12 | N | None | 2 | 1 | 5 | 10 | None on the board | |
| B9 | F1 open | Battery disconnected. USB-only operation still works with J4 closed [Doc] | IL | Device off on battery | 3 | 1 | 2 | 6 | None needed | |
| B10 | F1 bridged or wrong part | Fuse lost (latent). PCM stays | N | None | 2 | 1 | 5 | 10 | AOI | A |
| B11 | U8 internal VDD to GND short | Current from VBAT limited only by F1. U8 (2 x 2 mm) heats | F | F1 above about 10 A | 4 | 1 | 5 | 20 | None at rev A | |
| B12 | Gauge reads high SOC or gauge lost | No or late low-battery warning. Undervoltage cut-off comes unannounced | IL | Screen dark after the cut-off | 3 | 3 | 3 | 27 | Firmware: warn also on VCELL (for example below 3.45 V under load) and on gauge read failure | A |
| B13 | Thermostat lead short to GND | R43 limits to 4.2 mA [S]. Loads off, charging off. The 4.2 mA also flows with the device off, about 100 mAh per day | IL | Device off, red LED only | 3 | 2 | 2 | 12 | Harness inspection | A |

## 4. Temperature protection: NTC, R8, C35, thermostat, R43, R10, C31, Q2, R12, Q6

Verified circuit [S]: VSYS, R43 1 kΩ, J4 thermostat, THERM_OK. R10 1 MΩ and C31 to GND. Q2 AO3401A gate on THERM_OK, source VSYS, drain CHG_DIS with R12 100 kΩ to GND. Q6 2N7002 gate on CHG_DIS pulls THM to GND. THM below 6 % of PVL disables charging (buck state) [D].

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| T1 | NTC open or unplugged | Reads cold, charging suspended [Sim] | N | Red on, green off | 2 | 3 | 2 | 12 | None needed | |
| T2 | NTC shorted, C35 short | THM below 4.5 %, charger disabled [Sim][D] | N | Red on, green off | 2 | 1 | 2 | 4 | None needed | |
| T3 | NTC detached from the pack or poor thermal contact | Reads cooler than the pack. Charging continues above 45 °C at the cell | B | Thermostat at 60 ± 5 °C, if it stays attached | 4 | 2 | 5 | 40 | Bond with thermal pad and Kapton. At assembly, warm the pack with hot air and confirm that charging stops near 42 °C | A |
| | | | | | | | | | Pack temperature readout to firmware (MAX77972, issue 0003) for a plausibility check | B |
| T4 | Bare NTC lead touches a pack tab or BAT+ | 4.2 V on THM, as B2 | F | None | 4 | 2 | 4 | 32 | Heat-shrink the Dumet leads (in the BOM). Place the NTC away from the PCM end of the pack | A |
| T5 | R8 open | THM pulled low, reads hot or disabled, no charging | N | LED | 2 | 1 | 2 | 4 | None needed | |
| T6 | R8 short | THM at PVL, reads cold, no charging | N | LED | 2 | 1 | 2 | 4 | None needed | |
| T7 | R8 wrong value, lower than 154 kΩ | Reads colder. With 100 kΩ the hot stop moves to about 52 °C [A, B-value model] | B | J2 resistance sweep (listed) | 4 | 1 | 3 | 12 | Keep the sweep in the bring-up test | A |
| T8 | Thermostat fails closed | No 60 °C load cut-off. NTC still stops charging at 45 °C or below | B | None | 4 | 1 | 5 | 20 | Trip-test each thermostat with hot air before fitting | A |
| T9 | Thermostat contact resistance rises. It runs at about 4 µA (R10 1 MΩ). The Limitor data names 20 mA minimum current [D] | Q2 turns on above about 170 kΩ contact resistance (VGS(th) -0.5 V minimum [D]). Loads off, charging off. Fail-safe | IL | Device switches off | 3 | 1 | 4 | 12 | None at rev A. Accept or ask Limitor about dry-circuit use | B |
| T10 | Thermostat opens in flight from sun heat (by design) | Typical load at 50 °C ambient gives a 58 °C pack, inside the 55 to 65 °C opening band [Sim]. Device off. Auto reset after cooling [D] | IL | Screen dark. No audible warning | 3 | 4 | 3 | 36 | Firmware: read BMP581 temperature and warn above about 50 °C. Enclosure: light colour, shade, vents | A |
| | | | | | | | | | Pack temperature readout to firmware (issue 0003) | B |
| T11 | R43 open | No feed to THERM_OK. Device never starts, no charging | N | Device does not start | 2 | 1 | 1 | 2 | None needed | |
| T12 | R10 open or missing | Slider ON: THERM_OK still discharges through R13 and the 800 kΩ U6 EN pull-down [D], so loads still go off, slower. Slider OFF: THERM_OK floats high, Q2 stays off, thermostat no longer stops charging. NTC remains | B | None (latent) | 3 | 2 | 4 | 24 | Bring-up: open J4 with slider OFF and USB present, confirm red-only LED | A |
| T13 | Q2 or Q6 open or missing, Q6 gate open | Thermostat no longer stops charging. Loads still cut. NTC remains | B | None (latent) | 3 | 2 | 4 | 24 | Same test as T12 | A |
| T14 | Q2 or Q6 drain-source short, R12 open with Q2 leakage | Charging permanently disabled | N | LED | 2 | 1 | 2 | 4 | None needed | |
| T15 | C31 short | THERM_OK at GND. Device never starts, no charging | N | Device does not start | 2 | 1 | 1 | 2 | None needed | |
| T16 | Condensation on the THM and THERM_OK nodes (154 kΩ, 100 kΩ NTC, 1 MΩ) | Leakage to GND reads hot or trips (safe). Leakage from VSYS reads cold and moves the hot stop up [A] | B | None | 3 | 2 | 5 | 30 | Conformal coat the area around R8, C35, R10, C31, Q2, Q6 | A |

## 5. Undervoltage supervisor and latch: U5, Q7, Q8, Q9, R40, C37, R41, R42, SW1

Verified circuit [S]: U5 TLV840CADL29 (open drain [D]) on VSYS pulls PWR_EN low. Q7 AO3401A (source HOLD, gate PWR_EN) sets LATCH. Q8 2N7002 holds PWR_EN low. Q9 clears LATCH while USB_VBUS is high. HOLD is SW_EN through R40 10 kΩ and C37 100 nF. SW1 switches SW_EN between THERM_OK and GND.

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| V1 | U5 output stuck low, Q8 short | Device never turns on | N | Does not start | 2 | 1 | 1 | 2 | None needed | |
| V2 | U5 never trips (open output, missing) | No 2.9 V cut-off. Runs to the PCM cut-off at 2.5 V. Modules brown out near the end [A] | IL | None | 3 | 1 | 3 | 9 | Undervoltage bring-up test (listed) | A |
| V3 | Latch fails: Q7 or Q8 open, Q9 short, C37 short | After a trip SYS rebounds and the system restarts about every 11 ms [Doc]. Display may show garbage or reboot loops | IL | None | 3 | 1 | 3 | 9 | Undervoltage bring-up test (listed) | A |
| V4 | Latch sets at power-on: C37 open, R41 open | U5's 40 µs power-on pulse sets the latch. Device does not start or turns off at random [Doc][A] | N | Visible on the ground | 2 | 2 | 1 | 4 | None needed | |
| V5 | Q9 or R42 open | USB no longer clears the latch. The slider still does | N | | 2 | 1 | 3 | 6 | None needed | |
| V6 | USB_VBUS held high without a source (C7, C8) | Latch inhibited, as V3 | IL | None | 3 | 1 | 4 | 12 | X-ray U4 | A |
| V7 | SW1 contact interrupts under vibration (winch launch, landing) | PWR_EN drops once SW_EN is low longer than the C37 hold of about 1 ms [A]. 5 V rail restarts, modules reboot, GNSS reacquires | IL | Display reboot is visible | 3 | 2 | 4 | 24 | Vibration test of the assembled unit | A |
| | | | | | | | | | RC filter on SW_EN of tens of ms | B |
| V8 | SW1 stuck in ON | Cannot switch off. Unplug the battery | N | | 2 | 1 | 2 | 4 | None needed | |
| V9 | SW1 misplaced (peg offset still open [Doc]) | Wrong contacts. Device always on or off. THERM_OK to GND is limited to 4.2 mA by R43 [S] | N | Visible at bring-up | 2 | 2 | 1 | 4 | JLCPCB preview check (listed) | A |

## 6. 5 V rail: U6, L2, U7

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| P1 | Boost over-voltage: R15 short, R14 open, FB (pin 17) bridged, U6 internal | U6 drives VOUT to its 13.2 V OVP [D]. U7 absolute maximum is 6 V [D]. U11 and U12 have OVLO tied to GND, so their over-voltage lock-out is off [S][D]. Modules see up to 13 V | F | None on the carrier. First power-up measurement catches assembly faults only | 5 | 1 | 5 | 25 | First power-up without modules: measure TP7 at 5.0 to 5.1 V | A |
| | | | | | | | | | OVLO divider on U11 and U12 to trip at about 5.7 V (two resistors each), or a clamp on RAIL_5V | B |
| P2 | U6 stuck off, L2 open, EN open | No 5 V. Device dark | IL | Dark screen | 3 | 1 | 2 | 6 | None needed | |
| P3 | U6 stops switching but conducts, or high-side short | Modules get VSYS minus a diode, 3.0 to 4.2 V. The ADSBee LDO needs 3.7 V [Doc]. ADSBee may stop while the display runs [A] | SL | D5 still lit | 4 | 1 | 4 | 16 | Firmware ADSBee heartbeat (see M1) | A |
| P4 | U6 low-side short | VSYS shorted through L2. QBAT OCP hiccup [D] | IL | QBAT OCP | 3 | 1 | 2 | 6 | None needed | |
| P5 | U6 thermal shutdown | Rail drops and restarts. Junction 94 to 96 °C at 50 °C ambient, 1 A [Sim], TSD at 150 °C [D] | IL | Auto restart | 3 | 1 | 3 | 9 | None needed | |
| P6 | R17 short or ILIM (pin 19) bridged to AGND (pin 20) | Current limit goes very high (I_LIM inverse to R_ILIM [D]). Overload is no longer limited by U6 | N (latent) | QBAT 6 A | 3 | 2 | 4 | 24 | X-ray or AOI of U6. Load-step bench test (listed) | A |
| P7 | R17 open | Current limit near zero or undefined [A]. Boost fails to start or folds at Wi-Fi bursts | IL | Visible at bring-up | 3 | 1 | 2 | 6 | AOI | A |
| P8 | R16 open or wrong | Switching frequency moves. Spurs may land in the receiver bands. ADS-B range drops | SL partial | None | 3 | 1 | 5 | 15 | Noise floor measurement (listed) | A |
| P9 | Two of C18 to C20 missing or open | Less than the 25 µF minimum in the stability and load-step model [Sim]. Dips below 4.75 V at Wi-Fi bursts, ADSBee resets | SL | AOI | 4 | 1 | 4 | 16 | AOI. Load-step bench test (listed) | A |
| P10 | U7 stuck on | RAIL_5V follows BOOST_5V. With the slider OFF the modules get VSYS minus a diode. The latch cannot remove the load. Pack drains to the PCM cut-off | IL | D5 glows with slider OFF | 3 | 1 | 3 | 9 | Bring-up: TP7 at 0 V with slider OFF | A |
| P11 | U7 stuck off, ON open | No 5 V | IL | Dark screen | 3 | 1 | 2 | 6 | None needed | |
| P12 | Hard short on RAIL_5V (module fault, debris) | U7 TSD at 160 °C with auto restart [D]. U11 or U12 thermal shutdown latches off [D]. Battery side QBAT 6 A | B | TSD | 4 | 2 | 2 | 16 | Keep metal debris out of the sockets | A |

## 7. Module feeds: U11, U12, JP1, JP2, R44 to R47

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| M1 | ADSBee data lost while the display runs: JP2 left out after USB flashing, U11 off (R46 open, latched thermal fault), R29 or R30 open, ADSBee firmware hang | Display shows an empty sky. In a glider this looks normal most of the time | SL | None in hardware or current firmware | 4 | 3 | 4 | 48 | Firmware: ADSBee heartbeat or status message every second. Display shows "RECEIVER LOST" and beeps when it stops for more than a few seconds. Same for GNSS fix status. Pre-flight check of JP2 | A |
| | | | | | | | | | Remove JP1 and JP2. The LM73100 already blocks back-feed [Doc][D]. Use a solder jumper if an unpowered module is still needed | B |
| M2 | Display unpowered: JP1 out, U12 off | Display dark | IL | Dark screen | 3 | 2 | 2 | 12 | Silkscreen "ESP 5V" at JP1 (open issue) | A |
| M3 | U11 or U12 switch short | Reverse blocking lost. A module on its own USB can back-feed RAIL_5V with the shunt fitted | N | | 2 | 1 | 4 | 8 | None needed | |
| M4 | Socket or jumper contact intermittent under vibration (8.5 mm sockets, display on one standoff) | Module resets. Traffic gone for the reboot and GNSS reacquisition time [A] | SL or IL | None | 4 | 2 | 4 | 32 | Fit all module screws. Secure jumpers. Vibration test. Heartbeat as M1 | A |
| | | | | | | | | | Solder modules or integrate the GNSS chip (owner plan) | B |
| M5 | No over-voltage lock-out on module feeds (OVLO at GND) | See P1 | | | | | | | | |

## 8. Module insertion faults (sockets not keyed)

Socket geometry from the board [S]: J5 (GNSS) is one 1x8 row. The display uses four parallel groups J6 to J9 in two columns. J10 (1x13) and J11 (1x5) of the ADSBee run at right angles.

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| I1 | GNSS module rotated 180° in J5 | Module GND lands on an unconnected pin, GNSS_3V3 on INT. The module is fed through its I/O protection. ADSBee TX meets module TXO [S]. No fix, possible I/O damage. Traffic positions unusable | SL | Silkscreen only [Doc]. MH10 and MH11 do not align when rotated (holes 34 mm from J5 on one side) [S], so the screws catch it if fitted | 4 | 3 | 3 | 36 | Rule: fit both GNSS screws before power. Firmware: show "no GNSS" | A |
| | | | | | | | | | Integrate MAX-M10S or use a keyed connector | B |
| I2 | GNSS module offset by one pin | Module unpowered (+1) or its 3V3 pin on GND (-1) [S]. ADSBee 3V3 output is not shorted in either case | SL | Screws do not align (2.54 mm shift, M2.5 in 2.7 mm hole) [A] | 4 | 2 | 3 | 24 | As I1 | A |
| I3 | Display offset by one pitch toward pin 1 | ESP_5V on the module GND pad, carrier GND on module 3V3 and D11 [S]. The module 3.3 V domain is reverse-fed from RAIL_5V through U12. A 180° rotation does not fit the four groups [S] | F | MH13 screw does not align [A] | 5 | 2 | 3 | 30 | Rule: fit the MH13 screw before power. Mark pin 1 on module and socket | A |
| I4 | ADSBee offset by one pitch toward pin 1 | 5 V on module GND, carrier GND on module TX [S]. J11 then sits off its row, so this needs force. 180° rotation does not fit [S] | F | MH12, MH14 screws | 5 | 1 | 3 | 15 | As I3 | A |

## 9. Controls and I2C: SW2, SW3, U13, J12, ESP_3V3

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| K1 | SW2 or U13 P0 stuck at mute | Buzzer silent. The buzzer is ground feedback per [requirements.md](requirements.md), the display still shows traffic | N | Touchscreen shows the slider state [Doc] | 2 | 2 | 3 | 12 | None needed | |
| K2 | SW3 or U13 P1 stuck | EFB feed lost, or Wi-Fi stays on: more heat, shorter runtime | N | Touchscreen state | 2 | 2 | 3 | 12 | None needed | |
| K3 | U13, U10 or U8 holds SDA low (latch-up, ESD) | Gauge, barometer, expander and the display's touch controller share the bus [Doc]. Touch and SOC lost. Slider states retained [Doc] | IL | Shared failure icon (planned firmware) | 3 | 2 | 3 | 18 | Firmware: bus recovery with 9 clocks, failure icon, VCELL fallback as B12 | A |
| K4 | External voltage on J12 | ESP_3V3 back-fed, U13 damaged | N | None | 2 | 2 | 4 | 16 | Dry contacts only (documented). Series resistors | B |
| K5 | Short on ESP_3V3 (C28 to C30, C40, U9, U10, U13) | Display regulator overloaded, display resets or stays dark [A] | IL | Dark screen | 3 | 1 | 2 | 6 | None needed | |

## 10. Assembly faults

| ID | Item and failure mode | Effect on the system | Cockpit | Existing detection or limit | S | L | D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|---|---|---|---|
| A1 | U4 FC2QFN-24 bridges or opens (0.4 mm pitch, no exposed pad, footprint smaller than the Maxim land pattern [Doc]). Critical neighbours [S]: IFAST to BATT (12, 13), SYS to PVL (16, 17), LX to BYP (21, 22), BYP to CHGIN (22, 23) | From no charging up to over-voltage on 2.2 V pins and loss of charge control (C1, C8) | F | AOI cannot see joints under the body [A] | 5 | 2 | 4 | 40 | X-ray U4 on every board. Staged first power-up: USB without pack, then a current-limited bench supply at 3.7 V in place of the pack, check termination at 4.20 V, then the real pack | A |
| | | | | | | | | | Footprint to the Maxim land pattern | B |
| A2 | U6, U7, U11, U12 joint faults (exposed pads with unfilled vias, 0.45 mm pitch on U11, U12) | See P1, P6. Exposed-pad voids raise temperature | F worst | Inspection listed [Doc] | 5 | 2 | 3 | 30 | X-ray or AOI. First power-up without modules, measure TP7 | A |
| A3 | Heatsink stack on U4: copper spacer, epoxy, sink over C6, C7, C8, C11, C33, C34, R6, R8 [Doc]. Nominal clearance over the 1206 C33 is about 0.5 mm [A] | A tilted sink or conductive epoxy squeeze-out can bridge VBAT, BYP or LX to GND. VBAT to GND is upstream of QBAT, only F1 and the PCM act | F | None | 5 | 2 | 3 | 30 | Use electrically insulating epoxy. Kapton under the sink. Check clearance to all capacitor terminals after bonding | A |
| A4 | D4 fitted reversed (Reichelt page names it common anode, wrongly [Doc]) | Wrong or no charge indication | N | Bring-up LED check (listed) | 2 | 3 | 2 | 12 | Follow the BOM note | A |
| A5 | Pin tails of hand-soldered parts touch the pack | Puncture or short into the pack | F | Insulator board, tails trimmed to 1.5 mm [Doc] | 5 | 1 | 3 | 15 | Keep the insulator board mandatory | A |

## Design findings

Items that look like real design gaps, checked in the schematic or data sheet:

1. **No over-voltage protection on the module feeds.** U11 and U12 OVLO pins are tied to GND [S], which disables their lock-out [D]. One failed resistor in the boost feedback (R14 open or R15 short) drives RAIL_5V toward the TPS61088's 13.2 V OVP [D]. U7 is rated 6 V absolute [D]. Rev B fix: an OVLO divider per LM73100.
2. **J2, J3 and J4 are interchangeable.** All three are JST-PH 2-pin [S]. The battery plug fits J2 and puts the pack on THM, rated 2.2 V [D]. A J2 and J4 swap silently removes the 60 °C load cut-off.
3. **The POWER slider does not isolate the pack.** Soft shorts between about 2 and 6 A on VBAT, VSYS or BOOST_5V trip nothing: F1 needs 200 % for 5 s [D], QBAT trips at 6 A [D], and the boost conducts through its body diode when disabled. [Issue 0001](../issues/0001-battery-disconnect-switch.md) addresses part of this.
4. **The safety timer is too short for the 10 Ah pack.** 6 h ± 20 % [D] against 5 h 55 min of charging [Doc]. Incomplete charges are likely.
5. **The thermostat runs as a dry circuit.** About 4 µA through R10 against the 20 mA minimum in the Limitor data [D]. The failure direction is safe (nuisance trip), but contact reliability is not established.
6. **JP1 and JP2 add a silent failure path.** The LM73100 already blocks back-feed. A missing JP2 leaves the display running with no traffic.

## Rev A actions (no board change)

1. Firmware: ADSBee heartbeat with "RECEIVER LOST" alarm, GNSS fix status, VCELL low-battery fallback, I2C bus recovery, BMP581 over-temperature warning, "charge incomplete" hint.
2. Bring-up per board: X-ray U4. First power without modules and with a current-limited bench supply in place of the pack. TP7 at 5.0 to 5.1 V, TP7 at 0 V with slider OFF. Termination at 4.20 V. Thermostat trip with slider ON and with slider OFF plus USB. J2 resistance sweep.
3. Harness: colour code and label J2, J3, J4 and their plugs. Cut leads so wrong plugs cannot reach. Grommet and strain relief. Meter check of each pack lead.
4. Assembly rules: module screws fitted before power. Insulating epoxy and clearance check on the U4 heatsink. Conformal coat around the THM and THERM_OK parts.
5. Sensor fitting: thermal pad and Kapton, hot-air check that charging stops near 42 °C and the thermostat opens near 60 °C.

## Rev B actions (board change)

1. OVLO dividers on U11 and U12.
2. Distinct connectors for pack and sensors, or one keyed sensor connector.
3. Reverse-polarity stage at J3 (issue 0002) combined with a slider-controlled battery disconnect and a current limit near 3 A (issue 0001).
4. JST-XH or two contacts per pole for the pack.
5. U4 footprint to the Maxim land pattern. Longer charge timer or higher charge current (issue 0003).
6. Pack temperature readout to firmware (issue 0003).
7. Remove JP1 and JP2, or replace them with solder jumpers. Solder modules or integrate the GNSS chip.
8. RC filter on SW_EN against contact bounce.

## Top 10 risks

Ranked by RPN, then by severity.

| Rank | ID | Risk | S L D | RPN | Mitigation | Rev |
|---|---|---|---|---|---|---|
| 1 | B2 | Battery plug into the NTC header J2: pack voltage on THM | 4 3 4 | 48 | Colour code, lead lengths. Distinct connectors | A, B |
| 2 | M1 | ADSBee data lost while the display runs (JP2 out, U11 off, UART, hang) | 4 3 4 | 48 | Firmware heartbeat alarm. Remove the shunts | A, B |
| 3 | B1 | Reversed pack lead at J3 | 5 3 3 | 45 | Meter check per pack. Issue 0002 | A, B |
| 4 | B5 | Pack lead chafe, short upstream of F1 | 5 2 4 | 40 | Grommet, strain relief. J3 next to the pack | A, B |
| 5 | A1 | U4 solder fault, up to loss of charge control | 5 2 4 | 40 | X-ray, staged first power-up. Maxim land pattern | A, B |
| 6 | T3 | NTC detached from the pack, charging above 45 °C | 4 2 5 | 40 | Bonding, hot-air check. Pack temperature to firmware | A, B |
| 7 | I1 | GNSS module reversed in J5 | 4 3 3 | 36 | Screws before power, "no GNSS" display. Integrate GNSS | A, B |
| 8 | T10 | Thermostat shuts the device down in flight from sun heat | 3 4 3 | 36 | BMP581 temperature warning, shade, vents | A |
| 9 | B6 | J3 contact heat above its 2 A rating | 4 2 4 | 32 | Temperature check at 1 A. JST-XH | A, B |
| 10 | M4 | Socket or jumper contact loss under vibration | 4 2 4 | 32 | Screws, vibration test, heartbeat. Solder modules | A, B |

Severity 5 items below the top 10 that still need a decision: A3 heatsink stack short (30), I3 display offset (30), P1 boost over-voltage with no OVLO (25), C12 MLCC soft short with no isolation (25), C1 overcharge on U4 failure (25).
