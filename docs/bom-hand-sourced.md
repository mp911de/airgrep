# BOM: hand-sourced parts

Everything that JLCPCB does not assemble, per board, with order sources and order status. Prices are indicative and include VAT for Reichelt. JLCPCB assembles the SMD parts (resistors, capacitors, ICs, USB-C, slide switches, buzzer, barometer) from the KiCad BOM and CPL export; the board is listed once at the end.

> **D4 polarity:** the Kingbright L-115WEGW is common cathode (pin 1 red anode, pin 2 cathode, pin 3 green anode; Kingbright data sheet DSAB9091). Reichelt's product page says "common anode", which is wrong. The red-anode leg goes into the square pad (pin 1), the middle leg is the cathode.

## Modules

| Qty | Part | Description | Status | Where | Approx. price |
|---|---|---|---|---|---|
| 1 | Seeed Studio XIAO 1.47" IPS Touch Display (ESP32-S3), SKU 100069905 | Display and main MCU; sits on J4, J1, J9, J8 | ordered | [Seeed Studio](https://www.seeedstudio.com/1-47-Inch-Touch-Display-Powered-by-XIAO-ESP32-S3-Plus-p-6996.html), [OpenELAB (EU)](https://openelab.io/products/seeed-studio-xiao-1-47-ips-touch-display-esp32-s3) | $19.90 |
| 1 | Pants for Birds ADSBee 1090U | ADS-B 1090 MHz and 978 MHz receiver with Wi-Fi; sits on J6, J5 | arrived | [pantsforbirds.com](https://pantsforbirds.com/product/adsbee-1090u/) | see shop |

## Battery and battery sensors

| Qty | Part | Description | Where | Approx. price |
|---|---|---|---|---|
| 1 | Soldered 333288 (5000 mAh) **or** Soldered 333290 (10000 mAh) | Li-ion polymer pack, 3.7 V, JST-PH 2-pin plug fitted, built-in protection; data in [specs.md](specs.md) | Reichelt [AKKU SOLD333288](https://www.reichelt.com/de/en/shop/product/lithium-ion_battery_soldered_333288_5000_mah_3_7_v-373552) / AKKU SOLD333290 | €14.95 (5000 mAh) |
| 1 | Variohm ENTC-EB-100K4262-02 | NTC 100 kΩ, B25/85 4262 K ±0.5 %, ±0.2 °C from 0 to 70 °C, epoxy bead with 76 mm bare 32 AWG leads; taped to the pack, plugs into J12. Charge temperature window: [power.md](power.md) | Reichelt [350469](https://www.reichelt.de/de/de/shop/produkt/thermistor_ntc_-40_bis_150_c-350469) | €1.80 |
| 1 | Limitor LIM-P-82-60-5-100-10A | Thermostat 60 °C, normally closed, auto reset, flat 14.5 x 8 x 3.6 mm, wire leads; taped to the pack, plugs into J13 | Reichelt [LIM-P-82-60](https://www.reichelt.com/de/en/shop/product/temperature_limiter_60_c_nc_250_v_-351657) | €3.60 |

## Through-hole parts on the carrier (hand-soldered)

D4, D5, J1 to J6, J8 to J13, JP1, JP3 and SW4 are marked DNP, so the assembly BOM and position files leave them out. Their footprints are on the board; fit the parts below by hand. R90/R91 are separate DNP pull-up pads for bench tuning. The mounting holes (MH1 to MH4, MH6 to MH8, MH12 to MH14) are board-only footprints and are excluded from the assembly files as well.

| Qty | Ref | Part | Description | Where | Approx. price |
|---|---|---|---|---|---|
| 1 | SW4 | 6 x 6 mm tactile, normally open | Reset button below BZ1. Standard 6.5 x 4.5 mm pin spacing, 1 mm holes; verify the available button fits before fabrication. | Existing stock, hand-soldered | already owned |
| 3 | J12, J11, J13 | JST B2B-PH-K-S | JST-PH 2-pin header, vertical, 2 mm | Reichelt [JST PH2P ST](https://www.reichelt.com/de/en/shop/product/jst_pin_header_straight_1x2-pin_ph-185049) | €0.16 each |
| 1 | D5 | Kingbright L-7104GD | LED 3 mm green, diffused, 20 mcd, 40° (power) | Reichelt [KBT L-7104GD](https://www.reichelt.com/de/en/led-3-mm-leaded-green-20-mcd-40--kbt-l-7104gd-p230824.html) | €0.10 |
| 1 | D4 | Kingbright L-115WEGW | LED 3 mm red/green, 3-pin, **common cathode**, diffused, 60° (red USB present, green charging, blinking green fault) | Reichelt [KBT L-115WEGW](https://www.reichelt.com/de/en/shop/product/led_3_mm_tht_3-pin_red_green_627_565_nm_40_40_mcd_60_-231039) | €0.28 |
| 2 | J1, J4, J5, J6, J8, J9 | Female header 2.54 mm, 1x20, straight, 8.5 mm high, cut to length | Module sockets: 8 + 7 + 4 + 2 + 13 + 5 = 39 positions; cutting costs one position per cut, 2 strips are enough | Reichelt [BL 1X20G8 2,54](https://www.reichelt.com/de/en/20-pin-socket-terminal-strip-straight-rm-2-54-h-8-5-mm-bl-1x20g8-2-54-p51827.html) | about €1 each |
| 1 | JP1, JP3 | Pin header 2.54 mm, 1x40, straight, two 1x2 pieces cut off | Power shunts ESP 5V / ADS 5V; the rest of the strip serves as module pins (below) | Reichelt [SL 1X40G 2,54](https://reichelt.com/de/en/40-pin-header-straight-pitch-2-54-sl-1x40g-2-54-p19506.html) | about €0.30 |
| 2 pieces | J10 | Pin header 2.54 mm, 1x4, straight, cut from the SL 1X40G strip | Fit side by side in the 2x4 footprint; three connected dry-contact inputs, two grounds and three unused pins. Eight additional pins from the existing strips. | Reichelt SL 1X40G 2,54 (link above) | included in strip |
| 2 | JP1, JP3 | Jumper 2.54 mm, closed, with grip | Shunts | Reichelt [Jumper 2.54 mm closed with handle](https://www.reichelt.com/de/en/shop/product/jumper_2_54_mm_closed_with_handle_black-119938) | €0.10 each |
| 1 | U4 | Fischer ICK S 10 x 10 x 6.5 (or ICK BGA 10 x 10 x 6) heatsink, a 2.5 x 2.5 x 1.5 mm copper spacer and thermal epoxy (two layers, about 0.1 mm each) | Heatsink on BQ25798 U4; the spacer lifts it above neighbouring capacitors. See [power.md](power.md#heatsink) | RS 674-4784 / Bürklin (ICK S), RS 057-7631, DigiKey, Farnell (ICK BGA); spacer cut from 1.5 mm copper sheet | about €2 |

Samtec SSQ sockets (8.51 mm, Mouser or Digi-Key) are an alternative to the BL 1X20G8 strips, with the same height and pitch.

## Module pins

| Qty | Part | Description | Where |
|---|---|---|---|
| 1 | Pin header 2.54 mm, 1x40, straight (second strip if needed) | Male pins for the display (8 + 7 + 4 + 2), the ADSBee (13 + 5), only where the module ships without pins fitted | Reichelt [SL 1X40G 2,54](https://reichelt.com/de/en/40-pin-header-straight-pitch-2-54-sl-1x40g-2-54-p19506.html) |

## Sensor harnesses (user-made)

| Qty | Part | Description | Where | Approx. price |
|---|---|---|---|---|
| 2 | JST PHR-2 | JST-PH socket housing, 2-pin | Reichelt [JST PH2P BU](https://www.reichelt.com/de/en/shop/product/jst_socket_housing_1x2-pin_ph-185041) | €0.06 each |
| 4 (+ spares) | JST SPH-002T-P0.5S | JST-PH crimp contact, 24 to 30 AWG | Reichelt [JST PH CKS](https://www.reichelt.com/de/en/shop/product/jst_crimp_contact_socket_ph-185072) | €0.03 each |
| 1 m | Stranded wire 26 AWG, two colours | Harness wire | any (Reichelt LiFY 0.14 mm²) | |
| 1 | Heat-shrink tube 1.5 mm and 3 mm | Insulation of the NTC's bare Dumet leads and solder joints | any | |
| 1 | Kapton tape, thermal pad 0.5 mm | Fix both sensors to the middle of the pack face | any | |

A JST-PH crimp tool (e.g. Engineer PA-09 or a generic 2.0 mm open-barrel crimper) is needed for the contacts.

## Antennas and RF cabling

| Qty | Part | Description | Where |
|---|---|---|---|
| 1 | 1090 MHz antenna, SMA | ADS-B antenna; the ADSBee enclosure and sleeve-dipole bundle includes 1090 / 978 MHz antennas | [Pants for Birds bundle](https://pantsforbirds.com/product/adsbee-1090u-enclosure-and-1090mhz-978mhz-sleeve-dipole-antenna-bundle/) |
| 1 (2 with 978 MHz) | Delock 88343, SMA bulkhead jack to U.FL plug, 200 mm | ADSBee U.FL (1090 MHz, sub-GHz) to case bulkhead | Reichelt [DELOCK 88343](https://www.reichelt.com/de/en/sma-jack-mount-mhf-u-fl-plug-200-mm-delock-88343-p179726.html) |
| 1 | Abracon AANE-AP-0164, active GNSS antenna, SMA plug | Preferred. L1 multi-GNSS, LNA 28 dB, 15 to 25 mA at 3.0 to 5.0 V, 1 m RG-174 cable (`datasheets/abracon-aane-ap-0164.pdf`) | Digi-Key [AANE-AP-0164-1](https://www.digikey.de/de/products/detail/abracon-llc/AANE-AP-0164-1/26813090) |
| (1) | Navilock 60555, active GNSS antenna, SMA plug | Fallback. Multi-GNSS active antenna, 3 m cable; bias current not checked; the carrier feeds 3.3 V from GNSS_VCC_RF through R82 | Reichelt [NAVILOCK 60555](https://www.reichelt.com/de/en/gnss-antenna-sma-connector-3-m-outdoor-use-navilock-60555-p347955.html) |
| 1 | Delock 88343, SMA bulkhead jack to U.FL plug, 200 mm | Carrier J7 (GNSS) to case bulkhead | Reichelt [DELOCK 88343](https://www.reichelt.com/de/en/sma-jack-mount-mhf-u-fl-plug-200-mm-delock-88343-p179726.html) |
| 1 | W.FL / MHF3 to RP-SMA female bulkhead pigtail, 0.81 mm coax, 100 to 150 mm | ADSBee Wi-Fi output (W.FL, not U.FL) to case bulkhead; not stocked by Reichelt | [Amazon example](https://www.amazon.com/RP-SMA-female-bulkhead-IPEX-MHF3-connector/dp/B00K282VW0), [Data Alliance](https://www.data-alliance.net/w-fl-to-sma-female-cable-4-inch-6-inch-8-inch-10-inch-1ft-14-in-16-in-18-in/) |
| 1 | 2.4 GHz antenna, RP-SMA | Wi-Fi antenna for the ADSBee | any 2.4 GHz RP-SMA rod antenna |

## Mechanical

Hole positions and the insulator board are described in [mechanical.md](mechanical.md).

| Qty | Part | Description |
|---|---|---|
| 4 | M2.5 standoffs and screws | Case mounting at the corner holes MH1 to MH4. The height depends on the enclosure. |
| 5 | M2.5 screws, nuts and standoffs, 8.5 mm nominal | Module supports: two for the GNSS module, two for the ADSBee, one for the display. Check the assembled height and the fit in the module holes before buying in quantity. |
| 1 | Insulator board, 0.8 mm FR-4, no copper | Sits on the case standoffs between the carrier's solder side and the battery. Order from JLCPCB with the carrier; project in `hardware/insulator/`. |

## Assembled carrier board

| Qty | Part | Where |
|---|---|---|
| 5 (JLCPCB minimum) | Carrier PCB, 105 x 95 mm, 4 layers, 1.6 mm, stackup JLC04161H-2116C (select it in the order; 1 oz outer copper), SMD parts assembled on the top side (standard assembly, because of the USB-C shell tabs) | [JLCPCB](https://jlcpcb.com/). Upload Gerbers, BOM and CPL from `hardware/carrier-max-charger/jlcpcb/production_files/`. Not ordered; the files need a fresh export first, see [open-issues.md](open-issues.md). |
