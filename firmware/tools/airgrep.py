#!/usr/bin/env python3
"""AIRGREP USB control via PyUSB/libusb, console via pyserial and picocom."""

import argparse
import binascii
import math
import os
from pathlib import Path
import plistlib
import re
import shutil
import struct
import subprocess
import sys
import time
from types import SimpleNamespace


VERSION = 1
TIMEOUT = 3.0
COMMANDS = {"hello": 0x01, "reboot": 0x06, "dfu": 0x07}
STATUS = 0x80
STATUS_NAMES = (
    "OK", "CRC_MISMATCH", "VERSION_MISMATCH", "UNKNOWN_COMMAND",
    "UNKNOWN_COMPONENT", "INVALID_LENGTH", "INVALID_RANGE", "INTERNAL_ERROR",
)
HELLO = struct.Struct("<Q32s6IH4B2x")
HEALTH_NAMES = (
    "expander", "charger", "gauge", "usb_c", "barometer", "adsbee",
    "gnss", "battery", "memory", "cpu",
)


class ProtocolError(Exception):
    pass


def crc(frame):
    return binascii.crc_hqx(frame[:4] + frame[6:], 0xFFFF)


def request(command):
    frame = bytearray(struct.pack("<BHBH", VERSION, 6, command, 0))
    struct.pack_into("<H", frame, 4, crc(frame))
    return bytes(frame)


def decode_response(frame, command):
    if len(frame) < 7 or struct.unpack_from("<H", frame, 1)[0] != len(frame):
        raise ProtocolError("Invalid response length")
    if crc(frame) != struct.unpack_from("<H", frame, 4)[0]:
        raise ProtocolError("Response CRC mismatch")
    version, response, status = frame[0], frame[3], frame[6]
    if response == STATUS:
        if len(frame) != 7:
            raise ProtocolError("STATUS response contains unexpected payload")
        if status:
            name = STATUS_NAMES[status] if status < len(STATUS_NAMES) else f"status {status}"
            raise ProtocolError(f"Device replied {name}, device protocol {version}, client {VERSION}")
    elif response != command or status:
        raise ProtocolError("Unexpected response command or status")
    if version != VERSION:
        raise ProtocolError(f"Protocol version mismatch: device {version}, client {VERSION}")
    if command == COMMANDS["hello"]:
        if response != command or len(frame) != 7 + HELLO.size:
            raise ProtocolError("HELLO response is missing its 72-byte payload")
    elif response != STATUS:
        raise ProtocolError("Expected STATUS acknowledgement")
    return frame[7:]


def read_response(port, command, deadline):
    frame = bytearray()
    expected = 7
    while len(frame) < expected:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise ProtocolError("Response timed out after 3 seconds, command outcome unknown")
        port.timeout = remaining
        frame.extend(port.read(expected - len(frame)))
        if time.monotonic() >= deadline:
            raise ProtocolError("Response timed out after 3 seconds, command outcome unknown")
        if len(frame) >= 3:
            expected = struct.unpack_from("<H", frame, 1)[0]
            if expected < 7:
                raise ProtocolError("Response frame length is smaller than its header")
    return decode_response(frame, command)


class UsbChannel:
    def __init__(self, device, vendor, timeout_error):
        self.device = device
        self.interface, self.input, self.output = vendor
        self.timeout_error = timeout_error
        self.timeout = TIMEOUT
        self.buffer = bytearray()

    def reset(self):
        self.buffer.clear()
        self.device.ctrl_transfer(0x41, 1, 0, self.interface, b"", timeout=3000)

    def read(self, size):
        deadline = time.monotonic() + self.timeout
        while not self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return b""
            try:
                # USB packets cannot be split by a short host read. Buffer complete packets.
                self.buffer.extend(self.device.read(
                    self.input.bEndpointAddress, self.input.wMaxPacketSize,
                    timeout=max(1, math.ceil(remaining * 1000))))
            except self.timeout_error:
                return b""
        chunk = bytes(self.buffer[:size])
        del self.buffer[:size]
        return chunk

    def write(self, frame):
        return self.device.write(self.output.bEndpointAddress, frame, timeout=3000)


