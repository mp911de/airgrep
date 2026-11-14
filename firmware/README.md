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
idf.py --preset s3 -p /dev/cu.usbmodem2101 flash        # select the ROM download port
idf.py --preset s3-test menuconfig                      # airgrep > tags to run at boot
```

For the complete build, flash and monitor workflow, activate ESP-IDF and run:

```sh
./tools/build-flash.py
./tools/build-flash.py --profile s3 --serial 68ee8f4d0c74
```

The profile defaults to `s3`; `display`, `s3-hwtest` and `display-hwtest` are also
supported. Without `--serial`, the first discovered device is selected. The tool
builds before requesting bootloader entry, tracks the same
factory MAC through ROM and application enumeration, flashes, then launches IDF
Monitor on the new console port. Exit the monitor with Ctrl+]. A device already
in ROM download mode is supported. The monitor uses a temporary DTR setting for
the CDC console. Its upload shortcuts do not perform this USB discovery sequence;
exit and rerun the tool to flash again.

## Simulator

`simulator/` builds the user interface from `components/touch-lcd` as a macOS
application, a 172 x 320 window driven by LVGL's SDL driver. The mouse acts as
the finger. It needs CMake, Ninja and `brew install sdl2`, not the IDF. The first
configure downloads LVGL, pinned in `simulator/CMakeLists.txt`.

```sh
cmake -S simulator -B simulator/build -G Ninja
cmake --build simulator/build
simulator/build/airgrep-simulator.app/Contents/MacOS/airgrep-simulator --zoom 2
```

`--zoom` takes 1, 2 or 4 and defaults to 2. The View menu switches it at runtime,
Cmd+1, Cmd+2 and Cmd+4. Each panel pixel is drawn as a block of one colour.
Touches are logged in panel coordinates. Closing the window ends the process.

## USB console and control

Application presets use TinyUSB with one CDC console and one vendor-specific
USB bulk interface carrying the [binary protocol](../docs/usb-data-protocol.md).
The USB product is `airgrep-<factory MAC>` and the serial number is the factory
MAC alone, twelve lowercase hexadecimal digits. The console uses interfaces 0 and 1 with the label
`airgrep Console`. The data interface is number 2, class 0xff, labelled
`airgrep Data`, with bulk OUT endpoint 0x03 and IN endpoint 0x83. Only the console
appears as a serial port. The VID/PID is `fa00:fa01`.

From `firmware/`, using Python 3 with `pyusb`, `pyserial` and libusb installed:

```sh
./tools/airgrep.py
./tools/airgrep.py hello
./tools/airgrep.py reboot
./tools/airgrep.py dfu
./tools/airgrep.py console
```

Without a command, the tool prints brief usage and found devices with their
`--serial` values, console paths and USB bulk interfaces. `console` launches `picocom`, exit with
Ctrl+A then Ctrl+X. Discovery uses USB
identity and interface numbers on macOS and Linux, without probing the console
with binary data. Use `--serial <mac>` when several devices are connected,
or `--port /dev/cu.usbmodem...` to select a console port explicitly. Binary commands
use PyUSB and claim only the vendor interface. Install Python dependencies with
`python3 -m pip install pyusb pyserial`. On macOS, install the native dependencies
with `brew install libusb picocom`.

`dfu` acknowledges the request, then enters ROM serial download mode for
`idf.py flash` or esptool. It does not expose a USB DFU class. The ROM port has
Espressif's identity and can have a different path from the application console.
After flashing, use `./tools/airgrep.py console` to rediscover the application port.
For the transition from dual-CDC firmware (`303a:4002`), `dfu` automatically uses
that device's second CDC port. This compatibility path is limited to bootloader
entry; other binary commands use the vendor interface.
The first flash from the previous USB Serial/JTAG firmware still uses its
existing port. If the application cannot run, BOOT plus RESET remains the
manual route to ROM download mode.

All firmware protocol commands are implemented, including full-State READ,
partial-byte WRITE from a complete State, Traffic batches of up to 32 targets and
GNSS injection. Ingestion flags are written through the ADSBee State and return to
READ on connection reset or timeout. The Python tool covers only HELLO, reboot,
bootloader and console access. Reset commands wait for USB endpoint completion
before restarting. A vendor control request clears the data decoder and software
queues, pending endpoint transfers must drain before reuse. The client drains stale bytes before
sending. Disconnect, full Traffic transfers and bootloader behavior still require
testing on the board and host USB driver.

The `s3-uart0` preset retains its physical UART console, and Unity presets retain
USB Serial/JTAG. Existing application build directories must select
`ESP_CONSOLE_NONE` and `ESP_CONSOLE_SECONDARY_NONE` when enabling TinyUSB. These
disable the built-in console drivers, the application installs its CDC console
before component startup. Bootloader and panic text are not routed through this
application console. Light sleep is prevented while USB is mounted, including
host suspend, because the S3 full-speed PHY cannot wake from light sleep.
