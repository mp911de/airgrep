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
the same port: `help`, `log_level <tag|*> <level>`. Tags: `airgrep`, `loop`, `adsbee`,
`i2c`.

Hardware test mode boots the normal application and loop, then prints one line per check,
`HWTEST <name> PASS|FAIL <detail>`, for a host script to judge. First check: I2C presence
of
the gauge (0x36) and the USB controller (0x21).

A fresh build directory configured through a preset needs the toolchain file once:
`idf.py --preset s3-test -DCMAKE_TOOLCHAIN_FILE=$IDF_PATH/tools/cmake/toolchain-esp32s3.cmake reconfigure`.

## Tests

Tests live in `components/<name>/test/` as Unity `TEST_CASE`s. Tags: `[adsbee]` decoder,
pure logic. `[loop]` runs the real loop task with a 10 ms tick (`sdkconfig.test`) and
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

Boot order in `main.cpp`: loop first, then console, I2C, buzzer, ADSBee link. Every
request goes through the loop, so a silent or unpowered part costs one timeout.
