#!/usr/bin/env python3
"""AIRGREP USB control via PyUSB/libusb, console via pyserial and picocom.

Without a command the tool lists the devices it finds, each with its --serial value, console path and
bulk interface. It finds them by USB identity and interface number, and sends the console no binary
data. --serial takes the factory MAC with or without colons or the airgrep- prefix. Binary commands
claim only the vendor interface. The protocol is in docs/usb-data-protocol.md.

dfu makes the device enter the ROM serial download mode for idf.py flash or esptool, it is no USB DFU
class. The ROM port has Espressif's identity and may have another path than the console, so find the
console again with the console command after flashing. For the old dual-CDC firmware dfu uses that
device's second CDC port, no other command works there. An application that does not run leaves BOOT
plus RESET as the way into ROM download.
"""

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
TIMEOUT_MS = int(TIMEOUT * 1000)
APP_USB = (0xFA00, 0xFA01)
LEGACY_USB = (0x303A, 0x4002)
conversation_ns = 0
COMMANDS = {"hello": 0x01, "read": 0x02, "write": 0x03, "reboot": 0x06, "dfu": 0x07}
STATUS = 0x80
# A component's ID and the size of its State in protocol version 1, see docs/usb-data-protocol.md.
COMPONENTS = {
    "adsbee": (0x01, 240), "traffic": (0x02, 61440), "barometer": (0x03, 88), "battery": (0x04, 72),
    "charger": (0x05, 288), "expander": (0x06, 64), "usb_c": (0x07, 72), "wifi": (0x08, 144),
    "adsbee_clock": (0x09, 56),
}
WRITE = struct.Struct("<BHH")
GREEN = "\033[32m"
ORANGE = "\033[38;5;208m"
BLUE = "\033[38;2;146;179;244m"
RESET = "\033[0m"
STATUS_NAMES = (
    "OK", "CRC_MISMATCH", "VERSION_MISMATCH", "UNKNOWN_COMMAND",
    "UNKNOWN_COMPONENT", "INVALID_LENGTH", "INVALID_RANGE", "INTERNAL_ERROR",
)
HELLO = struct.Struct("<Q32s32s6IH5Bx128s128s128s")
# Whether READ and WRITE exist. Firmware that predates the field reports the first.
SERVICE_NAMES = ("UNKNOWN, firmware predates the field", "UNAVAILABLE", "AVAILABLE")
HEALTH_NAMES = (
    "i2c", "adsbee", "gnss", "antenna_short", "charger_overvoltage", "charger_overheated",
    "battery_overheated", "antenna_open", "backup_battery", "barometer", "gauge", "usb_c",
    "battery_low", "battery_cold", "memory", "cpu",
)


class ProtocolError(Exception):
    pass


def crc(frame):
    return binascii.crc_hqx(frame[:4] + frame[6:], 0xFFFF)


def request(command, payload=b""):
    frame = bytearray(struct.pack("<BHBH", VERSION, 6 + len(payload), command, 0) + payload)
    struct.pack_into("<H", frame, 4, crc(frame))
    return bytes(frame)


def read_payload(component):
    return bytes([COMPONENTS[component][0]])


def write_payload(component, offset, length, state):
    """WRITE carries the complete State. The device copies length bytes of it at offset."""
    return WRITE.pack(COMPONENTS[component][0], offset, length) + state


def patched(component, state, offset, data):
    """The State with data at offset, refused when the State is not the one this client knows."""
    size = COMPONENTS[component][1]
    if len(state) != size:
        raise ProtocolError(f"{component} State has {len(state)} bytes, this client expects {size}. Not writing")
    if offset + len(data) > size:
        raise ProtocolError(f"{len(data)} bytes at offset {offset} lie outside the {size} bytes of {component}")
    return state[:offset] + data + state[offset + len(data):]


def hexdump(data, start=0):
    """Sixteen bytes a line behind their offset, in decimal and hex, so a dump can be pasted and compared."""
    lines = []
    for at in range(0, len(data), 16):
        chunk = data[at:at + 16]
        lines.append(f"{start + at:5d}  {start + at:04x}  {chunk.hex(' ')}")
    return "\n".join(lines)


