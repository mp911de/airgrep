# Power architecture

The active design is `hardware/carrier-ti-charger/`. Its PCB and schematics own connectivity, values and placement. The MAX77757 board is a frozen reference, with no further development planned. Headline figures are in [specs.md](specs.md), rationale in [decisions.md](decisions.md), and pin maps in [interfaces.md](interfaces.md). Simulation reports describe their recorded inputs; they do not override the hardware files.

## Charger (BQ25798)

U4 is a TI BQ25798RQMR (LCSC C2876593, VQFN-HR 29, 4 x 4 mm, 0.4 mm pitch, no exposed pad). It is an I2C charger with a complete default mode: it charges at 1 A with no host, the display's ESP32-S3 raises the current to 1.8 A, and watchdog expiry resets only the registers marked "reset by WATCHDOG" when the host stops servicing it. Data sheet SLUSDV2C in `datasheets/bq25798.pdf`.

```
USB-C -- TVS -- BQ25798 VBUS   (D+/D- BC1.2 on chip; CC on U15 FUSB303B, I2C 0x21)
                   |
battery -- fuse --+-- BQ25798 BAT -- SYS -- TPS61088 boost -- TPS22975 load switch -- 5 V rail
                  |     L1 2.2 uH between SW1 and SW2       EN = thermostat AND slider AND supervisor (latched)
                  +-- MAX17048 gauge -- I2C -- display ESP32 -- I2C -- BQ25798 (0x6B)
                  +-- NTC -- BQ25798 TS (R59 80.6 kOhm from REGN, R60 768 kOhm to GND)
thermostat open -- Q2 -- CHG_DIS = BQ25798 CE (high: charging off)
```

### Operating modes

| Mode | Charge current | TREG | Who sets it |
|---|---|---|---|
| Default (power-on, or watchdog expired) | 1 A | 120 °C | silicon defaults; input limit from D+/D-, clamped to 1.5 A by ILIM_HIZ |
| Host | 1.8 A (ICHG register) | 60/80/100/120 °C, 100 °C planned | ESP32-S3 over I2C, WD_RST well inside 40 s |