def drain_stale(port):
    # Session reset cannot cancel in-flight USB transfers. Drain before the next request.
    deadline = time.monotonic() + TIMEOUT
    while True:
        remaining = deadline - time.monotonic()
        if remaining < 0.1:
            raise ProtocolError("Data port did not become quiet after connection reset")
        port.timeout = 0.1
        if not port.read(1024):
            return


def exchange(core, util, record, command):
    if record.vendor is None:
        raise ProtocolError(f"Vendor bulk data interface unavailable: {record.error}. Upgrade the firmware")
    device = record.device
    channel = UsbChannel(device, record.vendor, core.USBTimeoutError)
    claimed = False
    try:
        util.claim_interface(device, channel.interface)
        claimed = True
        channel.reset()
        drain_stale(channel)
        frame = request(command)
        if channel.write(frame) != len(frame):
            raise ProtocolError("Incomplete request write, command outcome unknown")
        payload = read_response(channel, command, time.monotonic() + TIMEOUT)
        if channel.buffer:
            raise ProtocolError("Unexpected bytes after the response frame")
        return payload
    except (ProtocolError, OSError, KeyboardInterrupt):
        if claimed:
            try:
                channel.reset()
            except OSError:
                pass
        raise
    finally:
        if claimed:
            try:
                util.release_interface(device, channel.interface)
            except OSError:
                pass
        util.dispose_resources(device)


def vendor_interface(configuration):
    matches = []
    for interface in configuration:
        if interface.bInterfaceClass != 0xFF or interface.bAlternateSetting != 0:
            continue
        bulk = [ep for ep in interface if ep.bmAttributes & 3 == 2]
        inputs = [ep for ep in bulk if ep.bEndpointAddress & 0x80]
        outputs = [ep for ep in bulk if not ep.bEndpointAddress & 0x80]
        if len(inputs) == len(outputs) == 1:
            matches.append((interface.bInterfaceNumber, inputs[0], outputs[0]))
    if len(matches) != 1:
        raise ProtocolError("Expected one vendor interface with bulk IN and OUT endpoints")
    return matches[0]


def is_airgrep(product):
    return re.fullmatch(r"airgrep(?:-[0-9a-fA-F]{12})?", product or "") is not None


def load_backend(libusb1):
    backend = libusb1.get_backend()
    if backend is None and sys.platform == "darwin":
        for path in ("/opt/homebrew/lib/libusb-1.0.dylib", "/usr/local/lib/libusb-1.0.dylib"):
            if Path(path).exists():
                backend = libusb1.get_backend(find_library=lambda name: path)
                if backend is not None:
                    break
    if backend is None:
        raise ProtocolError("libusb is required. Install it with brew install libusb or your package manager")
    return backend


def usb_devices(core, backend=None):
    records = []
    try:
        devices = core.find(find_all=True, backend=backend)
        for device in devices:
            try:
                product = device.product
            except (OSError, ValueError):
                continue
            if not is_airgrep(product):
                continue
            try:
                serial_number = device.serial_number
            except (OSError, ValueError):
                serial_number = None
            vendor, error = None, None
            try:
                vendor = vendor_interface(device.get_active_configuration())
            except (ProtocolError, OSError, ValueError) as exception:
                error = str(exception)
                if device.idVendor == 0x303A and device.idProduct == 0x4002:
                    error = "legacy dual-CDC firmware, USB PID 0x4002"
            records.append(SimpleNamespace(device=device, product=product,
                                           serial_number=serial_number, vendor=vendor, error=error))
    except core.NoBackendError:
        raise ProtocolError("libusb is required. Install it with brew install libusb or your package manager") from None
    return records


def select_usb(records, serial_number=None):
    matches = [record for record in records if serial_number is None or record.serial_number == serial_number]
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        choices = ", ".join(record.serial_number or "serial unavailable" for record in matches)
        raise ProtocolError(f"Multiple AIRGREP devices: {choices}. Select --serial")
    raise ProtocolError("No AIRGREP USB device found")


def is_legacy_cdc(record):
    return (record.vendor is None and is_airgrep(record.product)
            and record.device.idVendor == 0x303A and record.device.idProduct == 0x4002)


def legacy_data_port(record, ports, interfaces):
    if not is_legacy_cdc(record) or not record.serial_number:
        raise ProtocolError("Cannot identify the legacy AIRGREP data port without its USB serial number")
    matches = [port.device for port in airgrep_ports(ports, record.serial_number)
               if port.vid == 0x303A and port.pid == 0x4002
               and interfaces.get(port.device) in (2, 3)]
    if len(matches) != 1:
        raise ProtocolError("Cannot uniquely identify the selected AIRGREP legacy CDC data port")
    return matches[0]