def decode_response(frame, command):
    if len(frame) < 7 or struct.unpack_from("<H", frame, 1)[0] != len(frame):
        raise ProtocolError("Invalid response length")
    if crc(frame) != struct.unpack_from("<H", frame, 4)[0]:
        raise ProtocolError("Response CRC mismatch")
    device_version, response, status = frame[0], frame[3], frame[6]
    if response == STATUS:
        if len(frame) != 7:
            raise ProtocolError("STATUS response contains unexpected payload")
        if status:
            name = STATUS_NAMES[status] if status < len(STATUS_NAMES) else f"status {status}"
            raise ProtocolError(f"Device replied {name}, device protocol {device_version}, client {VERSION}")
    elif response != command or status:
        raise ProtocolError("Unexpected response command or status")
    if device_version != VERSION:
        raise ProtocolError(f"Protocol version mismatch: device {device_version}, client {VERSION}")
    if command == COMMANDS["hello"]:
        if response != command or len(frame) - 7 != HELLO.size:
            raise ProtocolError(f"HELLO response is missing its {HELLO.size}-byte payload")
    elif command == COMMANDS["read"]:
        if response != command or len(frame) == 7:
            raise ProtocolError("READ response is missing its State")
    elif response != STATUS:
        raise ProtocolError("Expected STATUS acknowledgement")
    return frame[7:]


def read_response(port, command, deadline, started_ns=None):
    global conversation_ns
    timed_out = ProtocolError(f"Response timed out after {TIMEOUT:g} seconds, command outcome unknown")
    frame = bytearray()
    expected = 7
    while len(frame) < expected:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise timed_out
        port.timeout = remaining
        frame.extend(port.read(expected - len(frame)))
        if time.monotonic() >= deadline:
            raise timed_out
        if len(frame) >= 3:
            expected = struct.unpack_from("<H", frame, 1)[0]
            if expected < 7:
                raise ProtocolError("Response frame length is smaller than its header")
    if started_ns is not None:
        conversation_ns += time.perf_counter_ns() - started_ns
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
        self.device.ctrl_transfer(0x41, 1, 0, self.interface, b"", timeout=TIMEOUT_MS)

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
        return self.device.write(self.output.bEndpointAddress, frame, timeout=TIMEOUT_MS)


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


def converse(port, command, payload=b""):
    frame = request(command, payload)
    started_ns = time.perf_counter_ns()
    if port.write(frame) != len(frame):
        raise ProtocolError("Incomplete request write, command outcome unknown")
    return read_response(port, command, time.monotonic() + TIMEOUT, started_ns)


def transact(port, command, reset, payload=b""):
    reset()
    drain_stale(port)
    return converse(port, command, payload)


def session(core, util, record, work):
    """Runs work(send) in one data session. The device restores session state on the reset that opens it."""
    if record.vendor is None:
        raise ProtocolError(f"Vendor bulk data interface unavailable: {record.error}. Upgrade the firmware")
    device = record.device
    channel = UsbChannel(device, record.vendor, core.USBTimeoutError)

    def send(command, payload=b""):
        payload = converse(channel, command, payload)
        if channel.buffer:
            raise ProtocolError("Unexpected bytes after the response frame")
        return payload

    claimed = False
    try:
        util.claim_interface(device, channel.interface)
        claimed = True
        channel.reset()
        drain_stale(channel)
        return work(send)
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


def exchange(core, util, record, command, payload=b""):
    return session(core, util, record, lambda send: send(command, payload))


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


def identity(value):
    """Factory MAC as twelve lowercase hex digits, from a serial number, Device ID or colon form."""
    value = (value or "").lower().removeprefix("airgrep-")
    if re.fullmatch(r"(?:[0-9a-f]{2}:){5}[0-9a-f]{2}", value):
        value = value.replace(":", "")
    return value if re.fullmatch(r"[0-9a-f]{12}", value) else None


def selects(selector, serial_number):
    if selector is None or selector == serial_number:
        return True
    return identity(selector) is not None and identity(selector) == identity(serial_number)


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
            # Reading strings opens the device. Leave unrelated USB devices untouched.
            if (device.idVendor, device.idProduct) not in (APP_USB, LEGACY_USB):
                continue
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
                if (device.idVendor, device.idProduct) == LEGACY_USB:
                    error = "legacy dual-CDC firmware, USB PID 0x4002"
            records.append(SimpleNamespace(device=device, product=product,
                                           serial_number=serial_number, vendor=vendor, error=error))
    except core.NoBackendError:
        raise ProtocolError("libusb is required. Install it with brew install libusb or your package manager") from None
    return records


def select_usb(records, serial_number=None):
    matches = [record for record in records if selects(serial_number, record.serial_number)]
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        choices = ", ".join(record.serial_number or "serial unavailable" for record in matches)
        raise ProtocolError(f"Multiple AIRGREP devices: {choices}. Select --serial")
    raise ProtocolError("No AIRGREP USB device found")