- Watchdog: 40 s default (20 to 160 s). On expiry every "reset by WATCHDOG" register returns to its default, so the firmware re-applies the whole profile whenever WD_STAT is set.
- Input: 3.6 to 24 V operating, OVP at 26 V. Input limit from BC1.2: SDP 0.5 A, CDP 1.5 A, DCP 3.25 A, unknown adapter 3 A, Apple/Samsung dividers 1 to 2.4 A. A C-to-C source leaves D+/D- open and reads as unknown (3 A). ILIM_HIZ (R57 120 kΩ from REGN, R58 100 kΩ) clamps IINDPM at 2.2 V, about 1.5 A at REGN 4.8 V, before the ESP boots. The firmware lifts the clamp (EN_EXTILIM = 0) only for a 3 A CC advertisement. EN_EXTILIM is reset by REG_RST only, not by the watchdog. Default VINDPM is VBUS0 - 0.7 V and backs off on a sagging source.
- CC: U15 FUSB303BTMX (onsemi, LCSC C895444). R80 pulls PORT/DEBUG_N low for sink mode and R81 pulls ADDR/ORIENT low for I2C address 0x21; both are 768 kΩ, 1%, 0402 (C49196905, the R60 part). The datasheet reads 1 to 4 MΩ to VDD or GND as float, so the straps stay below 1 MΩ; it recommends 900 kΩ only to limit standby current. When the pins turn into push-pull outputs, each draws 4.3 µA while driving high. EN_N is grounded, INT_N and ID are open. VDD comes from always-on +ESP_3V3 with C57 100 nF. Unpowered CC pins present Rd, so a Type-C source enables VBUS with a dead battery. VBUS_DET connects to +VUSB through R79, 1.5 kΩ, for surge current limiting. The datasheet wires the pin straight to VBUS and specifies a load to GND of 72.4 kΩ minimum, so R79 must stay small: 1.5 kΩ raises the 4.48 V maximum attach threshold to about 4.57 V at the connector. Firmware sets `CONTROL1.ENABLE` and applies the lower of CC advertisement and BC1.2 to IINDPM.
- Timers: fast charge 12 h default (5 to 24 h or off), precharge 2 h, trickle 1 h.
- ADC: 16-bit, die temperature (0.5 °C), IBAT and IBUS (1 mA), VBAT/VSYS/VBUS (1 mV), TS. Off by default, the host enables it.
- STAT: low while charging, high when done, blinking on a fault. Drives the green LED. INT ends on TP8 (polled, no free module GPIO). The red LED shows USB present through Q10, R63 47 kΩ, R64 100 kΩ.
- CE: the thermostat chain ends on CE (R12 100 kΩ pull-down, Q2 pulls it to VSYS when the thermostat opens). CE above 1.3 V disables charging; Q2 leakage at the 25 °C maximum leaves CE at 0.1 V (`hardware/sim/thermal/results_ti.md`).
- System reset: SW4 shorts QON to GND. Holding it for about 10 s turns Q13 off for about 350 ms, with the converter in HIZ and a typical 30 mA SYS discharge sink. Q13 then turns on automatically. These are TI's typical timings, not firmware delays ([BQ25798 section 7.3.12.3](../datasheets/bq25798.pdf)).
- Q13 is the CSD17575Q3 used in TI's [EVM, Figure 4-5](../datasheets/bq2579x-evm.pdf): drain to raw VBAT, source to CHG_BAT and U4 BAT, gate directly to SDRV. C49 is removed. BATP sensing and the MAX17048 remain on raw VBAT. The thermal model includes both copper paths and Q13 losses, with 2.6 mΩ typical and 6.4 mΩ as a conservative hot corner.
- D10 1N4148WS has its anode at ESP_PMID_EN and cathode at QON. It disables U14 while SW4 is held, preventing USB/PMID from keeping the display alive during reset. At 5.25 V, the 1 MΩ pull-up supplies less than 5.3 µA; the diode's maximum forward drop is 0.715 V at 1 mA, below U14's 1.076 V minimum falling EN threshold. A small-signal diode, not a Schottky: with EN low, reverse leakage flows through QON's 200 kΩ internal pull-up to 3.2 V. The 1N4148WS leaks at most 25 nA at 20 V (5 mV drop). A Schottky such as the RB551V-30 reaches about 10 µA at 55 °C, which pulls QON to its 1.3 V high threshold. Cold behavior and rail discharge need bench verification.
- Firmware must set REG14 bit 7, SFET_PRESENT, during initialization and after register reset. Its POR value is zero. This is a system power reset, not a charger or gauge reset; a charger failure that prevents QON handling, or a MAX17048 holding I2C low, is outside its recovery coverage. See [firmware.md](firmware.md#charger-host-control-bq25798-i2c-0x6b).

### Temperature window (TS)

Same Variohm 100 kΩ NTC on J12. RT1 80.6 kΩ (R59) from REGN and RT2 768 kΩ (R60) to GND put the chip's percent-of-REGN thresholds on the 100 kΩ curve so that the charge window, worst case with typical TS leakage, stays inside the pack's 0 to 45 °C rating. C35 10 nF stays on BAT_TS.

| Transition | TS/REGN | Nominal | Worst case (typical leakage) | Effect (defaults) |
|---|---|---|---|---|
| T1 cold | 73.3 % | 3.1 °C (releases 4.7 °C) | 1.1 to 5.0 °C | charging stops below |
| T2 cool | 68.4 % | 8.9 °C | 7.7 to 10.2 °C | 20 % of ICHG below (JEITA_ISETC) |
| T3 warm | 44.8 % | 32.2 °C | 31.1 to 33.2 °C | VREG −400 mV above (JEITA_VSET); current stays at 100 % (JEITA_ISETH default) |
| T5 hot | 34.2 % | 43.0 °C (releases 41.6 °C) | 41.8 to 44.2 °C | charging stops above |

Worst case: parts at 1 %, NTC ±0.2 K and B ±0.5 %, charger thresholds at their limits, TS leakage 0.1 µA typical. With 1 µA (maximum, assumed) the window widens to −0.5 to 44.7 °C. Details in `hardware/sim/thermal/results_ti.md`. The cool and warm points move down with the hot stop (the divider sets one curve); the host can raise T3 to 40 °C and T2 to 15 °C in the TS_T3 and TS_T2 bits.

- Simulated in `hardware/sim/thermal/results_ti.md` with the TS leakage assumed at ±0.1 µA typical, ±1 µA maximum (not specified). Thresholds from the data sheet, T2/T3 options at their defaults (10 °C, 45 °C).
- The hardware window is 3 to 43 °C nominal in default mode too. The leakage assumptions above limit the tolerance claim; verify the assembled board with an NTC resistance sweep.
- Thermistor unplugged: reads cold, charging suspended. Shorted: 0 %, reads hot, charging suspended.

### Components

| Part | Pin or function | Value | Note |
|---|---|---|---|
| C5, C41, C42, C43 | VBUS | 2 x 100 nF 50 V + 2 x 10 µF 25 V | data sheet 2 x 10 µF + 100 nF; C5 was 2.2 µF, merged into the 100 nF part |
| C6, C33, C44 | PMID | 10 µF 25 V + 22 µF 16 V + 100 nF | data sheet 3 x 10 µF |
| C8, C45 | BTST1 to SW1, BTST2 to SW2 | 2 x 47 nF 50 V | |
| L1 | SW1 to SW2 | 2.2 µH, FC-ALX4030D-2R2MT (C5370906) | 10.7 mΩ typical, 7.3 A minimum saturation; 750 kHz; 4.2 x 4.2 x 3.1 mm |
| C9, C10, C46 | SYS | 2 x 22 µF 10 V + 100 nF | data sheet 5 x 10 µF |
| C11 | raw VBAT | 10 µF 25 V | before Q13, near F1 |
| C47 | CHG_BAT | 22 µF 10 V | after Q13, at U4 BAT |
| R61 | BATP sense | 150 Ω to VBAT | high-impedance input |
| C48 | REGN | 4.7 µF 16 V | |
| Q13 | SDRV ship FET | CSD17575Q3, C529277 | replaces C49; no gate pull-down |
| SW4 | QON to GND | 6 x 6 mm momentary, normally open | hand-fitted below BZ1; 6.5 x 4.5 mm pin spacing |
| D10 | U14 reset clamp | 1N4148WS, C2128 | anode EN, cathode QON |
| R56 | PROG | 4.7 kΩ 1 % | 1S, 750 kHz; PWM_FREQ stays at 750 kHz, the only frequency the data sheet allows with the 2.2 µH L1 |
| R57, R58 | ILIM_HIZ | 120 kΩ / 100 kΩ from REGN (C25808, C25803) | 2.2 V, about 1.5 A clamp |
| R59, R60 | TS | 80.6 kΩ / 768 kΩ from REGN (C23249, C49196905) | window 3.1 to 43.0 °C, see above |
| R62, TP8 | INT | 100 kΩ to ESP_3V3 | polled |
| Q10, R63, R64 | USB present | 2N7002, 47 kΩ / 100 kΩ | red LED |
| U15, R79-R81, C57 | CC1, CC2, VBUS_DET, straps, VDD | FUSB303BTMX (C895444), 1.5 kΩ, 2 x 768 kΩ (C49196905), 100 nF | usb_input sheet |


### Layout

Use the active PCB and its `carrier.kicad_dru` for geometry and exceptions. Run `tools/check.sh` for current ERC, DRC and schematic parity; stored review verdicts are snapshots.

### Heatsink

U4 has no exposed pad. The selected top sink is Fischer ICK S 10 x 10 x 6.5, or ICK BGA 10 x 10 x 6, bonded through a 1.5 mm copper spacer and two thin thermal-epoxy layers. The spacer must clear neighbouring components; verify the assembled stack. The TI package specifies RθJC(top) 20.9 K/W. No bottom-side plate is part of the design.

### Heat

The enclosure model assumes four 8 cm² vents, two lower inlets and two upper outlets, 50 % open grilles and 35 mm vertical separation. [TI heat results](../hardware/sim/heat/ti/results.md) contain the losses and temperatures for their recorded board snapshot. Charging while running can reach the pack's charge-temperature stop. Validate the actual enclosure and heatsink on the bench.

Transient charging reports are generated by `run_charge_heat.py` and `run_charge_limits.py`. Check their recorded board hash before using their timing estimates.

### EMI

[TI EMI results](../hardware/sim/emi/results_ti.md) estimate converter coupling and emissions. L1 is 2.2 µH and R56 selects 750 kHz; the report's 1.5 MHz comparison requires a different 1 µH L1. Candidate comparisons do not describe fitted hardware. Converter edge rates, FB1 under DC bias and receiver desense remain bench checks.

## Module feeds

U11/U12/U14 use TPS259474LRPWR (C2864845) in the shared RPW0010A footprint. R70/R71/R72 are 1.65 kΩ, 1%, setting a nominal circuit-breaker threshold of 3334 / 1650 = 2.02 A, approximately 1.78–2.22 A including the specified 1.8–2.2 A threshold range and resistor tolerance. C56/C54/C55 are 2.2 nF, 50 V X7R, setting nominal slew to 2000 / 2200 = 0.909 V/ms, about 5.6 ms to 5.06 V. ITIMER is open for the shortest trip delay; the L variant latches off after overcurrent or thermal shutdown until IN is power-cycled or EN/UVLO is pulled below its shutdown threshold. Overvoltage clears when IN falls below the divider release threshold. Existing enable dividers and Q12 remain.

R73/R74, R75/R76 and R77/R78 implement IN-to-OVLO-to-GND dividers on U11, U12 and U14 respectively. U11/U12 sense RAIL_5V; U14 senses CHG_PMID. U11/U12 use 5.6 kΩ above 1.5 kΩ, both 1 %, ±100 ppm/°C, JLCPCB basic parts: R73/R75 0603WAF5601T5E ([C23189](https://www.lcsc.com/product-detail/C23189.html)), R74/R76 0402WGF1501TCE ([C25867](https://www.lcsc.com/product-detail/C25867.html)). Their trip is 5.426–5.976 V and release 4.935–5.453 V, between the boost's 5.25 V load-step maximum and the display's 6 V absolute maximum. The two bands overlap. U14 sees the USB input, specified at 4.75–5.5 V, with USB PD load transients up to 6.0 V for 5 ms. Its trip must stay between 5.5 V and the display SGM6029's 6 V absolute maximum, a window 1 % parts cannot meet. R77 is 4.7 kΩ 1 % basic 0603WAF4701T5E ([C23162](https://www.lcsc.com/product-detail/C23162.html)); R78 is 1.24 kΩ 0.1 %, ±25 ppm/°C, ARG02BTC1241 ([C2984409](https://www.lcsc.com/product-detail/C2984409.html)). Trip is 5.562–5.969 V, release 5.059–5.447 V. A 6.0 V PD transient trips U14 by design. Independent resistor tolerance and drift over −40 to +125 °C, IC threshold limits and ±0.1 µA OVLO leakage are included in all corners. Returning to 5 V does not guarantee recovery after an overvoltage fault: cycle POWER for U11/U12 or disconnect and reconnect USB for U14, allowing the input to discharge.

The display's XIAO SGM6029 regulator permits 5.5 V in operation and 6 V absolute maximum ([manufacturer datasheet](https://www.sg-micro.com/rect/assets/3323f5dc-718f-4b81-b820-398ddb702989/SGM6029.pdf)); the ADSBee 1090U schematic specifies a 3.7–6.5 V input. These limits do not certify every module component or a fault waveform. The divider does not hold the output below 5.5 V: OVLO response is 1.2 µs typical, and transient overshoot still needs measurement. U7, upstream of the eFuses, remains exposed to a boost fault and is rated 6 V absolute maximum.

The power and thermal models use 28.2 mΩ typical and 45 mΩ maximum on-resistance. Forward regulation is 16.9 mV typical; the 28.4 mV high model corner is an assumption because this datasheet specifies no maximum. The power report checks static OVLO corners; its transient benches omit OVLO switching. These models do not validate eFuse startup, inrush, trip/retry timing or module capacitance; those require prototype measurements. The heat model includes OVLO divider dissipation, about 3.7 mW per boost feed at 5.06 V and 3.6 mW on the USB feed at 5 V.

## Input protection

J14 feeds USB_VBUS_J1 through D3 (SMAJ12A), C1 (4.7 µF) and FB1 (BLM31KN471SZ1L) into the charger input. D8/D9 protect D+/D-. U15 handles CC detection. The input is used at 5 V only, without PD negotiation.

The charger provides reverse blocking and input-current control. D3 is a surge clamp, not a 5 V regulator. U14 protects the USB/PMID display feed; its OVLO and recovery limits are described above. This does not establish survival of an arbitrary faulty source.

## Battery pack

The selected packs and their data are in [specs.md](specs.md). The design against the 10 Ah pack (`datasheets/AKKU_SOLD333290_DB-EN.pdf`):

| Parameter | Pack | Design |
|---|---|---|
| Charge voltage | 4.2 V standard, 4.25 V cut-off | 4.20 V |
| Charge current | 5 A standard, 10 A maximum | 1.8 A |
| Charging temperature | 0 to 45 °C | 3.1 to 43.0 °C nominal; see TS tolerances above |
| Discharge current | 5 A standard, 10 A maximum | 1.55 A at 0.8 A load, 1.94 A at 1 A load, both at the 2.9 V cut-off |
| Discharge cut-off | 2.5 V (PCM) | 2.9 V on SYS |
| Discharge temperature | -10 to 60 °C | thermostat opens at 60 °C ±5 K |

At the 1.5 A output headroom the battery current is about 2.8 A at 3.1 V. This is an estimate from 7.6 W output at about 88 % efficiency, not a simulated case. The headroom is short-term, not a continuous rating.

## Battery protection

### Connector and fuse

- J11 is a JST-PH header, rated 2 A per contact with AWG 24 wire (see [interfaces.md](interfaces.md)). The battery current stays below 2 A up to the 1 A load and exceeds it at the 1.5 A headroom.
- F1 is a Littelfuse 0466005.NRHF (5 A, 32 V, very fast acting, 1206, 11 mΩ, 2.5 A²s; LCSC C57525; `datasheets/littelfuse-466.pdf`). Derated for continuous use and 60 °C it carries about 3.4 A.
- The pack's own PCM is the second line of protection.
- Reverse-polarity isolation is planned using two CSD25404Q3 MOSFETs (LCSC C2865523), without an indicator LED; see [issue 0002](../issues/0002-battery-reverse-polarity-protection.md). The current schematic and PCB still rely on checking J11 polarity before connection. The control circuit and its interaction with charging remain to be designed and validated.

### Undervoltage cut-off

U5 TLV840CADL29 (LCSC C3686404, 2.9 V, open drain, CT open) monitors SYS and pulls PWR_EN low. With USB present the charger holds SYS at 3.5 V or more, so the device runs from USB with an empty or missing pack.

A latch keeps the system off after a trip. Without it the system restarts when SYS rebounds: the load drop across pack, harness and switch (225 to 550 mV simulated) exceeds the supervisor's hysteresis (145 mV typical).

| Part | Value | Function |
|---|---|---|
| Q7 | AO3401A | source on HOLD, gate on PWR_EN; sets LATCH when U5 pulls PWR_EN low |
| Q8 | 2N7002 | gate on LATCH; holds PWR_EN low |
| R40, C37 | 10 kΩ, 100 nF | HOLD is SW_EN delayed by 1 ms, so U5's 40 µs power-on pulse cannot set the latch |
| R41 | 100 kΩ | LATCH to GND; keeps Q7 leakage below the Q8 threshold |
| Q9, R42 | 2N7002, 1 MΩ | gate from USB_VBUS; clears and inhibits the latch while USB is present |

- To clear the latch, open and close the slider or plug in USB. An opening thermostat clears it as well. HOLD discharges within 0.5 s.
- Latched current is about 60 µA.
- Cut-off with typical pack and harness resistance: 3.20 V cell open-circuit voltage at 0.8 A load, 3.28 V at 1 A.
- Simulated in `hardware/sim/power/undervoltage.cir`: one trip per discharge and no restart, at every path and hysteresis corner (`hardware/sim/power/results_ti.md`).

## Thermal protection

The Limitor LIM-P-82-60 thermostat is normally closed, auto-resetting, 60 °C ±5 K, taped to the pack and connected to J13.

```
VSYS -- R43 1 kΩ -- J13 thermostat -- THERM_OK
THERM_OK -- SW1 -- SW_EN -- R13 -- PWR_EN (U6 and U7 enable)
THERM_OK -- Q2 gate; Q2 source VSYS, drain CHG_DIS -- U4 CE
CHG_DIS -- R12 100 kΩ -- GND
THERM_OK -- R10 1 MΩ and C31 100 nF -- GND
```

An open or disconnected thermostat stops the receiver rail and raises CE to disable charging. The display can remain powered from USB through U14, with the POWER slider off. Q6 belongs to the frozen MAX circuit and is absent here.

R43 limits a grounded harness fault to about 4.2 mA. R12 holds CE low when Q2 is off. The thermal model gives about 0.093 V at 1 µA leakage; at the 55 °C leakage limit of 5 µA it gives 0.464 V, between CE's guaranteed low and high limits. Verify hot leakage and thermostat operation on hardware. See [TI thermal results](../hardware/sim/thermal/results_ti.md).

## Off-state draw

The existing budget totals about 36 µA typical and 50 µA maximum without USB. It omits the integrated GNSS backup supply and other unbudgeted leakage, so these are partial estimates, not a complete-board limit. Measure the assembled board; the breakdown is in [specs.md](specs.md).

## Boost and load switch

U6 TPS61088RHLR (LCSC C87357) converts SYS (3.1 to 4.4 V) to 5.06 V. The load is 0.8 A nominal and 1 A expected peak, with 1.5 A short-term headroom. U7 TPS22975DSGR (LCSC C524777) disconnects the rail, because a disabled TPS61088 still conducts from input to output.

| Part | Function | Value | Result |
|---|---|---|---|
| L2 | inductor | 2.2 µH, FC-ALX4030D-2R2MT (C5370906) | 10.7 mΩ typical, 7.3 A minimum saturation; 4.2 x 4.2 x 3.1 mm |
| R14, R15 | feedback | 240 kΩ, 75 kΩ, 1 % | 5.06 V with VREF 1.204 V (PWM mode) |
| R16 | switching frequency, FSW to SW | 220 kΩ | 710 to 732 kHz over 3.1 to 4.4 V |
| R17 | current limit | 160 kΩ | 5.84 A typical, 4.54 A minimum |
| R18, C21, C22 | compensation | 6.8 kΩ, 10 nF, 100 pF | see [Simulated behaviour](#simulated-behaviour) |
| C17 | soft start | 47 nF | 11.3 ms to regulation |
| C18 to C20, C24 | output capacitors | 3 x 22 µF, 10 µF | 38 µF nominal after DC bias, 25 µF minimum |
| C53 | HF bypass at VOUT | 100 nF 50 V 0603 (C14663) | next to C18, see open-issues for the value |
| C23 | load switch rise, on CT | 10 nF | 17 ms |
| R39, C36 | snubber, SW at L2 pin 2 to GND | not fitted; start with 2.2 Ω, 1 nF | |

- MODE is tied to GND (forced PWM, fixed frequency).
- PWR_EN enables U6 and U7 together. The load switch rises slower than the boost soft start, so rail inrush does not reach the boost current limit.
- BOOST_5V stays charged behind the open load switch after shutdown.
- SW1 (HRO K3-1280S-F1, LCSC C92658) carries control current only. Terminal 2, the middle pad, is the common.
- Fit the snubber only if the bench shows switch-edge ringing in the receiver bands. The EMI estimate (`hardware/sim/emi/results_ti.md`) puts the ring at about 80 MHz with the board's loop inductance and the edge tail of both converters within a few dB of the receiver noise floors; the real amplitude depends on the IC's edges and switch capacitance.
- L1 and L2 are 13 mm apart, centre to centre. Simulated magnetic pickup is at most 0.08 mV into the TS network and 0.03 mV between the inductors.

### Simulated behaviour

Worst case over 3.1 to 4.4 V input and all component corners, from `hardware/sim/power/results_ti.md` (720 kHz, capacitors derated for DC bias). The file has the full tables.

| Item | Result |
|---|---|
| Phase margin | 60° or more |
| Gain margin | 14 dB or more |
| Crossover against fSW/10 and RHPZ/5 | at most 0.72 of the limit up to 1 A; 1.07 at 1.5 A |
| Peak inductor current | 3.20 A at 3.1 V and 1.5 A; 3.35 A in the 1.5 A pulse bench; limit 4.54 A minimum |
| Output ripple | at most 28 mV peak to peak |
| Start-up | module terminal at 4.75 V about 20.6 ms after PWR_EN, no overshoot |
| Load steps 0.1 to 0.8 A and 0.8 to 1 A at 3.1 V | terminal between 4.792 and 5.246 V across both diode corners |
| 1.5 A pulses from 0.8 A | terminal not below 4.748 V |

- U11 (ADSBee) and U12 (display) have local 100 nF 0603 input bypass capacitors, C39 and C38 respectively (LCSC C14663), from RAIL_5V to GND.
- The transient and undervoltage models include both TPS259474L feeds, regulated forward drop, on-resistance and reverse blocking. At the assumed high diode-drop corner the load-step minimum is 4.792 V, leaving 42 mV above the assumed 4.75 V limit. The typical corner leaves 53 mV.
- The transient results assume 40 µF of total input capacitance, split with the load as 83.5 % ADSBee/GNSS and 16.5 % display. These quantities and the module voltage window are unmeasured. The model omits diode switching delays and parasitics.
- The boost loss figures rest on a switching term fitted to TI's efficiency curve, not on a measurement.

### Assembly of U6 and U7

Dimensions from the footprints on the board.

| | U6 TPS61088 | U7 TPS22975 |
|---|---|---|
| Exposed pad | 3.0 x 2.1 mm | 1.6 x 0.9 mm |
| GND vias in the pad | three in one row along the long axis | one in the centre |
| Via | 0.71 mm pad, 0.30 mm drill, unfilled | 0.71 mm pad, 0.30 mm drill, unfilled |
| Paste windows | four of 1.3 x 0.745 mm, two on each side of the via row | two of 0.8 x 0.465 mm, one on each side of the via |
| Paste coverage of the pad | 61.5 % | 51.7 % |
| Paste to drill edge | 0.10 mm | 0.10 mm |

- U6 has five more GND vias outside the pad: three at pins 11 to 13, two at pin 20.
- Both footprints are local library copies with these paste apertures. No paid via-fill process is selected.
- Use a 0.10 mm stencil. Ask the assembler to keep the paste windows and their drill clearance during CAM review.
- Paste kept off the holes does not prevent all solder wicking. Inspect the exposed-pad joints.

## Board

### Stackup

Four layers, JLCPCB JLC04161H-2116C, 1.6 mm: 1 oz outer, 0.5 oz inner copper, 0.20 / 1.065 / 0.20 mm dielectric.

| Layer | Use |
|---|---|
| F.Cu | components, signals, GND pour, filled polygons for VSYS, BOOST_SW, BOOST_5V |
| In1 | solid GND plane |
| In2 | power tracks (VSYS, RAIL_5V, ESP_5V, USB_VBUS) with GND fill |
| B.Cu | signals, GND pour, filled power polygons |

### Board-derived results

Copper path resistances, losses and temperatures belong in [TI heat results](../hardware/sim/heat/ti/results.md). Regenerate geometry-dependent reports after board changes; compare the calibration and report hashes with the PCB before using numerical results. Stored reports are model evidence, not physical acceptance.

## Gauge

U8 MAX17048 (LCSC C2682616) is supplied from VBAT, behind F1, and answers at I2C address 0x36. The bus and its pull-ups are described in [interfaces.md](interfaces.md).

## Indication

Two 3 mm through-hole LEDs next to the USB port, fitted by the user. What the user sees is in [specs.md](specs.md).

| LED | Part | Supply | On when |
|---|---|---|---|
| D5 green | Kingbright L-7104GD | RAIL_5V through R26 470 Ω | the 5 V rail is on |
| D4 red, pin 1 | Kingbright L-115WEGW | VSYS through Q4 and R23 470 Ω | USB present: Q10 pulls USB_OK_N low (R36 100 kΩ to VSYS) |
| D4 green, pin 3 | Kingbright L-115WEGW | VSYS through Q3 and R31 470 Ω | STAT is low (pull-up R33 47 kΩ to VSYS) |

- D4 is common cathode on the middle leg (pin 2 to GND). Q3 and Q4 are AO3401A.
- With no USB source Q10 is off and STAT is high impedance, so both LED FETs are off, so D4 draws nothing from the pack.
- LED current is 2.7 to 4.3 mA.
- STAT is not wired to the display. The ESP polls BQ25798 charging status; MAX17048 supplies state of charge.

## Parts sourcing

Passives use JLCPCB Basic stock where a Basic value exists. Expected outside Basic stock:

- the ICs U4 to U18
- the X7R bulk capacitors
- L1, L2, F1, D3, D8, D9
- J14, SW1, SW2, SW3, BZ1

The JLCPCB class of each part is not recorded in the project and is not verified here. Check it at the BOM export. Hand-fitted parts are in [bom-hand-sourced.md](bom-hand-sourced.md).
