# TUSB320 alternative investigation

## Question and decision

The required baseline is a 3.3 V-powered USB-C sink controller that presents dead-battery Rd, detects attach and the source's default, 1.5 A or 3 A advertisement, and can be polled without consuming an ESP32 GPIO. U15 currently occupies a 1.6 x 1.6 mm RWB X2QFN site at `(139.827, 118.364)`, with CC1, CC2, VBUS_DET through R79, SDA, SCL, +ESP_3V3 and C57 100 nF; PORT, ADDR and EN_N are strapped low and INT_N and ID are open ([board](../hardware/carrier-ti-charger/carrier.kicad_pcb), [power design](power.md#charger-variant-bq25798-carrier-ti-charger)).

**Decision: replace TUSB320LIRWBR with onsemi FUSB303BTMX and run the bus at 400 kHz.** The 1.6 x 1.6 mm, 0.4 mm pitch package fits the existing land pattern and surrounding placement. CC1, CC2, VBUS_DET, SDA, SCL, VDD, GND, EN_N, INT_N and ID retain their pad functions. Pads 3 and 5 need 910 kΩ straps instead of direct ground because PORT/DEBUG_N and ADDR/ORIENT become outputs after startup.

TI's March 2026 [CC-controller selection guide](https://www.ti.com/lit/an/sdaa284/sdaa284.pdf) identifies the TUSB320 variants as its I2C family, the TUSB321/TUSB322 variants as the GPIO family, and TUSB422 as the only PD-PHY family member. This bounds the relevant TI lineup; larger TPS257xx PD controllers were screened separately below.

## Candidate screen

