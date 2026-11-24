#!/usr/bin/env python3
"""Build and flash AIRGREP, then open its console, tracking one device across USB modes.
"""

import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from types import SimpleNamespace

import airgrep


FIRMWARE = Path(__file__).resolve().parents[1]
# Each profile with a pattern for the board its firmware reports, BOARD_NAME in components/airgrep/board.h.
PROFILES = {"s3": r"S3 Plus", "s3-debug": r"S3 Plus", "display": r"Seeed Display"}
FRAGMENTS = {"debug-uart": "sdkconfig.uart0", "hwtest": "sdkconfig.hwtest", "float": "sdkconfig.float"}
ROM_USB = (0x303A, 0x1001)
identity = airgrep.identity


def displays():
    variants = FIRMWARE / "components/display/variants"
    return sorted(path.name for path in variants.iterdir() if path.is_dir()) if variants.is_dir() else []


def idf_command():
    sdk = os.environ.get("IDF_PATH")
    script = str(Path(sdk) / "tools/idf.py") if sdk else shutil.which("idf.py")
    if not script or not Path(script).is_file():
        raise airgrep.ProtocolError("Activate ESP-IDF first, then run this tool from the activated shell")
    # Preserve IDF's launcher interpreter, which must match its CMake cache.
    return [script]


def dependencies():
    try:
        import usb.core
        import usb.util
        import usb.backend.libusb1
        from serial.tools import list_ports
    except ImportError:
        raise airgrep.ProtocolError("Install pyusb and pyserial in the active ESP-IDF Python environment") from None
    airgrep.require_picocom()
    return SimpleNamespace(core=usb.core, util=usb.util,
                           backend=airgrep.load_backend(usb.backend.libusb1), ports=list_ports.comports)


@contextmanager
def snapshot(usb):
    records = airgrep.usb_devices(usb.core, usb.backend)
    try:
        yield records, list(usb.ports())
    finally:
        for record in records:
            usb.util.dispose_resources(record.device)


def is_rom(port):
    return (port.vid, port.pid) == ROM_USB


def discover(usb, selector, profile):
    """Identify the first matching device, or None when nothing enumerates.

    A confirmed device is flashed without asking: an AIRGREP whose HELLO fits the profile,
    legacy AIRGREP firmware, or a ROM device named by --serial.
    """
    selected = identity(selector) if selector else None
    if selector and selected is None:
        raise airgrep.ProtocolError("--serial must be a factory MAC, with optional airgrep- prefix or colons")
    with snapshot(usb) as (records, ports):
        candidates = [(identity(record.serial_number), record) for record in records]
        candidates.extend((identity(port.serial_number), port) for port in ports if is_rom(port))
        if selector:
            candidates = [candidate for candidate in candidates if candidate[0] == selected]
        if not candidates:
            return None
        mac, record = candidates[0]
        if mac is None:
            raise airgrep.ProtocolError("The first device has no recognized factory MAC serial number")
        others = sorted({other for other, _ in candidates[1:] if other not in (mac, None)})
        if others:
            print(f"Using the first of {len(others) + 1} devices. Others: {', '.join(others)}. Select --serial")
        if record not in records:
            print(f"Device    ESP32-S3 {mac}\n  {'ROM':7} {record.device} (download mode)\n", flush=True)
            return SimpleNamespace(mac=mac, confirmed=bool(selector))
        airgrep.print_device(record, ports, airgrep.interface_numbers(ports))
        print(flush=True)
        if airgrep.is_legacy_cdc(record):
            return SimpleNamespace(mac=mac, confirmed=True)
        try:
            payload = airgrep.exchange(usb.core, usb.util, record, airgrep.COMMANDS["hello"])
        except (airgrep.ProtocolError, OSError) as error:
            print(f"No HELLO reply: {error}\n", flush=True)
            return SimpleNamespace(mac=mac, confirmed=False)
        airgrep.print_hello(payload, record.serial_number)
        print(flush=True)
        hello = airgrep.decode_hello(payload)
        if hello.board and not re.search(PROFILES[profile], hello.board):
            raise airgrep.ProtocolError(
                f"Device reports board {hello.board}, profile {profile} builds for {PROFILES[profile]}. "
                "Select the matching --profile. To flash regardless, hold BOOT while connecting for ROM download mode")
        return SimpleNamespace(mac=mac, confirmed=True)


def confirm(question):
    if not sys.stdin.isatty():
        print(f"{question} Not asked without a terminal.")
        return False
    return input(f"{question} [y/N] ").strip().lower() in ("y", "yes")


def enter_bootloader(usb, selected):
    with snapshot(usb) as (records, ports):
        rom = [port for port in ports if is_rom(port) and identity(port.serial_number) == selected]
        if len(rom) > 1:
            raise airgrep.ProtocolError("Multiple ROM ports report the selected factory MAC")
        if rom:
            return
        matches = [record for record in records if identity(record.serial_number) == selected]
        if len(matches) != 1:
            raise airgrep.ProtocolError("Selected AIRGREP device disappeared or has ambiguous USB identity")
        record = matches[0]
        print(f"Entering bootloader on {selected}.", flush=True)
        if airgrep.is_legacy_cdc(record):
            usb.util.dispose_resources(record.device)
            airgrep.legacy_dfu(record)
        else:
            airgrep.exchange(usb.core, usb.util, record, airgrep.COMMANDS["dfu"])