def legacy_dfu(record):
    """Enter the bootloader on firmware that predates the vendor bulk interface."""
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        raise ProtocolError("Legacy CDC upgrade requires pyserial. Install it with python3 -m pip install pyserial") from None
    ports = list(list_ports.comports())
    device = legacy_data_port(record, ports, interface_numbers(ports))
    port = serial.Serial(port=None, baudrate=115200, timeout=TIMEOUT, write_timeout=TIMEOUT)
    port.dtr = False
    port.rts = False
    port.port = device

    def reset():
        port.dtr = False
        port.reset_input_buffer()
        port.reset_output_buffer()

    try:
        port.open()
        reset()
        port.dtr = True
        drain_stale(port)
        frame = request(COMMANDS["dfu"])
        if port.write(frame) != len(frame):
            raise ProtocolError("Incomplete bootloader request write, command outcome unknown")
        return read_response(port, COMMANDS["dfu"], time.monotonic() + TIMEOUT)
    except (ProtocolError, OSError, KeyboardInterrupt):
        if port.is_open:
            try:
                reset()
            except OSError:
                pass
        raise
    finally:
        try:
            port.close()
        except OSError:
            pass


def macos_interfaces(roots):
    """Associate serial nodes with their nearest USB interface ancestor."""
    result = {}

    def visit(node, interface=None):
        interface = node.get("bInterfaceNumber", interface)
        device = node.get("IOCalloutDevice")
        if device is not None and isinstance(interface, int):
            result[device] = interface
        for child in node.get("IORegistryEntryChildren", []):
            visit(child, interface)

    for root in roots:
        visit(root)
    return result


def interface_numbers(ports):
    if sys.platform == "darwin":
        try:
            output = subprocess.check_output(
                ["/usr/sbin/ioreg", "-a", "-r", "-c", "IOUSBHostDevice", "-l"],
                stderr=subprocess.DEVNULL, timeout=TIMEOUT,
            )
            return macos_interfaces(plistlib.loads(output))
        except (OSError, subprocess.SubprocessError, ValueError, plistlib.InvalidFileException):
            return {}
    result = {}
    if sys.platform.startswith("linux"):
        for port in ports:
            path = (Path("/sys/class/tty") / Path(port.device).name / "device").resolve()
            for parent in (path, *path.parents):
                try:
                    result[port.device] = int((parent / "bInterfaceNumber").read_text().strip(), 16)
                    break
                except (OSError, ValueError):
                    continue
    return result


def airgrep_ports(ports, serial_number=None):
    return [port for port in ports if is_airgrep(port.product)
            and (serial_number is None or port.serial_number == serial_number)]


def port_role(interface):
    # Hosts associate the serial node with either the CDC control or data interface.
    return "console" if interface in (0, 1) else "unknown role"


def print_devices(records, ports, interfaces, serial_number=None):
    records = [record for record in records if serial_number is None or record.serial_number == serial_number]
    if not records:
        print("No AIRGREP USB devices found.")
    for record in sorted(records, key=lambda r: r.serial_number or r.product):
        selector = f"--serial {record.serial_number}" if record.serial_number else "serial unavailable"
        print(f"{record.product}  {selector}")
        console_ports = [port for port in airgrep_ports(ports) if record.serial_number
                         and port.serial_number == record.serial_number]
        if not console_ports:
            print("  console: path unavailable")
        for port in sorted(console_ports, key=lambda p: p.device):
            interface = interfaces.get(port.device)
            role = port_role(interface)
            detail = f"interface {interface}" if interface is not None else "interface unavailable"
            print(f"  {role}: {port.device} ({detail})")
        if record.vendor:
            interface, input_ep, output_ep = record.vendor
            print(f"  data: vendor interface {interface}, bulk OUT 0x{output_ep.bEndpointAddress:02x}, IN 0x{input_ep.bEndpointAddress:02x}")
        else:
            print(f"  data: vendor interface unavailable ({record.error})")


