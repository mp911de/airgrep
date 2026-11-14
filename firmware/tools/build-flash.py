#!/usr/bin/env python3
"""Build, flash and monitor AIRGREP, tracking one device across USB modes."""

import argparse
from contextlib import contextmanager
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace

import airgrep


FIRMWARE = Path(__file__).resolve().parents[1]
PROFILES = ("s3", "display", "s3-hwtest", "display-hwtest")
ROM_USB = (0x303A, 0x1001)


def identity(value):
    value = (value or "").lower()
    if value.startswith("airgrep-"):
        value = value[8:]
    if re.fullmatch(r"[0-9a-f]{12}", value):
        return value
    if re.fullmatch(r"(?:[0-9a-f]{2}:){5}[0-9a-f]{2}", value):
        return value.replace(":", "")
    return None


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


def choose_device(usb, selector):
    selected = identity(selector) if selector else None
    if selector and selected is None:
        raise airgrep.ProtocolError("--serial must be a factory MAC, with optional airgrep- prefix or colons")
    with snapshot(usb) as (records, ports):
        candidates = [record.serial_number for record in records]
        candidates.extend(port.serial_number for port in ports if is_rom(port))
        if selector:
            candidates = [candidate for candidate in candidates if identity(candidate) == selected]
        if not candidates:
            raise airgrep.ProtocolError("No matching AIRGREP application or ESP32-S3 ROM device found")
        selected = identity(candidates[0])
        if selected is None:
            raise airgrep.ProtocolError("The first device has no recognized factory MAC serial number")
        return selected


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
            mode = "ROM download port" if rom else "application console"
            raise airgrep.ProtocolError(f"Timed out waiting for {selected} {mode}")
        if matches:
            return matches[0]
        time.sleep(min(0.2, remaining))


def run_idf(command, profile, *arguments, env=None):
    subprocess.run([*command, "--preset", profile, *arguments], cwd=FIRMWARE, check=True, env=env)


def monitor(command, profile, port):
    # D1 asserts CDC DTR without resetting the application or dropping back into ROM.
    with tempfile.TemporaryDirectory(prefix="airgrep-monitor-") as directory:
        config = Path(directory) / "monitor.cfg"
        config.write_text("[esp-idf-monitor]\ncustom_hard_reset_sequence = D1\n")
        env = os.environ.copy()
        env["ESP_IDF_MONITOR_CFGFILE"] = str(config)
        run_idf(command, profile, "-p", port, "monitor", env=env)


def build_flash_monitor(command, usb, profile, selector, timeout):
    selected = choose_device(usb, selector)
    print(f"AIRGREP {selected}, profile {profile}.", flush=True)
    run_idf(command, profile, "build")
    enter_bootloader(usb, selected)
    rom_port = wait_port(usb, selected, timeout, rom=True)
    run_idf(command, profile, "-p", rom_port, "flash")
    console_port = wait_port(usb, selected, timeout, rom=False)
    print(f"Monitoring {console_port}. Ctrl+] exits.", flush=True)
    monitor(command, profile, console_port)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=PROFILES, default="s3",
                        help="Application build profile, default s3")
    parser.add_argument("--serial", help="Factory MAC or USB serial, default first found device")
    parser.add_argument("--timeout", type=float, default=15,
                        help="Seconds to wait for each USB mode change, default 15")
    args = parser.parse_args(argv)
    if not 0 < args.timeout < float("inf"):
        parser.error("--timeout must be finite and positive")
    try:
        command = idf_command()
        usb = dependencies()
        build_flash_monitor(command, usb, args.profile, args.serial, args.timeout)
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