def wait_port(usb, selected, timeout, rom):
    deadline = time.monotonic() + timeout
    while True:
        ports = list(usb.ports())
        if rom:
            matches = [port.device for port in ports if is_rom(port)
                       and identity(port.serial_number) == selected]
        else:
            interfaces = airgrep.interface_numbers(ports)
            matches = [port.device for port in ports if airgrep.is_airgrep(port.product)
                       and identity(port.serial_number) == selected
                       and airgrep.port_role(interfaces.get(port.device)) == "console"]
        if len(matches) > 1:
            raise airgrep.ProtocolError("Multiple ports report the selected factory MAC")
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            # A reset over USB does not read the BOOT button again: a bootloader entered with it stays.
            if not rom and any(is_rom(port) and identity(port.serial_number) == selected for port in ports):
                raise airgrep.ProtocolError(f"Flashed. {selected} is still in its bootloader: "
                                            "press RESET, or unplug it and plug it in again")
            mode = "ROM download port" if rom else "application console"
            raise airgrep.ProtocolError(f"Timed out waiting for {selected} {mode}")
        if matches:
            return matches[0]
        time.sleep(min(0.2, remaining))


def variation(profile, fragments):
    """idf.py options that build the profile's preset plus named sdkconfig fragments in its own directory."""
    if not fragments:
        return []
    presets = json.loads((FIRMWARE / "CMakePresets.json").read_text())["configurePresets"]
    defaults = next((preset.get("cacheVariables", {}).get("SDKCONFIG_DEFAULTS")
                     for preset in presets if preset["name"] == profile), None)
    if not defaults:
        raise airgrep.ProtocolError(f"CMakePresets.json has no preset {profile} with SDKCONFIG_DEFAULTS")
    build = "build/" + "+".join([profile, *fragments])
    return ["-B", build, f"-DSDKCONFIG={build}/sdkconfig",
            "-DSDKCONFIG_DEFAULTS=" + ";".join([defaults, *fragments.values()])]


def run_idf(command, profile, *arguments):
    subprocess.run([*command, "--preset", profile, *arguments], cwd=FIRMWARE, check=True)


def build_flash(command, usb, profile, selector, timeout):
    target = discover(usb, selector, profile)
    if target is None:
        print("No matching AIRGREP or ESP32-S3 ROM device found. Building without flashing.", flush=True)
    print(f"{'Profile':20} {profile}\n", flush=True)
    run_idf(command, profile, "build")
    if target is None:
        # A device connected during the build was never identified before it. Always ask.
        target = discover(usb, selector, profile)
        if target is None:
            raise airgrep.ProtocolError(
                "Built, not flashed. Connect the device, or hold BOOT while connecting for ROM download mode, "
                "then run again")
        target.confirmed = False
    if not target.confirmed and not confirm(f"Flash {target.mac} with profile {profile}?"):
        print("Built, not flashed.")
        return
    selected = target.mac
    enter_bootloader(usb, selected)
    rom_port = wait_port(usb, selected, timeout, rom=True)
    run_idf(command, profile, "-p", rom_port, "flash")
    console_port = wait_port(usb, selected, timeout, rom=False)
    print(f"Console on {console_port}. Ctrl+A then Ctrl+X exits.", flush=True)
    airgrep.open_console(console_port)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--profile", choices=PROFILES, default="s3",
                        help="Application build profile, default s3")
    parser.add_argument("--debug-uart", action="store_true", help="Console on UART0 GPIO1/2")
    parser.add_argument("--hwtest", action="store_true", help="Hardware test mode")
    parser.add_argument("--float", action="store_true",
                        help="Conflict engine in single precision, default double")
    parser.add_argument("--display", choices=displays(), help="Display variant, default none")
    parser.add_argument("--serial", help="Factory MAC or USB serial, default first found device. "
                        "Without a device the image is built and not flashed")
    parser.add_argument("--timeout", type=float, default=15,
                        help="Seconds to wait for each USB mode change, default 15")
    args = parser.parse_args(argv)
    if not 0 < args.timeout < float("inf"):
        parser.error("--timeout must be finite and positive")
    try:
        fragments = {name: path for name, path in FRAGMENTS.items() if getattr(args, name.replace("-", "_"))}
        if args.display:
            variant = Path("components/display/variants") / args.display
            fragments[args.display] = str(variant / "sdkconfig")
            board_config = variant / "sdkconfig.s3"
            if args.profile in ("s3", "s3-debug") and (FIRMWARE / board_config).is_file():
                fragments[args.display] += ";" + str(board_config)
        command = [*idf_command(), *variation(args.profile, fragments)]
        usb = dependencies()
        build_flash(command, usb, args.profile, args.serial, args.timeout)
        return 0
    except (airgrep.ProtocolError, OSError) as error:
        print(f"build-flash: {error}", file=sys.stderr)
        return 1
    except subprocess.CalledProcessError as error:
        print(f"build-flash: IDF command failed with exit code {error.returncode}", file=sys.stderr)
        return error.returncode if error.returncode > 0 else 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