def is_legacy_cdc(record):
    return (record.vendor is None and is_airgrep(record.product)
            and (record.device.idVendor, record.device.idProduct) == LEGACY_USB)


def legacy_data_port(record, ports, interfaces):
    if not is_legacy_cdc(record) or not record.serial_number:
        raise ProtocolError("Cannot identify the legacy AIRGREP data port without its USB serial number")
    matches = [port.device for port in airgrep_ports(ports, record.serial_number)
               if (port.vid, port.pid) == LEGACY_USB
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

    def start():
        reset()
        port.dtr = True

    try:
        port.open()
        return transact(port, COMMANDS["dfu"], start)
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
            and selects(serial_number, port.serial_number)]


def port_role(interface):
    # Hosts associate the serial node with either the CDC control or data interface.
    return "console" if interface in (0, 1) else "unknown role"


def print_devices(records, ports, interfaces, serial_number=None):
    records = [record for record in records if selects(serial_number, record.serial_number)]
    if not records:
        print("No AIRGREP USB devices found.")
    for record in sorted(records, key=lambda r: r.serial_number or r.product):
        selector = f"--serial {record.serial_number}" if record.serial_number else "Serial unavailable"
        print_device(record, ports, interfaces, selector)


def print_device(record, ports, interfaces, selector=None):
    print(f"Device    {record.product}" + (f"  {selector}" if selector else ""))
    console_ports = [port for port in airgrep_ports(ports) if record.serial_number
                     and port.serial_number == record.serial_number]
    if not console_ports:
        print("  Console Path unavailable")
    for port in sorted(console_ports, key=lambda p: p.device):
        interface = interfaces.get(port.device)
        role = port_role(interface).capitalize()
        detail = f"Interface {interface}" if interface is not None else "Interface unavailable"
        print(f"  {role:7} {port.device} ({detail})")
    if record.vendor:
        interface, input_ep, output_ep = record.vendor
        print(f"  {'Data':7} Vendor interface {interface}, bulk OUT 0x{output_ep.bEndpointAddress:02x}, IN 0x{input_ep.bEndpointAddress:02x}")
    else:
        print(f"  {'Data':7} Vendor interface unavailable ({record.error})")


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


def decode_hello(payload):
    uptime, _firmware_version, variant, *values = HELLO.unpack(bytes(payload))
    variant, firmware, board, chip = (value.split(b"\0", 1)[0].decode("utf-8", errors="replace")
                                      for value in (variant, *values[12:15]))
    health, mode, core0, core1, overall, service = values[6:12]
    return SimpleNamespace(uptime=uptime, internal=values[:3], psram=values[3:6], health=health,
                           mode=mode, load=(core0, core1, overall), service=service, firmware=firmware,
                           variant=variant, board=board, chip=chip)


def require_picocom():
    if not shutil.which("picocom"):
        raise ProtocolError("picocom is required. Install it with brew install picocom or your package manager")


def open_console(device):
    """Replace this process with picocom. Ctrl+A then Ctrl+X exits."""
    os.execvp("picocom", ["picocom", "--baud", "115200", device])


def print_hello(payload, serial_number=None):
    hello = decode_hello(payload)
    uptime, health, mode = hello.uptime, hello.health, hello.mode
    core0, core1, overall = hello.load
    modes = ("UNKNOWN", "ON", "CHARGER", "OFF")
    mode_name = modes[mode] if mode < len(modes) else "UNKNOWN"
    percent = lambda n: "UNAVAILABLE" if n == 0xFF else f"{n} %"
    failures = [name for bit, name in enumerate(HEALTH_NAMES) if health & (1 << bit)]
    print(f"{'Firmware':20} {hello.firmware}")
    print(f"{'Variant':20} {hello.variant}")
    print(f"{'Board':20} {hello.board}")
    print(f"{'Chip':20} {hello.chip}")
    print(f"{'Protocol':20} {VERSION}")
    mac = identity(serial_number)
    if mac:
        print(f"{'MAC':20} {':'.join(mac[i:i + 2].upper() for i in range(0, 12, 2))}")
        print(f"{'Device ID':20} airgrep-{mac}")
    print(f"{'Uptime':20} {uptime // 86400}d {uptime // 3600 % 24:02}:{uptime // 60 % 60:02}:{uptime % 60:02}")
    color = {"ON": GREEN, "CHARGER": BLUE, "UNKNOWN": ORANGE}.get(mode_name, "")
    print(f"{'Mode':20} {color}{mode_name}{RESET if color else ''}")
    color = ORANGE if health else GREEN
    status = f"ALERT (0x{health:X}{' ' + ', '.join(failures) if failures else ''})" if health else "OK (0x0)"
    print(f"{'Health':20} {color}{status}{RESET}")
    service = SERVICE_NAMES[hello.service] if hello.service < len(SERVICE_NAMES) else SERVICE_NAMES[0]
    print(f"{'Service commands':20} {service}")
    print(f"{'Load':20} core0 {percent(core0)}, core1 {percent(core1)}, overall {percent(overall)}")
    for label, memory in (("Internal RAM", hello.internal), ("PSRAM", hello.psram)):
        print(f"{label:20} {memory[0] // 1024} KB free, {memory[1] // 1024} KB minimum, {memory[2] // 1024} KB total")


