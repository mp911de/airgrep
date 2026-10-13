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

## Firmware dependency

Keep the display-to-ADSBee UART wiring. The local custom ADSBee `airgrep` build profile accepts
AT commands on COMMS_UART and returns replies there alongside traffic output.
The agreed link is 115200 baud, 8E1, no flow control, with CSBEE traffic and CRC16.
Display integration and physical verification remain open. See the
[UART contract](../docs/firmware.md#adsbee-uart).

Sources: [AT implementation](https://github.com/PantsForBirds/adsbee/blob/main/firmware/adsbee_1090/pico/application/comms/at/comms_at.cc),
[application build configuration](https://github.com/PantsForBirds/adsbee/blob/main/firmware/adsbee_1090/pico/application/CMakeLists.txt).

