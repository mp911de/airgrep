# Open issues

Ordered by what blocks the next step.

## Before schematic release

0. 29 Sep: charger changed from the BQ25606 to the MAX77757JEFG420 (C7454852, Extended, about $8, 100 in JLCPCB stock). Reason: the BQ25606 applies its ILIM resistor only to sources it cannot identify and draws 2.4 A from any source with a DCP signature regardless of the Type-C advertisement; the MAX77757 reads CC and D+/D- itself and applies the lower of the two without firmware (docs/power.md). Schematic converted, ERC 0 errors, netlist checked pin by pin. Board: U4 footprint swapped in place (FC2QFN-24 3 × 3 mm, 0.4 mm pitch, land pattern taken from the LCSC/JLCPCB library entry, no exposed pad), U1, U2, U3, Q1, Q5, R1 to R5, R7, R32, R35, C2 to C4, C32 removed, C33, C34, R36, Q6 dropped onto freed spots, the nine U4 thermal vias and every track that ended on a pad whose net changed removed, zones refilled. The charger area needs re-placing and re-routing per datasheet Figure 13 (CHGIN, BYP, SYS, VDD, BATT capacitors at the pins, PGND returned through the SYS/BYP capacitor ground): 35 unconnected items in DRC, no other errors. Still open from before: boost values (R16, R17, R18, C21, C22, C23) are revision C initial values, not validated. Slider common = terminal 2, verified.
0a. Done 29 Sep: MAX77757 JEITA window fitted to the pack's 0 to 45 °C by changing the NTC to the TDK B57164K0104 (100 kΩ, B25/100 4600 K) with R8 130 kΩ: 0.0 / 7.7 / 34.7 / 45.2 °C from the datasheet R/T table (docs/power.md). The Variohm 10 kΩ / 3435 K type cannot fit both ends with any bias value. The BOM entry in docs/bom-hand-sourced.md is updated; do not order the Variohm.
0b. MAX77757 input: 13.7 V over-voltage lockout, 16 V absolute maximum (BQ25606: 22 V). The SMAJ12A (breakdown 13.3 V min, clamp 19.9 V at 20.8 A) is kept; a USB-C source never delivers more than 5 V without a PD negotiation the device does not do. Consider SMAJ10A if 12 V adapters need not be tolerated.
0c. Charge current 2.0 A from R6 14 kΩ (Extended, C22803; decided 29 Sep, one more feeder fee). Pack lead rated 2 A per contact.
0d. Charger indication changed (docs/power.md): red = USB input valid, green blinking = charging, green solid = charged. The 1 Hz blink is the MAX77757 STAT encoding and cannot be filtered into a steady colour without extra parts. specs.md updated.

1. Battery pack verified (Soldered 333290, FE1160100). Carrier connector JST-PH 2 mm (decided), 2 A per contact; pack fuse 3 A.
2. Done 25 Sep: TS set for the pack's 0 to 45 °C charge rating (R8 10 kΩ, R9 removed, Variohm 10k/3435 NTC): hot stop 43.3 °C nominal, 44.6 °C worst case. Superseded 29 Sep by the MAX77757 THM thresholds (0a).
3. TPS61088 compensation, inductor and current limit for 5 V / 2.5 A. Revision C values are unvalidated.
4. Done 25 Sep: thermostat Limitor LIM-P-82-60 (60 °C NC, auto reset) from Reichelt and the NTC (since 29 Sep the TDK B57164K0104, see 0a), taped to the pack, user-made JST-PH harnesses to J4 and J2.
5f. Done 25 Sep: D4 is the Kingbright L-115WEGW (red charging, green charged, common cathode), D5 the Kingbright L-7104GD green, both from Reichelt, user-soldered.
5g. Connection review 24 Sep (all parts checked against datasheets, board nets = schematic nets). Fixed: J6 carried the UART on D14/D15 (removed, J6 is GND only); TPS61088 FSW resistor R16 went to GND instead of SW; TPS61088 NC pins 11/12 now GND per TI; SMAJ12A drawn as bidirectional with the cathode on GND (would have shorted VBUS), now cathode on VBUS; STAT LED resistor 470 Ω exceeded the 6 mA STAT sink rating, now 1 kΩ; full-indicator transistor Q3 changed to 2N7002 so the charging colour cannot glow when full.
5k. Silkscreen tidied 25 Sep: every visible reference sits next to its own part (closer to it than to any neighbour), clear of pads, other silkscreen and the board edge; tight spots use 0.8 mm text (JLCPCB minimum). Six references have no room and are on the fab layer only (assembly drawing and pick-and-place): U7, C11, C12, Q3, R13, R18. SW1, SW2 and J1 outline segments that ran over the board edge were moved to the fab layer.
5j. Done 25 Sep: D4 reworked to common cathode for the Kingbright L-115WEGW (Reichelt KBT L-115WEGW; its product page wrongly says common anode). Q4 is now a 2N7002, R34 removed, Q5 MMBT3904 and R35 10 kΩ added.
5i. 25 Sep: barometer BMP581 (C5362283, Extended, about $2.8) now fitted, moved next to the display I2C header J8 with C29/C30; track/via keepout under the sensor per Bosch. I2C pull-ups R19/R20 carry the real DNP flag (the display already pulls the bus up); fit them only if the bus is run without the display.
5h. Done 25 Sep: U1 changed from XC6206 (6 V operating, 7 V absolute) to TI TPS70933DBVR (LCSC C89347, 30 V input, 1.4 µA, SOT-23-5). EN left open (internal 300 nA pull-up; EN is limited to 6.5 V, never tie it to VIN). C2 raised to 4.7 µF 25 V 0805 (TPS709 needs 1.5 µF effective), C32 1 µF 50 V added at U1's input. 29 Sep: U1 removed again with the TUSB320; on VBUS remain the MAX77757 (16 V absolute maximum, open issue 0b), D3, C1 and C5. Remaining exposure: MAX77757 CC1/CC2 and DP/DN are rated 6 V and face the connector through the H5VLD3B ESD diodes only.
5. Done 25 Sep: USB-C receptacle is Hanbo MC-107DMS, 5 A, JLCPCB C3039295 (Extended, about 188k in stock).

