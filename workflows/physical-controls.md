# Physical mute and Wi-Fi controls

Status: hardware requirements agreed. Component selection and electrical design
remain to be completed before schematic and PCB implementation.

## Purpose

Operate frequently used functions without navigating the small touchscreen.
The ADSBee Wi-Fi feed serves the EFB; switching Wi-Fi off also saves battery
power. Physical controls must be included in the first carrier revision.

## Agreed scope

- Retain the maintained MUTE slider, SW2.
- Change mute to a firmware-controlled function. The ESP32 already generates
  buzzer PWM and will decide whether to generate it from the slider input.
- An independent hardware mute guarantee is not required. The POWER slider
  provides the user's recovery path if firmware hangs.
- Add a second maintained slider on the left side for ADSBee Wi-Fi.
- Provide input expansion so additional controls can be connected later.
- Both slider positions determine the requested state from startup onward.
  Keep the buzzer silent until its position has been read. The touchscreen
  displays state and does not override either slider.
- Low battery must not automatically disable Wi-Fi while its slider is ON.
  Show a low-battery warning; automatic backlight dimming may remain.
- Six inputs total are sufficient: two sliders and at least four spare inputs.
  Optimize for power consumption, available assembly stock and assembled cost.
  Eight inputs are acceptable if that is the better practical choice.
- Custom ADSBee firmware is already planned for FLARM support. Add wired Wi-Fi
  control to that firmware over the existing display-to-ADSBee UART.
- If the expander becomes unavailable, disregard unavailable input and retain
  the last known state of both controls. A read failure must not change mute or
  Wi-Fi state.

## Proposed implementation basis

An I2C GPIO expander on the existing display bus reads the controls. Retain
buzzer PWM on D11 and the direct display-to-ADSBee UART. Expect polling because
the currently available display GPIOs are occupied. Part, address, connector
arrangement and layout remain open.

The existing UART is not a demonstrated control transport with stock firmware.
The carrier documentation claims AT commands over COMMS_UART, but the local
ADSBee datasheet describes AT commands over CONSOLE. Upstream source inspected
on 2026-10-06 enables USB console and disables UART stdio; its AT parser reads
stdio and network-console input, with no COMMS_UART AT intake found.

Keep the current carrier UART wiring. The planned custom ADSBee firmware must
add command intake and responses alongside traffic output. This is an accepted
firmware dependency; it has not been implemented or verified on hardware.

Sources: [ADSBee AT implementation](https://github.com/PantsForBirds/adsbee/blob/main/firmware/adsbee_1090/pico/application/comms/at/comms_at.cc),
[application build configuration](https://github.com/PantsForBirds/adsbee/blob/main/firmware/adsbee_1090/pico/application/CMakeLists.txt).

The existing buzzer path is D11 to U9, R28, SW2 and BZ1. Reusing SW2 as an input
requires changing that path. This draft does not describe the current wiring.

### Expander selection, provisional

Prefer TI TCA9534PWR, JLCPCB C783615, subject to a current assembly quote and
confirmation of all display-module I2C addresses. It provides eight channels in
TSSOP-16, operates at 3.3 V and supports the existing 100 kHz bus. Its selectable
addresses are 0x20 to 0x27. The datasheet specifies at most 4 uA standby current
at 3.6 V with inputs low, and 2.2 uA with inputs high.

The JLCPCB catalog result retrieved on 2026-10-06 was indexed the previous week:
15,412 stock and USD 0.7132 at quantity one, Extended part. These are cached
catalog figures, not confirmed live allocatable stock or assembled cost.
Include feeder fees, placement and supporting parts in the final comparison.

TI PCA9534PWR, C133803, is a suitable alternative without a verified price
advantage. NXP PCF8574T/3,518, C7605, was less attractive: the indexed price was
USD 2.1193 at quantity one and its weak high-level output sources 30 to 300 uA
into each grounded input.

TCA9534 inputs need external bias. Include switch pull-resistor current in the
power comparison and define every unused input. Retained SPDT sliders can select
between rails, with weak bias during contact transitions; verify the complete
input circuit before implementation. Do not automatically populate extra I2C
bus pull-ups.

Sources: [TI TCA9534](https://www.ti.com/lit/ds/symlink/tca9534.pdf),
[JLCPCB C783615](https://jlcpcb.com/partdetail/TexasInstruments-TCA9534PWR/C783615),
[TI PCA9534](https://www.ti.com/lit/ds/symlink/pca9534.pdf),
[NXP PCF8574](https://www.nxp.com/docs/en/data-sheet/PCF8574_PCF8574A.pdf),
[JLCPCB C7605](https://jlcpcb.com/partdetail/NXPSemicon-PCF8574T_3518/C7605).

## Future ESP firmware work

Show a system failure icon when a peripheral fails. This shared fail mode applies
to the expander, fuel gauge and BMP barometer. For expander failure, preserve both
last known control states. Fault detection, recovery, polling, debounce and screen
behavior belong to the future ESP firmware work, not this hardware change.

The firmware must apply slider positions at startup. Detailed startup sequencing
between the display and ADSBee, including any interval before the Wi-Fi setting
is applied, belongs to that firmware work.

## Remaining electrical design work

- Confirm current assembly stock and total cost, then select the expander.
- Verify all shared-bus addresses and select a non-conflicting address.
- Define switch input bias, unused-input levels, decoupling and bus loading.
- Bring at least four spare inputs to a connector for future enclosure controls;
  select its pinout and footprint during the electrical design.
- Place the added Wi-Fi slider on the left edge and check enclosure access,
  component clearance and routing against the live board.

## Evidence and affected documentation

- [Requirements](../docs/requirements.md): function requirement 6 currently
  specifies hardware mute and excludes firmware mute; that is superseded by
  the agreed scope above, pending reconciliation of the project documents.
- [Interfaces](../docs/interfaces.md): existing GPIO allocation, buzzer drive
  and shared I2C bus.
- [Mechanical design](../docs/mechanical.md): SW2 on the left edge.
- [Open issues](../docs/open-issues.md): currently proposes automatic Wi-Fi
  shutdown at low state of charge; remove that proposal when reconciling the
  documents. Wi-Fi remains under slider control.

## Completion

Complete the electrical design and sourcing checks before hardware
implementation. Hardware work must include schematic and PCB changes,
project-document reconciliation and the required tools/check.sh run after each
schematic or board change. No hardware files have been changed for this spec.
