# Charger candidates

Stock and datasheets checked 2026-10-08, updated 2026-10-09.

**Decision 2026-10-09: a second board variant, `hardware/carrier-ti-charger/`, uses the BQ25798.** The display's ESP32-S3 is the I2C host; without it the chip charges at its 1 A default. Reasons and the open points are in [decisions.md](decisions.md#charger-variant-carrier-ti-charger-with-the-bq25798). The MAX77757 board stays as `hardware/carrier-max-charger/`. The sections below are the research that led there.

## Chargers with a host (2026-10-09)

Requirements for this round: power-on default charge current without a host, a host can raise it, fallback when the host stops, BC1.2 on chip, no BGA/WLP, JLCPCB stock.

| Part | Package | Default without host | Host | Fallback | TREG | Extras | Verdict |
|---|---|---|---|---|---|---|---|
| **BQ25798** | VQFN-HR 29, 4x4, 0.4 mm pitch | 1 A, 4.2 V, 12 h timer, input limit from D+/D- | I2C 0x6B, up to 1 MHz | watchdog 40 s (20 to 160 s) resets every register | 60/80/100/120 °C | 16-bit ADC: die temperature 0.5 °C, IBAT, IBUS, VBAT, VBUS, TS | selected |
| BQ25611D | WQFN-24 4x4 | 1.02 A, 10 h timer | I2C 0x6B | watchdog 40 s | 90/110 °C | none, no ADC | simpler, cooler; no die temperature readout |
| BQ25895M | QFN-24 4x4 | 2.05 A | I2C | watchdog 40 s | 60/80/100/120 °C | ILIM resistor caps the input | default too fast |
| BQ25601D | QFN-24 4x4 | 2.04 A | I2C | watchdog 40 s | 90/110 °C | | default too fast |
| MAX77840 | 81-bump WLP 0.4 mm | 450 mA, 8 h timer, no NTC control | I2C | registers reset at SYS UVLO | 85 to 130 °C | ModelGauge m5 gauge | WLP: not routable on a JLCPCB 4-layer board (no via fits a 0.4 mm grid), no CC detection, no thermal data |
| MAX77975/76/85/86 | FC2QFN-32 4x4 | 500 mA input limit | I2C | | 5 °C steps to 130 °C | 85/86 add BC1.2 | 75/76 have no D+/D-; 85/86 datasheet not reachable from here |
| MAX77972 | WLP | resistor | I2C | | 85 to 130 °C | CC and BC1.2 | WLP |

No TI charger integrates a fuel gauge; the MAX17048 stays on the board and the BQ25798 ADC adds die temperature and currents.


## MAX77757 without an MCU

