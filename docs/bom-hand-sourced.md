# BOM: hand-sourced parts

Everything that JLCPCB does not assemble, per board, with order sources. Status 25 Sep 2026. Prices are indicative (incl. VAT for Reichelt). The SMD assembly itself (all resistors, capacitors, ICs, USB-C, slide switches, buzzer, barometer) is ordered from JLCPCB with the KiCad BOM/CPL export; it is listed once at the end.

> **D4:** the Kingbright L-115WEGW is common cathode (pin 1 red anode, pin 2 common cathode, pin 3 green anode; Kingbright datasheet DSAB9091), although Reichelt's product page says "common anode". The circuit was reworked for common cathode on 25 Sep; the red-anode leg goes into the square pad (pin 1), the middle leg is the cathode.

## Modules

| Qty | Part | Description | Where | Approx. price |
|---|---|---|---|---|
| 1 | Seeed Studio XIAO 1.47" IPS Touch Display (ESP32-S3), SKU 100069905 | Display and main MCU; sits on J6, J7, J8, J9 | [Seeed Studio](https://www.seeedstudio.com/1-47-Inch-Touch-Display-Powered-by-XIAO-ESP32-S3-Plus-p-6996.html), [OpenELAB (EU)](https://openelab.io/products/seeed-studio-xiao-1-47-ips-touch-display-esp32-s3) | $19.90 |
| 1 | Pants for Birds ADSBee 1090U | ADS-B 1090 MHz and 978 MHz receiver with Wi-Fi; sits on J10, J11 | [pantsforbirds.com](https://pantsforbirds.com/product/adsbee-1090u/) | see shop |
| 1 | SparkFun GNSS Receiver Breakout MAX-M10S, GPS-18037 | GNSS receiver with SMA connector; sits on J5 | [SparkFun](https://www.sparkfun.com/sparkfun-gnss-receiver-breakout-max-m10s-qwiic.html), [Botland (EU)](https://botland.store/gps-modules/22065-gnss-max-m10s-module-qwiic-sparkfun-gps-18037.html) | $45.95 |

## Battery and battery sensors

| Qty | Part | Description | Where | Approx. price |
|---|---|---|---|---|
| 1 | Soldered 333288 (5000 mAh) **or** Soldered 333290 (10000 mAh) | Li-ion polymer pack, 3.7 V, JST-PH 2-pin plug fitted, built-in protection | Reichelt [AKKU SOLD333288](https://www.reichelt.com/de/en/shop/product/lithium-ion_battery_soldered_333288_5000_mah_3_7_v-373552) / AKKU SOLD333290 | €14.95 (5000 mAh) |
| 1 | TDK (EPCOS) B57164K0104K000 or J000 | NTC 100 kΩ, B25/100 4600 K (R/T curve 2005), lacquered disk, 5 mm lead spacing; taped to the pack, plugs into J2. Changed 29 Sep from the Variohm 10 kΩ / 3435 K: with the MAX77757 only this B value fits the 0 to 45 °C window (R8 130 kΩ). Reichelt lists the EPCOS B57164K series as "NTC-0,2"; else Bürklin 80E6452 or RS 1912314 | Bürklin [80E6452](https://buerklin.com/en/p/epcos/ntc-thermistors/b57164k0104j000/80E6452) or RS [1912314](https://uk.rs-online.com/web/p/thermistor-ics/1912314P) | about €0.50 |
| 1 | Limitor LIM-P-82-60-5-100-10A | Thermostat 60 °C, normally closed, auto reset, flat 14.5 × 8 × 3.6 mm, wire leads; taped to the pack, plugs into J4 | Reichelt [LIM-P-82-60](https://www.reichelt.com/de/en/shop/product/temperature_limiter_60_c_nc_250_v_-351657) | €3.60 |

## Through-hole parts on the carrier (hand-soldered)

| Qty | Ref | Part | Description | Where | Approx. price |
|---|---|---|---|---|---|
| 3 | J2, J3, J4 | JST B2B-PH-K-S | JST-PH 2-pin header, vertical, 2 mm | Reichelt [JST PH2P ST](https://www.reichelt.com/de/en/shop/product/jst_pin_header_straight_1x2-pin_ph-185049) | €0.16 each |
| 1 | D5 | Kingbright L-7104GD | LED 3 mm green, diffused, 20 mcd, 40° (power) | Reichelt [KBT L-7104GD](https://www.reichelt.com/de/en/led-3-mm-leaded-green-20-mcd-40--kbt-l-7104gd-p230824.html) | €0.10 |
| 1 | D4 | Kingbright L-115WEGW | LED 3 mm red/green, 3-pin, **common cathode**, diffused, 60° (red USB power, green charging or charged) | Reichelt [KBT L-115WEGW](https://www.reichelt.com/de/en/shop/product/led_3_mm_tht_3-pin_red_green_627_565_nm_40_40_mcd_60_-231039) | €0.28 |
| 3 | J5 to J11 | Female header 2.54 mm, 1 × 20, straight, 8.5 mm high, cut to length | Module sockets: 8 + 8 + 7 + 4 + 2 + 13 + 5 = 47 positions; cutting costs one position per cut, 3 strips are enough | Reichelt [BL 1X20G8 2,54](https://www.reichelt.com/de/en/20-pin-socket-terminal-strip-straight-rm-2-54-h-8-5-mm-bl-1x20g8-2-54-p51827.html) | about €1 each |
| 1 | JP1, JP2 | Pin header 2.54 mm, 1 × 40, straight, cut to 2 × 2 pins | Power shunts ESP 5V / ADS 5V; the rest of the strip serves as module pins (below) | Reichelt [SL 1X40G 2,54](https://reichelt.com/de/en/40-pin-header-straight-pitch-2-54-sl-1x40g-2-54-p19506.html) | about €0.30 |
| 2 | JP1, JP2 | Jumper 2.54 mm, closed, with grip | Shunts; remove for USB service of a module | Reichelt [Jumper 2.54 mm closed with handle](https://www.reichelt.com/de/en/shop/product/jumper_2_54_mm_closed_with_handle_black-119938) | €0.10 each |

The previously planned Samtec SSQ sockets (8.51 mm) are the premium alternative to the BL 1X20G8 strips (Mouser, Digi-Key); the Reichelt strips have the same height and pitch.

## Module pins

| Qty | Part | Description | Where |
|---|---|---|---|
| 1 | Pin header 2.54 mm, 1 × 40, straight (second strip if needed) | Male pins for the display (8 + 7 + 4 + 2), the GNSS breakout (8) and the ADSBee (13 + 5), only where the module ships without pins fitted | Reichelt [SL 1X40G 2,54](https://reichelt.com/de/en/40-pin-header-straight-pitch-2-54-sl-1x40g-2-54-p19506.html) |

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
| 1 | Active GNSS antenna, SMA plug, e.g. Navilock 60555 | Multi-GNSS active antenna (3 m cable) | Reichelt [NAVILOCK 60555](https://www.reichelt.com/de/en/gnss-antenna-sma-connector-3-m-outdoor-use-navilock-60555-p347955.html) |
| 1 | Delock SMA bulkhead jack to SMA plug 90° cable | GNSS breakout SMA to case bulkhead | Reichelt [Delock antenna cable SMA jack bulkhead / SMA plug 90°](https://www.reichelt.de/de/de/shop/produkt/antennenkabel_sma_buchse_zum_einbau_sma_stecker_90_-364462) |
| 1 | W.FL / MHF3 to RP-SMA female bulkhead pigtail, 0.81 mm coax, 100 to 150 mm | ADSBee Wi-Fi output (W.FL, not U.FL) to case bulkhead; not stocked by Reichelt | [Amazon example](https://www.amazon.com/RP-SMA-female-bulkhead-IPEX-MHF3-connector/dp/B00K282VW0), [Data Alliance](https://www.data-alliance.net/w-fl-to-sma-female-cable-4-inch-6-inch-8-inch-10-inch-1ft-14-in-16-in-18-in/) |
| 1 | 2.4 GHz antenna, RP-SMA | Wi-Fi antenna for the ADSBee | any 2.4 GHz RP-SMA rod antenna |

## Mechanical

| Qty | Part | Description |
|---|---|---|
| 5 | M3 standoff + screws | Case mounting at the five edge holes (height per enclosure design) |
| 12 | M3 or M2.5 screws, nuts, short standoffs | Module mounting: display 4, ADSBee 4, GNSS 4. Module holes are 2.5 mm (display, ADSBee) and 3.3 mm (GNSS); carrier holes are 3.2 mm |
| 1 | Insulating sheet (e.g. 0.5 mm polycarbonate or FR-4 blank) | Flat barrier between the carrier's solder side and the battery |

## Assembled carrier board

| Qty | Part | Where |
|---|---|---|
| 5 (JLCPCB minimum) | Carrier PCB, 105 × 95 mm, 2 layers, 1.6 mm, SMD parts assembled (top side, standard assembly for the USB-C shell tabs) | [JLCPCB](https://jlcpcb.com/), upload Gerbers, BOM and CPL exported from `hardware/kicad/` (export not done yet) |
