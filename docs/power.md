# Power architecture

The power design of the carrier board. Headline figures are in [specs.md](specs.md), reasons in [decisions.md](decisions.md), connectors in [interfaces.md](interfaces.md), open work in [open-issues.md](open-issues.md). Simulated values come from `hardware/sim/*/results.md`; those files are the reference when a number here differs.

```
USB-C -- TVS -- MAX77757 CHGIN   (CC and D+/D- detection on chip, no PD)
                    |
battery -- fuse --+-- MAX77757 BATT -- SYS -- TPS61088 boost -- TPS22975 load switch -- 5 V rail
                  |                           EN = thermostat AND slider AND supervisor (latched)
                  +-- MAX17048 gauge -- I2C -- display ESP32
                  +-- NTC -- MAX77757 THM
SYS -- R43 -- thermostat contact -- THERM_OK
5 V rail -- LM73100 U12 -- shunt JP1 -- display VBUS
5 V rail -- LM73100 U11 -- shunt JP2 -- ADSBee 5VIN
ADSBee 3V3 out -- GNSS 3.3 V
```

- Battery path: one conversion (boost).
- USB path: two conversions (buck to SYS, boost to 5 V), about 83 % at 0.8 A load (`hardware/sim/heat/results.md`). The loss is heat and does not affect runtime.
- The loads run from the 5 V rail only. The ADSBee's AZ1117 LDO needs 3.7 V, which SYS does not guarantee.
- The charger's reverse boost (OTG) is disabled. ENBST is tied to GND.

## Charger

U4 is an ADI MAX77757JEFG420+T (LCSC C7454852, FC2QFN-24, 3 x 3 mm, 0.4 mm pitch). It runs standalone, without I2C or firmware.

### Input

| Item | Value |
|---|---|
| Operating input | 4.5 to 13.7 V |
| Over-voltage lockout | 13.7 V |
| Absolute maximum | 16 V on CHGIN; 6 V on CC, D+, D- |
| Input current limit | 0.5 to 3.0 A, the lower of the CC and the BC1.2 result |
| Type-C advertisement (CC) | 0.5 / 1.5 / 3.0 A |
| BC1.2 (D+/D-) | SDP 0.5 A; CDP and DCP 1.5 A; Apple dividers 0.5 to 2.4 A; Samsung 2 A |
| Unknown source or open D+/D- | 3 A, bounded by the CC result |
| High-voltage requests | none (no PD, no 9 V or 12 V handshake) |

- Dead battery: a clamp on the CC pins acts as Rd while the chip is unpowered and hands over to the 5.1 kΩ resistor within 15 ms of power-up. A Type-C source therefore enables VBUS with an empty pack. No external CC pull-downs are fitted.
- Input regulation (AICL): the input current reduces when CHGIN sags to 4.5 V, so a weak source does not collapse.
- Power path: the system is supplied first and the remainder charges the cell. The battery supplements a weak source; SYS is then 70 mV below VBAT. With a source present SYS is at least 3.5 V (VSYSMIN).
- Charging from a weak source may reach the safety timer before the cell is full. There is no automatic retry.

### Charge profile

| Item | Value |
|---|---|
| Fast-charge current | 1.8 A nominal (12.4 kΩ on IFAST, MAX77757 Table 3) |
| Top-off current | 75 mA |
| Termination voltage | 4.20 V, fixed by the variant |
| Trickle | 300 mA below 3.1 V |
| Precharge | 55 mA below 2.5 V |
| Prequalification timer | 30 min |
| Fast-charge timer | 6 h |
| Restart | 100 mV below termination |

Source limit, system load, JEITA and thermal regulation can reduce the charge current.

### Temperature window (JEITA)

The pack thermistor is a Variohm ENTC-EB-100K4262-02 (100 kΩ, B25/85 4262 K ±0.5 %, ±0.2 °C from 0 to 70 °C) on J2, from THM to GND. R8 154 kΩ biases it from PVL. C35 10 nF at the THM pin filters RF pickup on the leads.

| Transition | THM/PVL | Nominal | Worst case | Effect |
|---|---|---|---|---|
| Cold | 74 % | -2.9 °C | -5.5 to -0.6 °C | charging stops below |
| Cool | 65 % | 4.6 °C | 2.4 to 6.7 °C | half current (0.9 A) below |
| Warm | 32.5 % | 31.4 °C | 29.2 to 33.6 °C | termination 4.05 V above |
| Hot | 23 % | 42.1 °C | 39.4 to 45.0 °C | charging stops above |