def read_state(send, component):
    """The State as hex, and a note when it is not as long as this client expects."""
    state = send(COMMANDS["read"], read_payload(component))
    size = COMPONENTS[component][1]
    note = "" if len(state) == size else f", this client expects {size}"
    return f"{component}: {len(state)} bytes{note}\n{hexdump(state)}"


def write_state(send, component, offset, data):
    """Replaces bytes of a State and shows them as read before and after. A poll may replace them again."""
    before = send(COMMANDS["read"], read_payload(component))
    state = patched(component, before, offset, data)
    send(COMMANDS["write"], write_payload(component, offset, len(data), state))
    after = send(COMMANDS["read"], read_payload(component))
    end = offset + len(data)
    return (f"{component}: wrote {len(data)} bytes at offset {offset}\n"
            f"before\n{hexdump(before[offset:end], offset)}\nafter\n{hexdump(after[offset:end], offset)}")


def service_arguments(command, arguments):
    """What a service command does with a way to send, or None for another command. ValueError names a misuse."""
    if command not in ("read", "write"):
        if arguments:
            raise ValueError(f"{command or 'listing devices'} takes no arguments")
        return None
    if not arguments or arguments[0] not in COMPONENTS:
        raise ValueError(f"{command} needs a component: {', '.join(COMPONENTS)}")
    component = arguments[0]
    if command == "read":
        if len(arguments) != 1:
            raise ValueError("read takes a component only")
        return lambda send: read_state(send, component)
    if len(arguments) < 3:
        raise ValueError("write needs a component, an offset and the bytes as hex")
    offset, data = int(arguments[1], 0), bytes.fromhex("".join(arguments[2:]))
    if not data:
        raise ValueError("write needs at least one byte")
    return lambda send: write_state(send, component, offset, data)


def main(argv=None):
    global conversation_ns
    conversation_ns = 0
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", nargs="?", choices=(*COMMANDS, "console"),
                        help="Command to execute, or omit to list connected devices")
    parser.add_argument("arguments", nargs="*", metavar="ARGUMENT",
                        help="read COMPONENT prints a State as hex. write COMPONENT OFFSET HEX replaces bytes of it. "
                             f"Components: {', '.join(COMPONENTS)}. Both need firmware with service commands")
    parser.add_argument("--port", help="Explicit console serial device, only for console")
    parser.add_argument("--serial", help="Select an AIRGREP device by its USB serial number")
    args = parser.parse_args(argv)
    try:
        service = service_arguments(args.command, args.arguments)
    except ValueError as error:
        parser.error(str(error))
    if args.port and args.serial:
        parser.error("Use either --port or --serial")
    if args.port and args.command != "console":
        parser.error("--port is only supported for console")
    if args.command is None:
        parser.print_usage()
    try:
        if args.command == "console":
            require_picocom()
        ports, interfaces = [], {}
        if not args.port:
            try:
                from serial.tools import list_ports
            except ImportError:
                if args.command in (None, "console"):
                    raise ProtocolError("pyserial is required. Install it in your Python environment with python3 -m pip install pyserial") from None
            else:
                ports = list(list_ports.comports())
                interfaces = interface_numbers(ports)
        if args.command == "console":
            device = args.port or select_port(ports, interfaces, args.serial)
            open_console(device)
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
        print_device(record, ports, interfaces)
        print(flush=True)
        if args.command == "dfu" and is_legacy_cdc(record):
            usb.util.dispose_resources(record.device)
            payload = legacy_dfu(record)
        elif service:
            print(session(usb.core, usb.util, record, service))
            return 0
        else:
            payload = exchange(usb.core, usb.util, record, COMMANDS[args.command])
        if args.command == "hello":
            print_hello(payload, record.serial_number)
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
    finally:
        if conversation_ns:
            print(f"\nStats: {conversation_ns / 1_000_000:.0f} ms USB I/O")


if __name__ == "__main__":
    sys.exit(main())
