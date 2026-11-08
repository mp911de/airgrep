# Firmware

This is the ESP32-S3 project for the display module or the bare XIAO ESP32-S3 Plus.
Behaviour is specified in [docs/firmware.md](../docs/firmware.md), pins
in [docs/interfaces.md](../docs/interfaces.md). `adsbee-hackpad.txt` and
`gps-sentences.txt` are working notes.

## Toolchain

ESP-IDF v6.1 installed by the ESP-IDF Installation Manager. Activate it in a shell with

```sh
source ~/.espressif/tools/activate_idf_v6.1.sh
```

## Presets

`CMakePresets.json` defines one preset per module and purpose. Each builds into
`build/<preset>/` with its own `sdkconfig`, layered from `sdkconfig.defaults`,
`sdkconfig.board.<preset>` and, for tests, `sdkconfig.test`.

| Preset           | Module                                                       | Binary             |
|------------------|--------------------------------------------------------------|--------------------|
| `display`        | Seeed 1.47 inch touch display, UART GPIO39/40, buzzer GPIO38 | application        |
| `s3`             | bare XIAO ESP32-S3 Plus, UART GPIO7/44, buzzer GPIO8         | application        |
| `display-test`   | as `display`                                                 | Unity test runner  |
| `s3-test`        | as `s3`                                                      | Unity test runner  |
| `display-hwtest` | as `display`, `sdkconfig.hwtest`                             | hardware test mode |
| `s3-hwtest`      | as `s3`, `sdkconfig.hwtest`                                  | hardware test mode |

Command line, from `firmware/`:

```sh
idf.py --preset s3 build
idf.py --preset s3 -p /dev/cu.usbmodem* flash monitor   # Ctrl+] exits the monitor
idf.py --preset s3-test menuconfig                      # airgrep > tags to run at boot
```

The XIAO programs over its native USB port, the console (`ESP_LOGx`) uses USB Serial/JTAG.
Log level defaults to info, debug is compiled in. The application runs the IDF console on
the same port: `help` lists command synopses, `help <cmd>` describes a command,
`uptime` shows reset reason and heap statistics, and `reboot` flushes the output
before restarting. Tags: `airgrep`, `loop`, `adsbee`, `i2c`.

Once the prompt opens, logs go to a 16 KB PSRAM ring, with internal RAM fallback.
`logs` replays and tails until a key, `logs debug` temporarily raises the default
level, `logs debug i2c,loop` temporarily raises and
filters those tags, and `logs -n 20` prints only the last 20 lines. Formatted log
fragments longer than 1023 bytes are marked truncated. Temporary levels preserve
explicit tag settings and default inheritance when the session ends.

Hardware test mode boots the normal application and loop, then prints one line per check,
`HWTEST <name> PASS|FAIL <detail>`, for a host script to judge. Checks include
charger, gauge, barometer, USB-C controller and expander identity reads.

A fresh build directory configured through a preset needs the toolchain file once:
`idf.py --preset s3-test -DCMAKE_TOOLCHAIN_FILE=$IDF_PATH/tools/cmake/toolchain-esp32s3.cmake reconfigure`.

## Tests

Tests live in `components/<name>/test/` as Unity `TEST_CASE`s. Tags: `[adsbee]` decoder,
pure logic. `[logs]` exercises log ring replay, wrap and filtering in RAM.
`[expander]` checks each TCA9534 register decoder in RAM.
`[charger]` checks BQ25798 register decoding against datasheet values.
`[battery]` checks MAX17048 registers and remaining-time calculations.
`[barometer]` checks BMP580 registers and standard pressure altitude.
`[fusb]` checks FUSB303B registers and validity of CC current advertisements.
`[buzzer]` checks beep schemes, frequency selection and mute gating.
`[loop]` runs the real loop task with a 10 ms tick (`sdkconfig.test`) and
asserts through the public API only. Hardware paths have no Unity tests, hardware test
mode covers them. The test build runs the tags from `CONFIG_AIRGREP_TEST_TAGS` at boot,
then opens the Unity menu on the console: Enter lists the tests, a quoted name, an index,
a `[tag]` or `*` runs them.

## CLion

1. Settings | Build, Execution, Deployment | Toolchains: add a toolchain, Add
   environment | From file, pick `~/.espressif/tools/activate_idf_v6.1.sh`.
2. Open `firmware`. CLion reads `CMakePresets.json`; enable the presets you need under
   Settings | Build, Execution, Deployment | CMake and assign the toolchain.
