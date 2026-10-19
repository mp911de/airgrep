# Design FMEA: active TI carrier and battery harness

Scope: `hardware/carrier-ti-charger/`, including its pack and harness. PCB and schematic connectivity were checked through a fresh netlist on 2026-10-18. This is a design risk register, not physical acceptance. The frozen MAX board's failure analysis and RPN scores remain in git; its IFAST, THM and six-hour timer conclusions do not apply here.

Sources: [specs](specs.md), [decisions](decisions.md), [pin maps](interfaces.md), local component datasheets and recorded TI simulation results. Proposed firmware mitigations are requirements, not implemented protection. Model results must be checked against their recorded inputs before use. Likelihood and detectability have not been requalified for this revision, so no current RPN ranking is claimed.

## Battery and harness

| ID | Failure and consequence | Existing protection and remaining action |
|---|---|---|
| B1 | Reversed pack at J11 can damage the charger, gauge and GNSS backup supply | F1 is not polarity isolation. Meter-check each lead; [issue 0002](../issues/0002-battery-reverse-polarity-protection.md) is not implemented. |
| B2 | Pack plugged into NTC J12 applies cell voltage to BAT_TS | J11/J12/J13 use interchangeable two-pin PH housings. Label and colour-code plugs, constrain lead reach; consider distinct sensor connectors. |
| B3, B4 | Battery or sensor plugs exchanged with thermostat J13 can defeat temperature protection or prevent startup | Verify harness wiring and both temperature cut-offs before use. Do not infer correct wiring from startup alone. |
| B5 | Lead chafe or a short upstream of F1 heats the harness | Only pack protection acts upstream of F1. Provide strain relief, an insulated edge path and inspection access. |
| B6 | JST-PH contact or crimp overheats | The 2 A contact rating is exceeded by some short-term load cases. Measure contact temperature during charging and peak loads; connector upgrade remains open. |
| B7, B8 | Pack internal fault or failed protection circuit | NTC and thermostat only sense temperature. Inspect for swelling, use the selected packs and retain a vent path. Pack protection has not been physically qualified. |
| B9, B10 | F1 open, bridged or incorrectly fitted | An open fuse removes battery operation; a bridge removes board overcurrent protection. Inspect part marking and continuity. USB display power can remain with J13 open. |
| B11 | Gauge supply short, or soft short on VBAT/VSYS/boost input | POWER does not isolate the pack. F1 and pack protection do not guarantee interruption of every damaging current. Avoid board flex and review [off-state isolation](../issues/0001-battery-disconnect-switch.md). |
| B12 | Gauge reads high or stops responding | Low-battery warning can be lost. Firmware needs voltage plausibility checks and a visible peripheral fault. Hardware undervoltage cut-off remains independent. |

## Charger and USB input

| ID | Failure and consequence | Existing protection and remaining action |
|---|---|---|
| U1, U2 | J14, D1 or an input capacitor shorts | Source current limiting is external. Inspect and first power from a current-limited source. |
| U3, U4 | Input surge, faulty source or D1 open | D1 is not a 5 V set. Charger input ratings do not establish module survival; U14 OVLO protects the display feed subject to transient verification. |
| U5, U6 | FB1 open or bypassed | Open removes USB power; bypass can worsen conducted emissions and RF desense. Verify the real adapter and receiver noise floor. |
| C1, TI3 | Internal charger fault, solder bridge or wrong host VREG overcharges the cell | Pack protection is the last line. Verify 4.20 V regulation with a bench substitute for the pack; set and read back host settings. |
| TI1, TI2 | Host absent or watchdog expires | Defaults are 1 A, 4.20 V, TREG 120 °C and 12 h. Only watchdog-reset registers revert; EN_EXTILIM survives. Firmware must handle source changes independently and reapply the profile on WD_STAT. |
| TI4 | TS divider wrong or pack too hot | R59 82 kΩ / R60 768 kΩ give nominal 2.7 to 42.6 °C cut-offs. Sweep NTC resistance, including open/short, and verify hot leakage. |
| TI5 | PROG R56 wrong, open or short | Wrong startup configuration can affect cell count and switching frequency. Verify VREG and VSYSMIN before connecting a pack. |
| TI6, TI8 | ILIM_HIZ network or CC handling fails | R57 120 kΩ / R58 100 kΩ set about 1.5 A before host override. Firmware must initialize U15 and follow the lower of CC and BC1.2. Test weak sources, detach and replug. |
| TI7 | Q2 leakage lifts CE into its undefined input region | R12 is 100 kΩ. The modeled 55 °C maximum leakage gives about 0.464 V, above guaranteed VIL. Measure hot behavior. |
| C4, C5, C6 | Heat, a weak source or timer expiry leaves the pack partly charged | Charger status and ADC are readable. Show actual charging/fault state; do not treat USB present as evidence of net charging. |
| C7, C8, C9, C10, C11 | Charger pin bridges, inductor/capacitor faults or SYS short | Native charger protections cover specified cases only. Inspect fine-pitch joints and check both battery-only and USB operation with a current-limited supply. |