## Before layout release

5a. Done 25 Sep: USB-C changed from GCT USB4105-GF-A-120 (EasyEDA footprint, pads 0.14 mm from the peg holes, low stock) to Hanbo MC-107DMS (C3039295) on a footprint copied from KiCad's HRO TYPE-C-31-M-12 with the front shell slots lengthened to 1.4 mm per the Hanbo drawing. Pads now 0.40 mm from the peg holes. ESD diodes D6 to D9 moved into two rows above the connector. Needs routing.
5b. Done 25 Sep: thermal vias in the exposed pads of U6 (4 × 3) and U7 (2), 0.7112/0.3048 mm since 29 Sep, into the bottom GND plane; the nine U4 vias went on 29 Sep with the BQ25606 (the MAX77757 FC2QFN has no exposed pad, its PGND pins return through the SYS/BYP capacitor ground); exposed pads and U6 GND pins use solid pour connection. Vias are not plugged (JLCPCB via plugging optional).
5e. Routed 25 Sep. Boost re-placed per TI layout: U6 rotated 180° (SW pins face L2, VOUT faces U7), L2 moved up 1.5 mm, C16 (boot) above BOOT/SW pins, C18 to C20 in a column left of the VOUT-to-U7 path, R14 beside C15, FB/COMP/ILIM exits hand-drawn. R7/R8 moved below the VBAT path. Hand-drawn power paths (widths): BAT_IN 1.2, VBAT 1.0 to 1.2 (0.8 riser), VSYS 1.2, CHG_SW 1.0, BOOST_SW 0.8 to 1.0, BOOST_5V 1.0, USB_VBUS 1.0 (J1 to charger on the bottom layer, two 0.8/0.4 vias each end). Remaining narrow runs: RAIL_5V to the module shunts mostly 0.4 mm over about 70 mm (about 45 mV drop at 0.5 A, 130 mV at 1.5 A), ADS_5V/ESP_5V 0.4 to 0.8 mm. Track keepouts: 0.6 mm board-edge band, mounting holes, BMP581 centre (no tracks, vias or pour under the sensor).
5d. D3 (SMAJ12A VBUS TVS) sits at the charger input (61, 64.8), about 30 mm from the USB-C; the four H5VLD3B ESD diodes sit at the connector. Acceptable for a 5 V input; move D3 closer if the bottom-right strip is re-arranged.
5c. Done 24 Sep: U6 footprint checked against TI RHL0020A land pattern (pin order, pitch, rotation match; exposed pad simplified to a 3.0 × 2.1 mm rectangle without the PGND tabs, same net).

6. ADSBee real header coordinates (image estimate ±0.5 mm now). Needs a 1:1 template or the board.
7. Display socket height versus rear components.
8. MAX17048 TDFN land pattern per Analog Devices 90-0065 and JLCPCB clearance rule.
9. Antenna bulkhead models, hole sizes, coax routing. GNSS active antenna bias current must fit the ADSBee 200 mA 3.3 V output.
10. Copper for power paths: done 25 Sep (see 5e). Converter loops are short (boost SW node 6.8 mm, boost output to U7 9.4 mm, charger SW node 6.4 mm, no vias in these paths).

## Before bring-up

11. FLARM 868 MHz decoding on ADSBee firmware is unproven.
12. Display firmware: gauge readout, Wi-Fi traffic feed, buzzer PWM on D11.
13. Bench tests: supplement mode with 0.5 / 1 / 2 / 3 A sources, switch off with USB present, thermostat trip, no backfeed, input current with a 1.5 A Type-C source that also shorts D+/D- (must stay at 1.5 A), dead-battery attach to a Type-C source (CC clamp, then Rd within 15 ms), LED pattern per docs/power.md.