The [power design](power.md#earlier-charge-limiting) records the fixed threshold and IFAST uncertainty. AICL reduces demand to hold input near 4.5 V, not to control temperature. Native thermal foldback starts at 130°C and reduces current by 5% of programmed current per additional degree. JEITA halves current when the battery is cool and lowers charge voltage by 150 mV when warm. [Datasheet pp. 15, 23-24](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX77757.pdf)

IFAST settings are 12.4 kΩ / 1.8 A, 11 kΩ / 1.5 A and 8.2 kΩ / 1.2 A. Runtime sampling and resistance interpolation are unspecified. A thermostat switching these resistors is conditional on live changes working. Smooth thermistor control needs further evidence. [Datasheet p. 29](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX77757.pdf)

An [ADI forum question asks exactly about changing current during charging](https://ez.analog.com/power/f/q-a/577726/max77757-change-charge-current-on-the-go-reverse-polarity-protection). The employee response only redirects to support, so it provides no confirmation.

## Available at JLCPCB

Snapshot of usable stock from JLCPCB's public product data (`canPresaleNumber`). Total inventory includes unavailable quantities. Recheck before ordering.

| Exact part | JLCPCB | Usable stock | Assessment |
|---|---|---:|---|
| BQ25601DRTWR | [C2859691](https://jlcpcb.com/partdetail/TexasInstruments-BQ25601DRTWR/C2859691) | 3,889 | I2C current control, BC1.2 detection, 4 × 4 mm QFN. Requires controller and external CC detection for the proposed policy. |
| BQ25601RTWR | [C468236](https://jlcpcb.com/partdetail/TexasInstruments-BQ25601RTWR/C468236) | 5,116 | Similar charger, but lacks the D variant's BC1.2 detection. More external circuitry. |
| BQ25895MRTWR | [C485917](https://jlcpcb.com/partdetail/TexasInstruments-BQ25895MRTWR/C485917) | 136 | BC1.2 and adjustable thermal regulation. Its 4.352 V reset voltage requires protection against unconfigured charging. |
| BQ25606RGER | [C374063](https://jlcpcb.com/partdetail/TexasInstruments-BQ25606RGER/C374063) | 8,358 | Standalone resistor control. Live current switching and CC limit enforcement remain unresolved. |
| MP2636GR-Z | [C128212](https://jlcpcb.com/partdetail/Monolithic_PowerSystems-MP2636GRZ/C128212) | 29 | USB bypass plus battery boost. Weak-USB supplementation remains unverified. Architecture study only. |
| FUSB303TMX | [C603072](https://jlcpcb.com/partdetail/onsemi-FUSB303TMX/C603072) | 5,013 | Autonomous CC detector. Does not program a charger's current or temperature settings. |

FUSB303 supports pin-configured sink operation, dead-battery handling and default/1.5 A/3 A current detection. I2C is optional. The controller still needs to translate detection results into charger input limits. [Onsemi datasheet pp. 1, 5, LCSC mirror](https://datasheet.lcsc.com/datasheet/pdf/a2c0fd00f7e33704519b0f60ca6bc884.pdf?productCode=C603072)

MAX77972, MAX77789 and BQ25640 remain technical references. Their JLC availability was reported insufficient by Mark. Only the quantities in the table above were independently checked here.

## Requirements retained

[Requirements](requirements.md): 1S LiPo, 5/10 Ah, USB 5 V only, 1.8 A nominal charging, simultaneous 5 V load of 0.8 A continuous / 1 A peak / 1.5 A briefly, battery supplement on weak USB, CC and BC1.2 detection. Charging must work with the display disconnected and physical load-off selected. Pack NTC and separate hardware shutdown remain independent.

No additional microcontroller. Controller-dependent alternatives below are outside the current scope. Their stock and technical details remain comparison data, not implementation recommendations.

## Practical comparison

Thermal settings below start charge-current reduction. They are not guaranteed equilibrium temperatures or hard junction ceilings. Current figures are register targets, subject to device tolerances.

| Part | Live current control | Junction regulation | Startup and watchdog | Primary source |
|---|---|---|---|---|
| **BQ25601D** | 60 mA steps, **1.8 / 1.5 / 1.2 A** | **90 or 110°C**, default 110 | Defaults 4.199 V / 2.04 A. Watchdog default 40 s, disable or service it. Expiry restores current and temperature defaults. | [Datasheet pp. 20-21, 36, 38-39](https://www.ti.com/lit/ds/symlink/bq25601d.pdf) |
| BQ25601 | Same current steps | 90 or 110°C | Similar host control, source detection external | [Datasheet](https://www.ti.com/lit/ds/symlink/bq25601.pdf) |
| BQ25895M | 64 mA steps, **1.792 / 1.472 / 1.216 A** | 60 / 80 / 100 / 120°C, default 120 | Defaults **4.352 V / 2.048 A**. HVDCP and MaxCharge disabled by default. Watchdog expiry restores charge-voltage defaults. | [Datasheet pp. 16, 31, 34, 36, 38-40](https://www.ti.com/lit/ds/symlink/bq25895m.pdf) |
| BQ24195 | Same approximate currents as BQ25895M | 60 / 80 / 100 / 120°C, default 120 | Defaults 4.208 V / 2.048 A. BC1.2 included, CC external. Watchdog and USB100/HIZ behavior need handling. Stock not checked. | [Datasheet pp. 13-17, 25, 29-31](https://www.ti.com/lit/ds/symlink/bq24195.pdf) |

BQ25601D can autonomously fold back at 90°C after initialization, but obtaining that setting still requires I2C configuration. Firmware current steps additionally require an external temperature input because it has no die-temperature telemetry. This does not meet the no-MCU scope. [Datasheet pp. 3, 20-21, 39](https://www.ti.com/lit/ds/symlink/bq25601d.pdf)

## Why the boost stays

For the TI chargers above, SYS is near battery voltage. Integrated boost reverses power toward USB or PMID. It does not establish an independent regulated 5 V system rail while charging and supplementing weak USB. Keep the existing system boost for this investigation. [BQ25601D pp. 3, 21, 26](https://www.ti.com/lit/ds/symlink/bq25601d.pdf), [BQ25895M boost operation](https://www.ti.com/lit/ds/symlink/bq25895m.pdf)

MP2636 instead bypasses USB to SYS when input is present and boosts the battery to SYS otherwise. That may reduce conversion losses, but SYS then follows USB and weak-source battery supplementation has not been established. It is not a qualified replacement. [MPS datasheet, operating modes and application information](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/EN/sku/MP2636/document_id/1443/)

## Options needing evidence

- **BQ25606:** ICHG resistor programming is documented, changing it during charging is not explicitly established. ILIM applies to unknown adapters. Recognized DCP selects 2.4 A, so switching ILIM resistors from a CC detector alone does not solve source-limit precedence. [Datasheet pp. 6, 18-19, 23](https://www.ti.com/lit/ds/symlink/bq25606.pdf), [design note](https://www.ti.com/lit/an/slva924/slva924.pdf)
- **MAX77972/MAX77789:** configurable 85-130°C foldback starts, but initialization is required for a lower target. MAX77972 configuration is RAM, rewritten after power restoration. [MAX77972 guide p. 1](https://www.analog.com/media/en/technical-documentation/user-guides/max77972-software-implementation-guide.pdf), [MAX77789 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max77789.pdf)
- **BQ25640:** integrated CC/BC1.2 and 20 mA current steps, but native thermal settings are only 60/120°C. [Datasheet](https://www.ti.com/lit/ds/symlink/bq25640.pdf)
- **BQ25896:** unlike BQ25895/M, it requires external USB source detection through PSEL. Do not group it with parts containing BC1.2 detection. [Datasheet, input-source detection](https://www.ti.com/lit/ds/symlink/bq25896.pdf)

## Continue the selection

Start with usable assembly stock, then [TI's charger catalog](https://www.ti.com/product-category/battery-management-ics/battery-charger-ics/overview.html) and [ADI's selector](https://www.analog.com/en/parametricsearch/11467). Record exact variant, package, SYS/boost topology, default voltage/current, USB detection, live current command, thermal settings, reset behavior, NTC and charge timer. Marketing summaries omit several of these.

Next establish whether MAX77757 samples IFAST during charging, how resistor changes affect the charge cycle, and which intermediate values are supported. Until then, hardware current steps and smooth taper remain unverified proposals. No board, simulation or measured thermal result changes follow from this research.