- Temperatures are simulated (`hardware/sim/thermal/results.md`). The worst case covers thermistor, R8 and threshold tolerances at the typical THM leakage of 0.1 µA.
- Thresholds are ±1.5 points of PVL with 1 point hysteresis (data sheet).
- At the data sheet maximum leakage of 1 µA the outer limits move to 47.2 and -24.1 °C.
- The Variohm data sheet has no R/T table. The nominal figures follow the B-value model and may be off by about 1 °C.
- The cold cut-off lies below the pack's 0 °C charging limit. This is accepted, see [decisions.md](decisions.md).
- Thermistor unplugged: reads cold, charging is suspended. THM below 6 % of PVL (thermistor shorted, or Q6 on): the charger is disabled.

### Protection and status

- Battery overcurrent: the BATT-to-SYS switch opens above 6 A for 6 ms.
- Quiescent current on battery: 30 µA typical, 50 µA maximum.
- INOKB (open drain): low while the input is valid and the buck runs.
- STAT (open drain): 1 Hz toggle while charging, low in top-off and done, high impedance otherwise (no input, fault, timer expiry, disabled).
- There is no CE pin. The thermostat disables the charger by pulling THM to GND through Q6, see [Thermal protection](#thermal-protection).

### Components

| Part | Pin or function | Value | Note |
|---|---|---|---|
| C5 | CHGIN | 2.2 µF 25 V | |
| C6, C33 | BYP | 10 µF 25 V + 22 µF 16 V | matches data sheet Table 4; DC-bias derating unverified |
| C9, C10 | SYS | 2 x 22 µF 10 V | data sheet asks for 2 x 10 µF |
| C11 | BATT | 10 µF 25 V | |
| C7 | VDD | 2.2 µF 25 V | 1.8 V internal rail, no external load |
| C34 | PVL | 2.2 µF 25 V | 1.8 V internal rail, loaded by R8 only |
| C8 | BST to LX | 100 nF | |
| L1 | LX to SYS | 1 µH, FXL0630-1R0-M (C167216) | 7.4 mΩ, 12 A saturation; data sheet range 0.47 to 1 µH |
| R6 + R37 | IFAST | 6.2 kΩ + 6.2 kΩ, 1 % (C4260) | 12.4 kΩ |
| R8 | THM bias from PVL | 154 kΩ, 1 % (C22881) | |

All bulk ceramics are X7R (125 °C):

| Value | Part | LCSC | Used for |
|---|---|---|---|
| 4.7 µF 50 V 0805 | Murata GRM21BZ71H475KE15L | C437557 | C1 |
| 2.2 µF 25 V 0603 | Murata GRM188Z71E225KE43D | C415535 | C5, C7, C15, C34 |
| 10 µF 25 V 0805 | Samsung CL21B106KAYQNNE | C3039694 | C6, C11, C24 to C26 |
| 22 µF 10 V 0805 | Murata GRM21BZ71A226ME15L | C907991 | C9, C10, C13, C14, C18 to C20 |
| 22 µF 16 V 1206 | [Taiyo Yuden EMK316BB7226ML-T](https://ds.yuden.co.jp/TYCOMPAS/ut/detail?pn=EMK316BB7226ML-T) | C385906 | C33 |

The Murata 22 µF 10 V parts lose 52 % of their capacitance at 5 V and 44 % at 4.2 V (`datasheets/murata-grm21bz71a226.pdf`). The simulations use the derated values.

L1 uses the footprint `IND-SMD_L7.0-W6.6_FXL0630` (pads 2.72 x 3.6 mm). It covers both the vendor land pattern and the terminal position of the JLCPCB part.

### Layout

Distances are pad centre to pin centre, measured on the board.

| Part | Pin | Distance |
|---|---|---|
| C8 | BST (1), LX (21) | 1.8 mm, 1.6 mm |
| C34 | PVL (17) | 2.9 mm |
| C7 | VDD (10) | 3.4 mm |
| C33, C6 | BYP (22) | 4.3 mm, 2.1 mm |
| C11 | BATT (13) | 4.0 mm |
| C14, C10, C9 | SYS (15, 16) | 12.1 mm, 14.3 mm, 18.1 mm |

- The SYS capacitors are farther out than data sheet Figure 13 asks for. The pin order and the L1 body leave no room at the pins.
- The package has no exposed pad. Junction heat leaves through the pins.
- F.Cu zones "U4 GND heat", "U4 VSYS heat", "U4 VBAT heat" and "U4 CHGIN heat2" tie the pins to wider copper with solid connections.
- Four GND vias at the package reach the In1 plane: one under the body, one at the PGND corner (pins 18, 19), two left of pins 8 and 9.
- In2 carries the zones "U4 VSYS In2 heat2" and "U4 VBAT In2 heat2" under U4, each fed by vias next to the pins.
- Rule areas named FINE_* relax clearances and via sizes locally. The rules are in `hardware/kicad/carrier.kicad_dru`.

### Heatsink

A 10 x 10 mm aluminium sink (Fischer ICK S 10 x 10 x 6.5, or ICK BGA 10 x 10 x 6) sits on the package top, bonded with thermal epoxy to a 1.5 mm copper spacer. The spacer lifts the sink over the 0805 and 1206 capacitors, which are taller than the 0.65 mm package. C6, C7, C8, C11, C33, C34, R6 and R8 lie under the sink. The stack is about 8.9 mm high, 0.4 mm above the top of the module sockets; no module overhangs U4.

Heat model, 3 A source, charging with 0.8 A load, 25 °C ambient (`hardware/sim/heat/results.md`):

| Condition | Cell 3.7 V | Cell 3.2 V |
|---|---|---|
| Open bench, with sink: U4 junction at 1.8 A | 70 °C | 76 °C |
| Open bench, bare: U4 junction at 1.8 A | 92 °C | 102 °C |
| Closed case, with sink: charge current | 1.80 A at 118 °C | 1.80 A at 127 °C |
| Closed case, bare: charge current | 1.59 A at 130 °C | 1.35 A at 130 °C |

- The closed case includes the selected 5 cm² inlet and 5 cm² outlet. These nominal-corner figures model junction regulation only; pack JEITA may stop charging first.
- Charging with the device off in the closed case: 1.8 A, U4 at 62 °C.
- On battery U4 dissipates under 0.1 W and the sink makes no difference.
- The sink path is an estimate. Junction to package top is not published for this package; the model assumes 35 K/W.

### Earlier charge limiting

The [transient graphs](../hardware/sim/heat/charging.pdf) model battery heating delay and JEITA cutoff for both packs. [Assumptions and results](../hardware/sim/heat/charging.md) distinguish estimated timing from measured behaviour. The 90-100 °C limiter and external spreader paths are proposals, not fitted hardware.

The MAX77757's native 130 °C threshold is fixed. Options:

- Lower the IFAST resistance total from 12.4 kΩ (1.8 A) to 8.2 kΩ (1.2 A) or 6.65 kΩ (1 A). This also lowers the termination-current threshold from 75 to 50 mA. Values are from [Table 3](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX77757.pdf).
- Add a [TMP302 temperature switch](https://www.ti.com/product/TMP302) beside U4 and use its output to disable charging through THM, preserving the pack NTC. Selectable thresholds and hysteresis allow an earlier stop and restart. Sensor placement, junction offset, supply and output interface need circuit design and bench checks.
- Use a sensor and switched IFAST resistors for staged reduction, then THM for complete charge suspension. Runtime IFAST switching is not explicitly characterised in the datasheet; verify it with ADI and on the bench. The plotted continuous taper is an ideal target, not a claim that IFAST supports an analogue control voltage.

PCB or heatsink temperature is below U4 junction temperature by a load-dependent amount. A 95 °C sensor threshold does not establish a 100 °C junction ceiling. Keep the pack NTC and thermostat independent. Charging suspension leaves the USB system load running.

Heat-spreader materials to evaluate:

| Part | Purpose | Limit |
|---|---|---|
| [Würth WE-TGS 4051100100017](https://www.we-online.com/components/products/datasheet/4051100100017.pdf) | 100 x 100 mm, 37 µm graphite laminate, spreads heat across a surface | 120 °C maximum. Conductive graphite requires separate insulation; do not rely on the adhesive as electrical protection. |
| [Würth WE-TGF 40016005](https://www.we-online.com/components/products/datasheet/40016005.pdf) | 100 x 100 x 0.5 mm, 6 W/mK insulating gap pad between PCB and metal spreader | Fills a gap; it is not a lateral heat spreader. Select thickness for the actual clearance. |
| [Würth sample kit 400002](https://www.we-online.com/en/components/products/THERMAL_SAMPLE_KIT_HEAT_SPREADING_SOLUTIONS) | Graphite sheet and graphite foam gasket samples | Useful for comparing handling and fit. |

For a first prototype, compare a thin copper or aluminium plate plus an insulating gap pad with graphite. Connect the spreader to exposed metal or outside airflow. A strip's lateral resistance depends on thickness and width, not just conductivity. Keep metal clear of the module antennas and avoid a thermal bridge into the battery.

## Input protection

| Part | Net | Value | Note |
|---|---|---|---|
| D3 | USB_VBUS | SMAJ12A, unidirectional, cathode to VBUS | breakdown 13.3 V minimum, clamp 19.9 V at 20.8 A |
| D6 to D9 | CC1, CC2, D+, D- | H5VLD3B, 5 V, SOD-323 (C20615805) | at the connector |
| C1, C5 | USB_VBUS_J1, USB_VBUS | 4.7 µF at J1, 2.2 µF at U4 | 6.9 µF in total, below the 10 µF hot-plug limit |
| FB1 | USB_VBUS_J1 to USB_VBUS | Murata BLM31KN471SZ1L, 470 Ω at 100 MHz, 4 A, 20 mΩ, 1206 (C710518) | with C1, keeps the charger's 1.3 MHz input ripple off the USB cable |

- A USB-C source does not deliver more than 5 V without PD, so D3 only sees ESD and surge.
- The BYP capacitors are behind the charger's input switch and do not count as VBUS capacitance.
- J1, D3 and C1 sit on USB_VBUS_J1, ahead of FB1. The EMI estimate gives 13 to 32 dB conducted margin with FB1 and -2 dB with FB1 replaced by 0 Ω (`hardware/sim/emi/results.md`). FB1's inductance at 3 A is an estimate.
- Reverse blocking, over-voltage lockout and the input current limit are functions of the charger.

## Battery pack

The qualified packs and their data are in [specs.md](specs.md). The design against the 10 Ah pack (`datasheets/AKKU_SOLD333290_DB-EN.pdf`):

| Parameter | Pack | Design |
|---|---|---|
| Charge voltage | 4.2 V standard, 4.25 V cut-off | 4.20 V |
| Charge current | 5 A standard, 10 A maximum | 1.8 A |
| Charging temperature | 0 to 45 °C | -2.9 to 42.1 °C nominal, hot cut-off at most 45.0 °C |
| Discharge current | 5 A standard, 10 A maximum | 1.55 A at 0.8 A load, 1.94 A at 1 A load, both at the 2.9 V cut-off |
| Discharge cut-off | 2.5 V (PCM) | 2.9 V on SYS |
| Discharge temperature | -10 to 60 °C | thermostat opens at 60 °C ±5 K |

At the 1.5 A output headroom the battery current is about 2.8 A at 3.1 V. This is an estimate from 7.6 W output at about 88 % efficiency, not a simulated case. The headroom is short-term, not a continuous rating.

## Battery protection

### Connector and fuse

- J3 is a JST-PH header, rated 2 A per contact with AWG 24 wire (see [interfaces.md](interfaces.md)). The battery current stays below 2 A up to the 1 A load and exceeds it at the 1.5 A headroom.
- F1 is a Littelfuse 0466005.NRHF (5 A, 32 V, very fast acting, 1206, 11 mΩ, 2.5 A²s; LCSC C57525; `datasheets/littelfuse-466.pdf`). Derated for continuous use and 60 °C it carries about 3.4 A.
- The pack's own PCM is the second line of protection.
- Reverse-polarity isolation is planned using two CSD25404Q3 MOSFETs (LCSC C2865523), without an indicator LED; see [issue 0002](../issues/0002-battery-reverse-polarity-protection.md). The current schematic and PCB still rely on checking J3 polarity before connection. The control circuit and its interaction with charging remain to be designed and validated.

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
- Simulated in `hardware/sim/power/undervoltage.cir`: one trip per discharge and no restart, at every path and hysteresis corner (`hardware/sim/power/results.md`).

## Thermal protection

The thermostat is a Limitor LIM-P-82-60-5-100-10A: normally closed, auto reset, 60 °C ±5 K, taped to the pack. A user-made lead connects it to J4.

```
VSYS -- R43 1 kΩ -- THERM_FEED -- J4.1 [thermostat] J4.2 -- THERM_OK
THERM_OK -- SW1 slider -- SW_EN -- R13 100 kΩ -- PWR_EN (boost and load switch enable)
THERM_OK -- Q2 AO3401A gate (source VSYS), drain = CHG_DIS, R12 100 kΩ to GND
CHG_DIS  -- Q6 2N7002 gate, drain = charger THM
THERM_OK -- R10 1 MΩ and C31 100 nF to GND
```

| Thermostat | THERM_OK | Loads | Charger |
|---|---|---|---|
| Closed | VSYS less a few mV | slider decides | Q2 and Q6 off, THM free, charging allowed |
| Open or unplugged | low (R10) | off | Q2 on, CHG_DIS high, Q6 pulls THM to GND, charger disabled |

- R43 limits a short of the thermostat lead to GND to 4.2 mA.
- R12 holds the Q6 gate at 0.5 V or below at Q2's maximum leakage (5 µA at 55 °C). It carries 42 µA while the thermostat is open.
- C31 filters noise on the high-impedance thermostat lead.
- The loads stay off without the thermostat lead, even with USB present. USB-only operation therefore needs the thermostat connected to J4.
- Simulated in `hardware/sim/thermal/thermostat_trip.cir`.

## Off-state draw

With the slider off and no USB the board draws about 51 µA from the pack, 0.4 % of the 10 Ah pack per month. The breakdown is in [specs.md](specs.md).

## Boost and load switch

U6 TPS61088RHLR (LCSC C87357) converts SYS (3.1 to 4.4 V) to 5.06 V. The load is 0.8 A nominal and 1 A expected peak, with 1.5 A short-term headroom. U7 TPS22975DSGR (LCSC C524777) disconnects the rail, because a disabled TPS61088 still conducts from input to output.

| Part | Function | Value | Result |
|---|---|---|---|
| L2 | inductor | 2.2 µH, CKST0603 (C3002634) | 15.5 mΩ, 10 A saturation |
| R14, R15 | feedback | 240 kΩ, 75 kΩ, 1 % | 5.06 V with VREF 1.204 V (PWM mode) |
| R16 | switching frequency, FSW to SW | 220 kΩ | 710 to 732 kHz over 3.1 to 4.4 V |
| R17 | current limit | 160 kΩ | 5.84 A typical, 4.54 A minimum |
| R18, C21, C22 | compensation | 6.8 kΩ, 10 nF, 100 pF | see [Simulated behaviour](#simulated-behaviour) |
| C17 | soft start | 47 nF | 11.3 ms to regulation |
| C18 to C20, C24 | output capacitors | 3 x 22 µF, 10 µF | 38 µF nominal after DC bias, 25 µF minimum |
| C23 | load switch rise, on CT | 10 nF | 17 ms |
| R39, C36 | snubber, SW at L2 pin 2 to GND | not fitted; start with 2.2 Ω, 1 nF | |

- MODE is tied to GND (forced PWM, fixed frequency).
- PWR_EN enables U6 and U7 together. The load switch rises slower than the boost soft start, so rail inrush does not reach the boost current limit.
- BOOST_5V stays charged behind the open load switch after shutdown.
- SW1 (HRO K3-1280S-F1, LCSC C92658) carries control current only. Terminal 2, the middle pad, is the common.
- Fit the snubber only if the bench shows switch-edge ringing in the receiver bands. The EMI estimate (`hardware/sim/emi/results.md`) puts the ring at about 80 MHz with the board's loop inductance and the edge tail of both converters within a few dB of the receiver noise floors; the real amplitude depends on the IC's edges and switch capacitance.
- L1 and L2 are 13 mm apart, centre to centre. Simulated magnetic pickup is at most 0.08 mV into the THM network and 0.03 mV between the inductors.

### Simulated behaviour

Worst case over 3.1 to 4.4 V input and all component corners, from `hardware/sim/power/results.md` (720 kHz, capacitors derated for DC bias). The file has the full tables.

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
- The transient and undervoltage models include both LM73100 feeds, regulated forward drop, on-resistance and reverse blocking. At maximum diode drop the load-step minimum is 4.792 V, leaving 42 mV above the assumed 4.75 V limit. The typical corner leaves 53 mV.
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

### Copper path resistances

At 20 °C, from the heat model (`hardware/sim/heat/results.md`).

| Net | Path | Resistance |
|---|---|---|
| USB_VBUS | J1 to U4 CHGIN | 8.1 mΩ |
| VSYS | L1 to U4 SYS | 8.2 mΩ |
| VSYS | U4 SYS to L2 | 8.7 mΩ |
| VBAT | F1 to U4 BATT | 4.9 mΩ |
| BAT_IN | J3 to F1 | 1.3 mΩ |
| RAIL_5V | U7 to JP2 | 13.0 mΩ |
| ESP_5V | JP1 to J7 | 5.5 mΩ |
| ADS_5V | JP2 to J10 | 1.5 mΩ |

### Losses

Closed case, 25 °C ambient, nominal corner (`hardware/sim/heat/results.md`):

| Operating point | Board loss | Cell to 5 V |
|---|---|---|
| Battery 3.1 V, typical 1.22 W load | 166 mW | 89.9 % |
| Battery 3.1 V, 0.8 A load | 458 mW | 89.4 % |
| Battery 3.1 V, 1.0 A load | 626 mW | 88.2 % |
| USB 3 A source, cell 3.7 V, charging, 0.8 A load | 2.41 W | |

- At 0.8 A on battery the largest items are U6 (201 mW), L2 (54 mW), the charger's BATT-to-SYS switch (43 mW), the LEDs (32 mW) and F1 (21 mW).
- While charging the charger buck dissipates 1.58 W. L1, the USB copper and the USB contacts add about 70 to 100 mW each.

### Temperatures

Heat model at 25 °C ambient unless stated (`hardware/sim/heat/results.md`). The closed case includes a 5 cm² lower inlet and a 5 cm² upper outlet, 50 % open grille and 35 mm vertical separation. The enclosure is not designed yet. U4 is covered under [Heatsink](#heatsink).

| Item | Open bench | Closed case | Closed case, 50 °C ambient |
|---|---|---|---|
| U6 junction at 1 A load | 36 °C | 69 to 70 °C | 94 to 96 °C |
| Hottest capacitor | 57 °C (C6) | 108 °C (C6, charging) | 93 °C (C15, 1 A load) |
| Pack at 0.8 A load | 26 °C | 50 to 51 °C | 76 to 77 °C |
| Pack at 1.0 A load | 27 °C | 57 to 58 °C | 82 to 84 °C |

- L2 and U7 stay below 100 °C, the X7R capacitors below their 125 °C rating.
- With the selected vents, the pack stays below the thermostat's lowest opening point (55 °C) at 0.8 A and 25 °C ambient, but exceeds it at 1.0 A. These estimates do not simulate thermostat or JEITA shutdown; see [open-issues.md](open-issues.md).

## Gauge

U8 MAX17048 (LCSC C2682616) is supplied from VBAT, behind F1, and answers at I2C address 0x36. The bus and its pull-ups are described in [interfaces.md](interfaces.md).

## Indication

Two 3 mm through-hole LEDs next to the USB port, fitted by the user. What the user sees is in [specs.md](specs.md).

| LED | Part | Supply | On when |
|---|---|---|---|
| D5 green | Kingbright L-7104GD | RAIL_5V through R26 470 Ω | the 5 V rail is on |
| D4 red, pin 1 | Kingbright L-115WEGW | VSYS through Q4 and R23 560 Ω | INOKB is low (pull-up R36 100 kΩ to VSYS) |
| D4 green, pin 3 | Kingbright L-115WEGW | VSYS through Q3 and R31 560 Ω | STAT is low (pull-up R33 47 kΩ to VSYS) |

- D4 is common cathode on the middle leg (pin 2 to GND). Q3 and Q4 are AO3401A.
- With no USB source both charger outputs are high impedance and both FETs are off, so D4 draws nothing from the pack.
- LED current is 2.7 to 4.3 mA.
- STAT and INOKB are not wired to the display. The ESP reads the charging state from the MAX17048 charge-rate register.

## Parts sourcing

Passives use JLCPCB Basic stock where a Basic value exists. Expected outside Basic stock:

- the ICs U4 to U13
- the X7R bulk capacitors
- R8 154 kΩ (C22881)
- L1, L2, F1, D3, D6 to D9
- J1, SW1, SW2, SW3, BZ1

The JLCPCB class of each part is not recorded in the project and is not verified here. Check it at the BOM export. Hand-fitted parts are in [bom-hand-sourced.md](bom-hand-sourced.md).