| Exact orderable part | Electrical and functional fit | Package and board impact | Result |
|---|---|---|---|
| **FUSB303BTMX** | 2.4 to 5.5 V VDD, dead-battery Rd, sink current detection, address 0x21, 400 kHz I2C with 400 pF and 250 ns limits, 20 mA SDA sink guarantee. | Same 1.6 x 1.6 mm body and 0.4 mm pitch. Existing functional pads align; adds two 0402 910 kΩ straps and firmware must set `CONTROL1.ENABLE`. | **Selected.** Provides the required baseline and a margin-backed 200 pF Fast-mode design. [Data sheet](../datasheets/fusb303b.pdf). |
| **TUSB320LAIRWBR** | Active-low enable, 2.7 to 5 V VDD, dead-battery Rd, current detection, addresses 0x47/0x67. Same 1.6 mA at VOL 0.4 V and same 100 pF limit at 400 kHz as U15. | Same RWB-12 body, pinout and passives; a literal footprint replacement. | **Refuted.** Fits, but does not improve I2C. [Data sheet](https://www.ti.com/lit/ds/symlink/tusb320hai.pdf). |
| **TUSB320HIRWBR / TUSB320HAIRWBR** | Same CC and I2C capability and limits; active-high enable. | Existing pad 11 is tied to GND for active-low EN_N, so these variants would be disabled. | **Refuted.** No I2C gain and not electrically drop-in. [LI/HI data sheet](https://www.ti.com/lit/ds/symlink/tusb320hi.pdf), [LAI/HAI data sheet](https://www.ti.com/lit/ds/symlink/tusb320hai.pdf). |
| **TUSB322IRWBR** | Detects source current and has I2C at 0x47/0x67, but still specifies 1.6 mA at VOL 0.4 V and 100 pF at 400 kHz. It requires 4.5 to 5.5 V VDD. | Same body size, different pins: pad 3 becomes VBUS_DET, pad 4 DIR, pad 6 VCONN. The current 3.3 V supply and routing do not work. | **Refuted.** Same I2C limit plus supply and pinout redesign. [Data sheet](https://www.ti.com/lit/ds/symlink/tusb322i.pdf). |
| **TUSB321AIRWBR** | GPIO-only current detection through OUT1/OUT2 removes the CC controller from I2C and retains dead-battery Rd, but requires 4.5 to 5.5 V VDD. | Same 1.6 x 1.6 mm RWB body, but needs two routed inputs to the TCA9534. U13 is about 62 mm away and its spare pins are already routed to J12. Powering from VBUS also needs protection against back-powering the 3.3 V GPIO side. | **Refuted.** It trades the I2C issue for 5 V supply, level/power sequencing and long routing changes. [Data sheet](https://www.ti.com/lit/ds/symlink/tusb321ai.pdf). |
| **TUSB422IYFPR / TUSB422IYFPT** | 2.7 to 5.5 V VDD, dead-battery Rd, reset role 0x0A presents Rd on both CC pins, and CC Status reports default, 1.5 A or 3 A. I2C supports 1 MHz, 150 pF at 400 kHz and 1 MHz, 6 mA sink at VOL 0.4 V, and 10 pF pin capacitance. | Smaller 1.41 x 1.365 mm, 9-ball DSBGA, 0.4 mm pitch. Needs direct VBUSIN, two 200 to 450 pF CC shunts, and new TCPC firmware. Fixed address 0x20 conflicts with U13. | **I2C improved, system fit refuted.** Details below. [Data sheet](https://www.ti.com/lit/ds/symlink/tusb422.pdf), [product page](https://www.ti.com/product/TUSB422). |
| **TPS25730ASRSMR / TPS25751SRSMR** | Active sink-capable PD controllers with dead-battery support and I2C access. Their power-path, PD policy and configuration functions exceed the required 5 V CC advertisement reader. | 4 x 4 mm, 32-pin VQFN plus power-path and configuration circuitry. The present U15 area is bounded by C57 and R79 and does not hold this implementation without moving the surrounding USB and power circuitry. | **Refuted.** Different subsystem, not a TUSB320 replacement. [TPS25730A](https://www.ti.com/product/TPS25730A), [TPS25751](https://www.ti.com/product/TPS25751). |

All listed TUSB orderables are marked active production in their current TI data-sheet package addenda. Lifecycle does not rescue a rejected electrical or layout fit.

## JLCPCB stock after fit screening

Stock was checked on 2026-10-15 and is volatile. Only the current part and the two candidates that survive far enough to merit sourcing are included.

| Part | JLCPCB part | Stock | Meaning |
|---|---:|---:|---|
| FUSB303BTMX | [C895444](https://jlcpcb.com/partdetail/onsemi-FUSB303BTMX/C895444) | 5,024 | Selected controller, extended, exact orderable part. |
| 0402WGF9103TCE | [C25800](https://jlcpcb.com/partdetail/UniroyalElec-0402WGF9103TCE/C25800) | 7,800 | Selected 910 kΩ, 1% straps R80/R81. |
| 0603WAF1801T5E | [C4177](https://jlcpcb.com/partdetail/UniroyalElec-0603WAF1801T5E/C4177) | 389,700 | Selected 1.8 kΩ, 1% pull-ups R19/R20. |
| TUSB320LIRWBR | [C2836598](https://jlcpcb.com/partdetail/TexasInstruments-TUSB320LIRWBR/C2836598) | 5,909 | Former part, extended, healthy stock. |
| TUSB320LAIRWBR | [C132554](https://jlcpcb.com/partdetail/TexasInstruments-TUSB320LAIRWBR/C132554) | 2 | Exact physical substitute, but no I2C improvement and not a useful stock fallback. |
| TUSB422IYFPR | [C702378](https://jlcpcb.com/partdetail/TexasInstruments-TUSB422IYFPR/C702378) | 229 | Extended and orderable, but rejected on system fit. |

No exact JLCPCB listing was found for TUSB322IRWBR. It was already rejected on electrical and pinout grounds, so sourcing it elsewhere would not change the decision.

## TUSB422 cross-check

The TUSB422 is the only hypothesis that improves the limiting I2C participant. At 3.6 V pull-up maximum, its 6 mA low-level capability gives a theoretical `Rp(min) = (3.6 - 0.4) / 0.006 = 533 ohm`; TI instead specifies 620 to 2200 ohm for a 150 pF, 400 kHz bus. The current 2.2 kΩ carrier pull-ups in parallel with the display's 5.1 kΩ give 1.54 kΩ, inside that device-specific range. The existing [145 pF estimate and 200 pF design envelope](i2c-bus-requirements.md) leave only 5 pF below TUSB422's absolute bus-capacitance limit, and the AXS5106L sink-current and pin-capacitance limits remain unpublished. A TUSB422 therefore does not establish a margin-backed 400 kHz system.

It also does not fit the present architecture cleanly:

- TUSB422's fixed seven-bit address is 0x20. U13 already uses 0x20 because A0, A1 and A2 are directly grounded ([U13 board pads](../hardware/carrier-ti-charger/carrier.kicad_pcb), [interface map](interfaces.md#display-seeed-100069905)). Changing either side requires board work.
- The body fits inside U15's 2.3 x 2.3 mm courtyard, but it is not footprint compatible. C57 can remain the required 100 nF VDD bypass and VCONN may float in this sink-only use. R79's 887 kΩ VBUS_DET feed must become a direct VBUSIN connection, and two CC shunt capacitors must be added and placed close to the device. This area fit is plausible, not placement-and-routing proven.
- The center ball B2 is INT_N. Polling can leave it unused, so the package does not inherently force via-in-pad. Routing INT_N would require via-in-pad or TI's 2 mil escape, both outside the current 6 mil board rules ([design rules](../hardware/carrier-ti-charger/carrier.kicad_dru), [board setup](../hardware/carrier-ti-charger/carrier.kicad_pro)).
- Firmware must move from the small TUSB320 register map to the TCPC-style TUSB422 map. Reset already selects Rd on both CC pins, but software must issue `Look4Connection` and read `CC_STATUS.CC1_STATE/CC2_STATE` for current advertisement.

## Hypotheses

| Hypothesis | Evidence | State |
|---|---|---|
| A non-TI controller fits the site and removes the I2C limit. | FUSB303BTMX retains the functional pad map, fits the land pattern, supports 400 pF Fast mode and is stocked by JLCPCB. Two high-value straps resolve its multiplexed pins. | **Supported and selected** |
| A newer pin-compatible TUSB320 variant removes the I2C restriction. | TUSB320LAI is pin-compatible but retains 1.6 mA and 100 pF at 400 kHz; active-high variants also conflict with the EN_N strap. | **Refuted** |
| TUSB422 provides the same baseline with better I2C. | Electrical functions and I2C are supported, but address 0x20 collides, 150 pF has no design margin, and the replacement needs new passives, routing and firmware. | **Refuted for this board** |
| A GPIO CC controller removes the I2C limit cheaply. | TUSB321AI reports the three current levels, but needs 5 V and two long GPIO routes to U13. | **Refuted** |

The schematic and unrouted placement now use FUSB303BTMX. Remaining proof is PCB routing, ERC/DRC closure, firmware implementation and prototype measurement of the shared bus, especially the undocumented AXS5106L electrical limits.