3. Run targets `flash` and `monitor` appear per profile. If the port is not found, add
   `ESPPORT=/dev/cu.usbmodemXXXX` to the profile's environment.
4. Debugging: Settings | Advanced Settings | Debugger, enable Debug Servers, add a generic
   server with
   `~/.espressif/tools/openocd-esp32/<version>/openocd-esp32/bin/openocd -f board/esp32s3-builtin.cfg`,
   debugger `xtensa-esp32s3-elf-gdb`, connection `tcp::3333`. Espressif's
   walkthrough: https://developer.espressif.com/blog/clion/

## Layout

```
firmware/
  main/            main.cpp and console.c (application) or test_main.c (test preset), Kconfig.projbuild
  components/
    loop/          event loop task, promises, scheduling, Kconfig, test/
    airgrep/       carrier hardware: board.h pin map, I2C bus, buzzer, ADSBee UART link and decoder, Kconfig, test/
    hwtest/        hardware test mode checks and HWTEST report line
```

Boot order in `main.cpp`: version and build banner, loop, I2C, charger, battery, barometer, USB-C controller, expander, buzzer, ADSBee link, console. Every
request goes through the loop, so a silent or unpowered part costs one timeout.

`exp` prints the last expander sample and its age. `exp --trigger N` pulses P2
through P5 for 10 ms. P2/P3 are driven low only, then released to input. P4/P5
need an extra latch preparation and restoration around those two direction
writes, because their idle preload is high. Each write is a separate loop action.
The TCA9534 has no identity register, so its boot check reads INPUT_PORT.

`charger` prints the latest 2 s sample, faults, ADC readings and configuration.
Only the ADC, ship-FET presence and 20 s watchdog are configured. Polls read
WD_STAT before resetting the watchdog, and reapply configuration after expiry.
ADC values remain pending until a full conversion cycle has elapsed. IBAT
discharge sensing is left unchanged and reported unavailable when disabled.

`battery_level` prints the latest 2 s gauge sample, charge or discharge rate,
remaining time and alerts, including RI. Estimates are hidden below 1 %/h or
outside 0 to 100 % state of charge. Gauge configuration and alerts are retained.

`baro` prints the latest 1 s sample and configuration. Pressure is enabled in
normal mode at 1 Hz, with existing OSR and IIR settings retained. `baro --reset`
returns after the reset write, accepting the expected NACK. The next poll reads
identity and reapplies configuration. Pressure altitude uses 1013.25 hPa.

`fusb` prints the latest 250 ms USB-C sample and the CC-derived current limit.
Explicit limits require an attached sink with valid VBUS and orientation. Default
current leaves the charger BC1.2 result to decide. `fusb --reset` returns after
the reset write, then polling re-identifies and enables the controller after 100 ms.

`beep` shows mute, playback and scheme names. `beep --short`, `--long`, `--double`
and `--alarm` use 4000 Hz unless a frequency follows the option. Double uses a
50 ms gap, alarm alternates 300 ms on and off. `--sweep` steps from 3500 to
4500 Hz in 100 Hz, 50 ms steps. `beep off` stops any scheme, including while muted.
Playback stays silent until the first valid mute read. Muting stops a running
scheme, unmuting does not resume it. A failed expander read retains the mute state.

`adsbee` shows link state, TX attachment, last update, ping age from the last AT reply,
and CSBEE traffic/error counters.
The RX task and a 100 ms liveness poll run regardless of ADSBee presence or power.
TX stays high impedance with pulls disabled until power is on and a checksum-valid
AIRGREP frame arrives in that power cycle. It detaches on power-off, expander errors,
a power sample older than 500 ms, or three seconds without valid telemetry.
`Liveness::rx_timeout_us` sets that RX timeout. Power loss clears readiness, so
each new power cycle waits for new RX. No AT commands are sent automatically.
`adsbee --at AT+DEVICE_INFO?` sends
one command and collects its multiline reply for 900 ms, up to 2047 bytes. Reply lines
go to the debug log, the `AT+DEVICE_INFO?` reply is also kept for `adsbee`. Console
writes print OK when the loop accepted the action, their outcome shows in the State.
Overflow fails visibly. The ADSBee protocol has no reply terminator or correlation
ID, so this bounded window cannot prove completeness or identify late replies.
CSBee and M10 frames use the same trailing-comma CRC check before dispatch.
`[adsbee]` tests CRC, reply bounds, power cycles and RX expiry in RAM.
The console uses an 8 KB stack for state output.