## Temperature and power switching

| ID | Failure and consequence | Existing protection and remaining action |
|---|---|---|
| T1, T2 | NTC open or short | TS suspends charging. Confirm at J12 on the assembled board. |
| T3, T4 | NTC detached from the pack or bare lead contacts a power node | Temperature reading can be falsely low, or BAT_TS damaged. Insulate leads and bond the sensor to the pack face. Verify thermal coupling without heating the pack beyond its rating. |
| T5, T6, T7 | R59/R60 wrong, open, shorted or contaminated | TS thresholds move or charging stops. Inspect values and resistance-sweep the complete harness. |
| T8, T9 | Thermostat stuck closed or unreliable at dry-circuit current | NTC still controls charging, but receiver temperature cut-off may be lost. Validate thermostat operation and contact reliability at the actual sensing current. |
| T10 | Sun heats the pack to the thermostat trip | Receiver power is removed; USB display power can remain. Validate the closed enclosure and show over-temperature status where observable. |
| T11-T16, B13 | R43/R10/C31/Q2/R12 or harness fault defeats CE or causes nuisance shutdown | Thermostat feeds THERM_OK through R43; Q2 drives CE directly. Q6 is absent. Test open thermostat with POWER on and off, on battery and USB. |
| V1-V6 | Supervisor, latch or USB-clear fault causes early cut-off, deep discharge or restart cycling | U5 monitors SYS at 2.9 V. Verify one trip, no rebound restart and both clearing paths with real capacitance. |
| V7-V9 | POWER contact bounce, wrong switch placement or stuck contact | POWER carries control current and does not isolate VBAT. Check switch sample, assembly orientation and vibration behavior. |
| P1, M5 | Boost feedback fault overvolts the 5 V feeds | U11/U12 use 5.6 kΩ / 1.5 kΩ, both 1 %, tripping at 5.68 V nominal (5.60 to 5.79 V). U14 uses 4.7 kΩ 1 % / 1.24 kΩ 0.1 %, tripping at 5.75 V (5.66 to 5.86 V). U7 remains upstream and exposed. Measure overshoot, survival and input-cycle recovery without modules. |
| P2-P9 | Boost or output-capacitor fault removes or disturbs receiver power | Nominal model margins do not cover wrong parts or missing capacitance. Inspect, verify startup/load steps and measure receiver noise. |
| P10-P12 | U7 stuck on/off, rail short or feed eFuse latched off | A disabled boost still conducts. Confirm rail discharge and shutdown; check recovery after overload. |

## Modules, controls and assembly

| ID | Failure and consequence | Existing protection and remaining action |
|---|---|---|
| M1 | ADSBee loses power or UART while the display stays on | A missing JP3, latched U11 or firmware hang can look like an empty sky. Require receiver heartbeat, visible data-loss indication and GNSS validity. |
| M2, M3 | JP1 absent or feed isolation fails | Display can be dark or own-USB power can backfeed. Follow the module-power shunt rules; verify isolation. |
| M4 | Socket or jumper intermittency resets a module | Fit mounting screws, secure shunts and test vibration. U16 is soldered; old removable-GNSS socket risks do not apply. |
| I3, I4 | Display or ADSBee inserted offset by one pitch | Unkeyed headers can connect power to signal/ground pads. Verify all rows and mounting alignment before power. Fit only one of display or alternative Plus. |
| K1, K2 | MUTE/Wi-Fi slider or expander input stuck | Requested state can be lost. Show read state and test both positions; firmware controls the effect. |
| K3, K5 | Shared I2C or ESP_3V3 fault | Controls, touch, charger control, gauge and barometer can be lost together. Retain last slider states and show a fault. QON reset leaves charger/gauge powered and may not recover a stuck bus. |
| K4 | Wrong voltage or load applied to J10 | Expander or ESP supply can be damaged. J10 carries 3.3 V reset/backlight-control logic and GND only; do not connect display power, raw backlight current or 5 V signals. |
| A1, A2 | Fine-pitch or exposed-pad solder fault | Inspect U4 VQFN-HR-29 and U6/U7/U11/U12/U14 joints. Verify rails before fitting modules; inspect exposed-pad solder quality. |
| A3 | Tilted U4 top sink or conductive bonding material shorts nearby parts | The top sink is part of the selected thermal arrangement. Use electrically insulating bonding material and verify clearance after assembly. |
| A4 | D2 reversed or wrong common-terminal type | Charge indication is wrong. Follow the common-cathode pin map in the BOM and verify LED states. |
| A5 | Hand-soldered pin tails contact the pack | Trim tails and fit the insulator board with adequate spacing. Verify the complete stack and harness route. |

## Acceptance work

[Open issues](open-issues.md) owns pending design and bench work. The key unresolved risks are reversed or misplaced pack leads, silent receiver loss, pack/harness damage, module overvoltage, and assembled thermal behavior. No bench, RF, vibration or enclosure acceptance is claimed by this document.
