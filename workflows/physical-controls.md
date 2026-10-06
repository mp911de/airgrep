# Physical mute and Wi-Fi controls

Status: schematic and PCB implemented for revision 1. Firmware and physical
bring-up remain open. The hardware is not a fabrication release.

## Agreed behavior

- SW1 remains an independent maintained POWER switch and the recovery control
  if firmware hangs.
- SW2 remains a maintained MUTE slider. The ESP reads it through the expander
  and decides whether to generate buzzer PWM on D11.
- SW3 is a maintained Wi-Fi slider on the left edge, included in revision 1.
  It controls ADSBee Wi-Fi, primarily the EFB feed and secondarily battery use.
- Both slider positions determine requested state from startup. Keep PWM silent
  until the first valid mute read. The touchscreen displays the states without
  overriding them.
- Low battery does not override Wi-Fi ON. Show a warning; backlight dimming may
  remain.
- If an expander read fails, retain both last known control states.
- Future ESP firmware shows a shared system-failure icon for expander, gauge
  and BMP faults. Polling, debounce, fault detection/recovery and startup
  sequencing belong to that firmware work.

## Implemented circuit

- U13: TI TCA9534PWR, C783615, TSSOP-16, supplied by ESP_3V3. A0-A2 are grounded
  for address 0x20. C40 is 100 nF local decoupling. Its ground uses an explicit
  trace and via rather than a zone thermal connection.
- Existing 100 kHz I2C bus; no extra bus pull-ups. R19/R20 remain DNP. INT is
  unconnected because the exposed display GPIOs are already allocated.
- P0 is BUZZ_ENABLE from SW2; P1 is WIFI_ENABLE from SW3. Common pin 2 goes to
  the input, pin 3 to GND, and pin 1 is unconnected. Low means muted or Wi-Fi
  off; high permits buzzer output or requests Wi-Fi on.
- R48-R55 are 100 kOhm pull-ups to ESP_3V3, one per P0-P7. All inputs therefore
  have defined levels, including unused expansion inputs.
- J12 is a hand-fitted 2x4, 2.54 mm header: pins 1-6 are P2-P7; pins 7 and 8
  are GND. It provides six spare inputs for short enclosure wiring and dry
  contacts to ground. It is DNP and excluded from assembly BOM/CPL.
- D11 -> U9 -> R28 (150 Ohm) -> BZ1 is now direct. BUZZ_LIM reaches BZ1;
  BUZZ_PLUS and the hard mute connection have been removed.
- SW1-SW3 all use HRO K3-1280S-F1, C92658, and the new local footprint.

The shared addresses are 0x20 (TCA9534), 0x36 (MAX17048), 0x46 (BMP581),
0x63 (AXS5106L touch), and 0x6A (LSM6DS3 IMU). Seeed documents the two display
addresses in its [peripheral guide](https://wiki.seeedstudio.com/function_1.47_inch_touch_display_esp32s3/).
Validate the assembled bus and its rise time at bring-up.

The [TI datasheet](../datasheets/tca9534.pdf) specifies at most 4 uA standby at
3.6 V with inputs low, or 2.2 uA with inputs high. Each grounded 100 kOhm input
adds 33 uA at 3.3 V. The power model allows 0.1 mA for the IC and two grounded
sliders, including an estimated polling allowance. Each grounded spare adds
another 33 uA; all eight pulls would draw 264 uA. Measure polling current and
bus loading on the prototype.

## Component selection

Eight channels satisfy the requirement for at least six total. TCA9534PWR
provides low standby current in TSSOP-16 and was the preferred stocked assembly
option in the comparison. TI PCA9534PWR (C133803) had no verified price advantage.
PCF8574T/3,518 (C7605) was less attractive because each grounded quasi-bidirectional
input sinks its 30-300 uA weak source and its indexed unit price was higher.

The JLCPCB result retrieved on 2026-10-06 listed 15,412 TCA9534 units and USD
0.7132 at quantity one, Extended. It was indexed the previous week, so this is
not a live allocation or assembled quote. Recheck stock, feeder fees and placement
cost at order time. [JLCPCB C783615](https://jlcpcb.com/partdetail/TexasInstruments-TCA9534PWR/C783615).

## Switch footprint and assembly evidence

The [HRO manufacturer drawing](../datasheets/hro-k3-1280s.pdf), dated 2013-09-26,
is the footprint source. It specifies 2.50 mm terminal pitch, 1.20 x 2.45 mm
solder lands, 0.90 mm peg holes spaced 6.80 mm, and a peg-line to solder-land
centre offset of (1.85 + 4.30) / 2 = 3.075 mm. The pattern-side drawing is mirrored
for the board top view. The origin is common terminal 2. We number the two outer
terminals locally; the manufacturer drawing does not number them.

A read of the [C92658 supplier CAD API](https://easyeda.com/api/products/C92658/components?version=6.5.51)
on 2026-10-06 exposed inconsistent data: symbol title K3-1280S-F2, manufacturer
field K3-1280S-F1, and package SW-SMD_K3-1280S-K1. Package UUID is
cb2986fb1dba498699763b187255a8d2. Its pad-2 origin is (3986.695, 3000) and its peg
X is 4000.105 in 0.254 mm units, giving 3.406 mm rather than 3.075 mm. Its 3D model
is also K1, so it is not assigned to the board. The LCSC [product page](https://www.lcsc.com/product-detail/C92658.html)
continues to identify C92658 as F1.

`tools/fix_cpl.py` now handles the new footprint: offset (0, 0), rotation +270
degrees. The supplier origin is the common pad and its actuator points right;
our unrotated footprint's actuator points down. Aligning the body reverses the
supplier's outer-terminal numbering relative to our local convention. The
middle/common terminal agrees. Thus SW1 exports at 270 degrees and SW2/SW3 at
180 degrees. This is derived from supplier CAD geometry, not a JLCPCB placement
preview or physical fit test. The supplier CAD discrepancy must be resolved by
checking an F1 sample and the assembly preview before ordering.

The board positions and enclosure overhangs are in [mechanical.md](../docs/mechanical.md).
No verified F1 3D model is available in the project. All SMD parts remain on top.

## Firmware dependency

Keep the display-to-ADSBee UART wiring. Stock ADSBee firmware has not established
AT command intake on COMMS_UART: the local datasheet describes AT commands on
CONSOLE, and the inspected upstream parser reads stdio/network-console input.
Custom ADSBee firmware is already planned for FLARM; it must add wired Wi-Fi
command intake and responses alongside traffic output. No expander coordination
protocol or additional hardware signal is required.

Sources: [AT implementation](https://github.com/PantsForBirds/adsbee/blob/main/firmware/adsbee_1090/pico/application/comms/at/comms_at.cc),
[application build configuration](https://github.com/PantsForBirds/adsbee/blob/main/firmware/adsbee_1090/pico/application/CMakeLists.txt).

## Verification

`tools/check.sh` passes: zero ERC errors/warnings, zero DRC errors/warnings,
zero unconnected items and parity issues, 108 matching nets, 127 matching
references, 45 model bindings and 20 existing simulation tests. Zone fills are
saved. Missing-model warnings remain, including the three new switches.

The schematic and placement drawings were refreshed. Production Gerbers/BOM/CPL
were not regenerated; the existing exports were already stale. Firmware,
mechanical sample fit, supplier assembly preview, live bus behavior and switch
failure handling have not been tested on hardware. These remain explicit items
in [open issues](../docs/open-issues.md).
