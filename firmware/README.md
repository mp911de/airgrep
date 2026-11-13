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
| `s3-uart0`       | as `s3`, `sdkconfig.uart0`, console on UART0 GPIO1/2         | application        |
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
