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
| `s3-debug`       | as `s3`, with USB State access and data injection enabled      | development        |
| `display-test`   | as `display`                                                 | Unity test runner  |
| `s3-test`        | as `s3`                                                      | Unity test runner  |

Command line, from `firmware/`:

```sh
idf.py --preset s3 build
idf.py --preset s3 -p /dev/cu.usbmodem2101 flash        # select the ROM download port
idf.py --preset s3-test menuconfig                      # airgrep > tags to run at boot
```

For the complete build, flash and console workflow, activate ESP-IDF and run:

```sh
./tools/build-flash.py
./tools/build-flash.py --profile s3 --serial 68ee8f4d0c74
./tools/build-flash.py --profile s3-debug
./tools/build-flash.py --profile s3 --debug-uart
./tools/build-flash.py --profile display --hwtest --display seeed-172x320-touch
```

The profile is an application preset and defaults to `s3`; `s3-debug` and `display`
are also supported. Flags vary it without a preset of their own:

| Flag               | Effect                                                              |
|--------------------|---------------------------------------------------------------------|
| `--debug-uart`     | adds `sdkconfig.uart0`, console on UART0 GPIO1/2                    |
| `--hwtest`         | adds `sdkconfig.hwtest`, hardware test mode                         |
| `--display <name>` | adds `components/display/variants/<name>/sdkconfig`, LVGL and that UI |

A build with flags gets its own directory and `sdkconfig`, named after the profile
and its flags, such as `build/s3+debug-uart`. Without `--display` the firmware
contains no display code. With it the user interface is compiled, no panel driver
shows it yet. Display builds resolve their managed components into
`dependencies.display.lock`, so `dependencies.lock` is the same with and without
a display.

Without `--serial`, the first discovered device is selected and the others are
listed. The tool sends HELLO and prints firmware, board and chip. A board that
does not fit the profile stops the run before the build. The tool then builds,
requests bootloader entry, tracks the same factory MAC through ROM and
application enumeration, flashes, and opens the new console port in `picocom`,
as `airgrep.py console` does. Exit with Ctrl+A then Ctrl+X.

Flashing starts without a question only for an identified device: a HELLO reply
that fits the profile, legacy dual-CDC firmware, or a ROM download device named
by `--serial`. Otherwise the tool builds first and asks before flashing: for a
ROM download device found without `--serial`, a device that does not answer
HELLO, or one connected during the build. With no device at all it builds and
stops. Hold BOOT while connecting to reach ROM download mode on a board that no
longer enumerates.

## Simulator

`simulator/` builds each variant in `components/display/variants` as a macOS
application, an SDL window as the panel (`simulator/sdl_panel.cpp`). The mouse
acts as the finger. It needs CMake, Ninja and `brew install sdl2`, not the IDF.
The first configure downloads LVGL, pinned in `simulator/CMakeLists.txt`.

```sh
./tools/simulator.py --display waveshare-800x480-touch
./tools/simulator.py --display seeed-172x320-touch --zoom 4
```

The tool builds the variant into `simulator/build/` and runs it. `--zoom` takes
1, 2 or 4 and defaults to the largest that fits the screen. The View menu
switches it at runtime, Cmd+1, Cmd+2 and Cmd+4. Zoom counts physical screen
pixels: at 1 a panel pixel is one screen pixel, also on a Retina screen, at 2
and 4 a block of one colour. Touches are logged in panel coordinates. Closing the window
ends the process. The simulator feeds the display a fixed `display::Model`.

A variant is a directory with a `panel.hpp` for the size, a `screen.cpp` that
implements `display::variant::build` and `display::update`, and an `sdkconfig`
fragment that sets `CONFIG_AIRGREP_DISPLAY` to the directory name. The simulator
and `build-flash.py --display` pick it up by that name.

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

HELLO protocol 1 includes the same formatted Firmware, Board and Chip values as
the console, while retaining the separate firmware version field. Python
displays MAC and Device ID from USB discovery. The extended HELLO requires the
updated client, which still reads firmware without board and chip. `dfu` retains
its existing wire format. `--serial` accepts the factory MAC with or without
colons or the `airgrep-` prefix.

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

Normal builds expose HELLO, reboot and bootloader entry. READ, WRITE,
UPDATE_TRAFFIC and UPDATE_GNSS return UNKNOWN_COMMAND unless
`CONFIG_AIRGREP_USB_SERVICE_COMMANDS=y`. Their handlers and component registry
are excluded from normal application builds. The `s3-debug` profile inherits
`s3`, uses its own `build/s3-debug` directory and sdkconfig, and enables these
commands through `sdkconfig.usb-service`.

Service-enabled firmware supports full-State READ, partial-byte WRITE from a
complete State, Traffic batches of up to 32 targets and GNSS injection.
Ingestion flags are written through the ADSBee State and return to
READ on connection reset or timeout. The Python tool covers only HELLO, reboot,
bootloader and console access. Reset commands wait for USB endpoint completion
before restarting. A vendor control request clears the data decoder and software
queues, pending endpoint transfers must drain before reuse. The client drains stale bytes before
sending. Disconnect, full Traffic transfers and bootloader behavior still require
testing on the board and host USB driver.

A `--debug-uart` build retains its physical UART console, and Unity presets retain
USB Serial/JTAG. Existing application build directories must select
`ESP_CONSOLE_NONE` and `ESP_CONSOLE_SECONDARY_NONE` when enabling TinyUSB. These
disable the built-in console drivers, the application installs its CDC console
before component startup. Bootloader and panic text are not routed through this
application console. Light sleep is prevented while USB is mounted, including
host suspend, because the S3 full-speed PHY cannot wake from light sleep.
