# Data sheets

Data sheets for the parts on the carrier and the parts fitted to it by hand. Coverage is not complete; the gaps are listed below. [manifest.json](manifest.json) records source URLs and, where taken, page counts and SHA-256 hashes. Most PDFs have a searchable text extraction in [pdf2text/](pdf2text/) under the PDF's name.

| PDF | Part | Source | Host |
|---|---|---|---|
| [max77757.pdf](max77757.pdf) | U4 charger | [Original](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX77757.pdf) | RS Components mirror |
| [tps61088.pdf](tps61088.pdf) | U6 boost | [Original](https://www.ti.com/lit/ds/symlink/tps61088.pdf) | Manufacturer |
| [tps22975.pdf](tps22975.pdf) | U7 load switch | [Original](https://www.ti.com/lit/ds/symlink/tps22975.pdf) | Manufacturer |
| [tlv840.pdf](tlv840.pdf) | U5 supervisor | [Original](https://www.ti.com/lit/ds/symlink/tlv840.pdf) | Manufacturer |
| [max17048.pdf](max17048.pdf) | U8 fuel gauge | [Original](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX17048-MAX17049.pdf) | Adafruit mirror |
| [sn74lvc1g17.pdf](sn74lvc1g17.pdf) | U9 buffer | [Original](https://www.ti.com/lit/ds/symlink/sn74lvc1g17.pdf) | Manufacturer |
| [bst-bmp581-ds004.pdf](bst-bmp581-ds004.pdf) | U10 barometer | [Original](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp581-ds004.pdf) | Manufacturer |
| [ao3401a.pdf](ao3401a.pdf) | Q2, Q3, Q4, Q7 | [Original](https://www.aosmd.com/pdfs/datasheet/AO3401A.pdf) | Manufacturer |
| [2n7002-nexperia.pdf](2n7002-nexperia.pdf) | Q6, Q8, Q9 | [Original](https://assets.nexperia.com/documents/data-sheet/2N7002.pdf) | Manufacturer |
| [cenker-ckst-series.pdf](cenker-ckst-series.pdf) | L2 | [Manufacturer source](https://www.cenkersz.com/file/upload/products/CKSTPDF.pdf) | LCSC mirror |
| [murata-grm21bz71a226.pdf](murata-grm21bz71a226.pdf) | 22 µF X7R 0805 capacitors | [Product page](https://www.murata.com/en-us/products/productdetail?partno=GRM21BZ71A226ME15%23) | LCSC mirror |
| [hanbo-mc107dms.pdf](hanbo-mc107dms.pdf) | J1 USB-C | | |
| [hongjiacheng-h5vld3b.pdf](hongjiacheng-h5vld3b.pdf) | D6 to D9 ESD (CC, D+, D-) | [LCSC C20615805](https://www.lcsc.com/datasheet/C20615805.pdf) | LCSC mirror |
| [littelfuse-466.pdf](littelfuse-466.pdf) | F1 fuse | [Series page](https://www.littelfuse.com/products/fuses-overcurrent-protection/fuses/surface-mount-fuses/thin-film-fuses/466) | Distributor mirror |
| [hro-k3-1280s.pdf](hro-k3-1280s.pdf) | SW1 to SW3, K3-1280S-F1 | HRO drawing, 2013-09-26 | LCSC mirror |
| [tca9534.pdf](tca9534.pdf) | U13, TCA9534PWR | [TI](https://www.ti.com/lit/ds/symlink/tca9534.pdf) | Manufacturer |
| [ck-js-switches.pdf](ck-js-switches.pdf) | superseded switch | C&K JS102011SAQN, retained as historical reference | Distributor mirror |
| [jst-ph-series.pdf](jst-ph-series.pdf) | J2, J3, J4 | [Original](https://www.jst-mfg.com/product/pdf/eng/ePH.pdf) | Manufacturer |
| [samtec-ssq-drawing.pdf](samtec-ssq-drawing.pdf), [footprint](samtec-ssq-footprint.pdf), [catalog](samtec-ssq-catalog.pdf) | Module sockets, alternative part | Samtec SSQ | Manufacturer |
| [samtec-snt-drawing.pdf](samtec-snt-drawing.pdf) | JP1, JP2 shunts, alternative part | Samtec SNT-100 | Manufacturer |
| [variohm-entc-eb.pdf](variohm-entc-eb.pdf) | Pack thermistor | [Series page](https://www.variohm.com/products/temperature-sensors/ntc-thermistor-temperature-sensors/entc-eb-series-) | Distributor mirror |
| [AKKU_SOLD333290_DB-EN.pdf](AKKU_SOLD333290_DB-EN.pdf), [MSDS](AKKU_SOLD333290_SDB-EN.pdf), [UN38.3](AKKU_SOLD_UN38.3-EN.pdf) | Battery 10 Ah | Fullest Energy FE1160100 | reichelt |
| [AKKU_SOLD333288_DB-EN.pdf](AKKU_SOLD333288_DB-EN.pdf) | Battery 5 Ah option | | reichelt |
| [datasheet_adsbee_1090.pdf](datasheet_adsbee_1090.pdf) | ADSBee 1090U module | | |
| [max-m10s.pdf](max-m10s.pdf) | GNSS module (SparkFun GPS-18037) | u-blox | |
| [100069905.pdf](100069905.pdf) | Display module | Seeed XIAO 1.47" ESP32-S3 | |
| [FTD-116-FLARM-FAMP-Public-Protocol-v1.02.pdf](FTD-116-FLARM-FAMP-Public-Protocol-v1.02.pdf) | FLARM protocol | Firmware reference | |

## Notes

- The Variohm series sheet has no R/T table and its ordering list stops at 50 kΩ. The 100K4262 part shares the 4262 K curve.
- The Murata sheet's DC-bias curve is the source of the output-capacitance derating in `hardware/sim/run_power.py`.
- The Hanbo, battery, ADSBee, u-blox, Seeed and FLARM documents have no source URL in the manifest.

## Parts without a data sheet

| Part | Ref |
|---|---|
| FXL0630-1R0-M inductor, 1 µH (LCSC C167216) | L1 |
| SMAJ12A TVS diode (LCSC C19077531) | D3 |
| Kingbright L-115WEGW and L-7104GD LEDs | D4, D5 |
| PKMCS0909E4000-R1 buzzer (LCSC C910763) | BZ1 |
| Limitor LIM-P-82-60 thermostat | on the pack, plugs into J4 |
| JSCJ 2N7002 (LCSC C8545); the Nexperia sheet stands in | Q6, Q8, Q9 |
| Reichelt BL 1X20G8 sockets, SL 1X40G headers and jumpers ([BOM](../docs/bom-hand-sourced.md)); the Samtec SSQ and SNT sheets describe the alternative parts with the same pitch and height | J5 to J12, JP1, JP2 |

## PDFs without a text extraction

`murata-grm21bz71a226.pdf`, `littelfuse-466.pdf`, `ck-js-switches.pdf`, `samtec-ssq-drawing.pdf`, `samtec-ssq-footprint.pdf`, `samtec-ssq-catalog.pdf`, `samtec-snt-drawing.pdf`, `AKKU_SOLD_UN38.3-EN.pdf` and `FTD-116-FLARM-FAMP-Public-Protocol-v1.02.pdf`.

New data sheets go into this directory, with an entry in the manifest and a text extraction in `pdf2text/`.