def select_port(ports, interfaces, serial_number=None):
    devices = airgrep_ports(ports, serial_number)
    matches = [port for port in devices if port_role(interfaces.get(port.device)) == "console"]
    if len(matches) == 1:
        return matches[0].device
    if len(matches) > 1:
        choices = ", ".join(f"{p.device} (serial {p.serial_number})" for p in matches)
        raise ProtocolError(f"Multiple AIRGREP devices: {choices}. Select --serial or --port")
    if devices:
        choices = ", ".join(port.device for port in devices)
        raise ProtocolError(f"Cannot identify CDC console port among {choices}. Use --port")
    raise ProtocolError("No AIRGREP USB device found. Connect the device or select --port")


def print_hello(payload):
    uptime, firmware, *values = HELLO.unpack(payload)
    internal = values[:3]
    psram = values[3:6]
    health, mode, core0, core1, overall = values[6:]
    firmware = firmware.split(b"\0", 1)[0].decode("utf-8", errors="replace")
    modes = ("UNKNOWN", "ON", "CHARGER", "OFF")
    mode_name = modes[mode] if mode < len(modes) else "UNKNOWN"
    percent = lambda n: "UNAVAILABLE" if n == 0xFF else f"{n} %"
    failures = [name for bit, name in enumerate(HEALTH_NAMES) if health & (1 << bit)]
    print(f"{'Firmware':20} {firmware}, protocol {VERSION}")
    print(f"{'Uptime':20} {uptime // 86400}d {uptime // 3600 % 24:02}:{uptime // 60 % 60:02}:{uptime % 60:02}")
    print(f"{'Mode':20} {mode_name}")
    color = "\033[38;5;208m" if health else "\033[32m"
    status = f"ALERT (0x{health:X}{' ' + ', '.join(failures) if failures else ''})" if health else "OK (0x0)"
    print(f"{'Health':20} {color}{status}\033[0m")
    print(f"{'Load':20} core0 {percent(core0)}, core1 {percent(core1)}, overall {percent(overall)}")
    for label, memory in (("Internal RAM", internal), ("PSRAM", psram)):
        print(f"{label:20} {memory[0] // 1024} KB free, {memory[1] // 1024} KB minimum, {memory[2] // 1024} KB total")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", nargs="?", choices=(*COMMANDS, "console"),
                        help="Command to execute, or omit to list connected devices")
    parser.add_argument("--port", help="Explicit console serial device, only for console")
    parser.add_argument("--serial", help="Select an AIRGREP device by its USB serial number")
    args = parser.parse_args(argv)
    if args.port and args.serial:
        parser.error("Use either --port or --serial")
    if args.port and args.command != "console":
        parser.error("--port is only supported for console")
    if args.command is None:
        parser.print_usage()
    try:
        if args.command == "console" and not shutil.which("picocom"):
            raise ProtocolError("picocom is required. Install it with brew install picocom or your package manager")
        if args.command in (None, "console") and not args.port:
            try:
                from serial.tools import list_ports
            except ImportError:
                raise ProtocolError("pyserial is required. Install it in your Python environment with python3 -m pip install pyserial") from None
            ports = list(list_ports.comports())
            interfaces = interface_numbers(ports)
        if args.command == "console":
            device = args.port or select_port(ports, interfaces, args.serial)
            os.execvp("picocom", ["picocom", "--baud", "115200", device])
        try:
            import usb.core
            import usb.util
            import usb.backend.libusb1
        except ImportError:
            raise ProtocolError("PyUSB is required. Install it in your Python environment with python3 -m pip install pyusb") from None
        records = usb_devices(usb.core, load_backend(usb.backend.libusb1))
        if args.command is None:
            try:
                print_devices(records, ports, interfaces, args.serial)
            finally:
                for record in records:
                    usb.util.dispose_resources(record.device)
            return 0
        record = select_usb(records, args.serial)
        if args.command == "dfu" and is_legacy_cdc(record):
            usb.util.dispose_resources(record.device)
            payload = legacy_dfu(record)
        else:
            payload = exchange(usb.core, usb.util, record, COMMANDS[args.command])
        if args.command == "hello":
            print_hello(payload)
        elif args.command == "dfu":
            print("Bootloader entry acknowledged. The ROM download port may have a different path.")
        else:
            print("Reboot acknowledged.")
        return 0
    except (ProtocolError, OSError) as error:
        print(f"airgrep: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())
