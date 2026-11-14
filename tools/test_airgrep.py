"""Host checks for the USB CLI, without opening serial devices."""

import contextlib
import importlib.util
import io
from pathlib import Path
import struct
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch


spec = importlib.util.spec_from_file_location(
    "airgrep", Path(__file__).resolve().parents[1] / "firmware/tools/airgrep.py")
airgrep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(airgrep)


def response(command, status=0, payload=b"", version=1):
    frame = bytearray(struct.pack("<BHBHB", version, 7 + len(payload), command, 0, status) + payload)
    struct.pack_into("<H", frame, 4, airgrep.crc(frame))
    return frame


class ChunkReader:
    def __init__(self, chunks):
        self.chunks = iter(chunks)
        self.timeout = None

    def read(self, size):
        chunk = next(self.chunks, b"")
        assert len(chunk) <= size
        return chunk


class Interface(list):
    def __init__(self, number, class_id, endpoints):
        super().__init__(endpoints)
        self.bInterfaceNumber = number
        self.bInterfaceClass = class_id
        self.bAlternateSetting = 0


def endpoint(address, attributes=2):
    return SimpleNamespace(bEndpointAddress=address, bmAttributes=attributes, wMaxPacketSize=64)


def usb_record(serial="airgrep-a1b2c3d4e5f6"):
    vendor = (2, endpoint(0x83), endpoint(0x03))
    return SimpleNamespace(device=Mock(), product=serial, serial_number=serial, vendor=vendor, error=None)


class ProtocolTests(unittest.TestCase):
    def test_published_hello_request_vector(self):
        self.assertEqual(airgrep.request(1), bytes.fromhex("01 06 00 01 F5 50"))

    def test_published_version_error_vector(self):
        with self.assertRaisesRegex(airgrep.ProtocolError, "VERSION_MISMATCH"):
            airgrep.decode_response(bytes.fromhex("01 07 00 80 AA D1 02"), 1)

    def test_version_error_reports_device_version(self):
        with self.assertRaisesRegex(airgrep.ProtocolError, "device protocol 2, client 1"):
            airgrep.decode_response(response(0x80, 2, version=2), 1)

    def test_hello_requires_the_firmware_description(self):
        with self.assertRaisesRegex(airgrep.ProtocolError, "456-byte payload"):
            airgrep.decode_response(response(1, payload=bytes(72)), 1)

    def test_crc_includes_command_status_payload(self):
        frame = response(1, payload=bytes(200))
        for offset in (0, 1, 3, 6, 7, 78, 206):
            with self.subTest(offset=offset):
                damaged = bytearray(frame)
                damaged[offset] ^= 1
                with self.assertRaises(airgrep.ProtocolError):
                    airgrep.decode_response(damaged, 1)

    def test_hello_at_every_usb_split(self):
        frame = response(1, payload=bytes(200))
        for split in range(1, len(frame)):
            with self.subTest(split=split):
                # read() cannot return more bytes than requested, including across the header.
                chunks = [frame[:split], frame[split:]]
                pending = bytearray()

                def read(size):
                    if not pending:
                        pending.extend(chunks.pop(0))
                    part = bytes(pending[:size])
                    del pending[:size]
                    return part

                port = SimpleNamespace(read=read)
                with patch.object(airgrep.time, "monotonic", return_value=0):
                    self.assertEqual(airgrep.read_response(port, 1, 3), bytes(200))

    def test_trickle_does_not_extend_deadline(self):
        port = ChunkReader([b"\x01", b"\x07", b"\x00", b"\x80"])
        with patch.object(airgrep.time, "monotonic", side_effect=[0, 1, 1, 2, 2, 3]):
            with self.assertRaisesRegex(airgrep.ProtocolError, "timed out"):
                airgrep.read_response(port, 6, 3)

    def test_full_response_at_deadline_is_too_late(self):
        port = ChunkReader([response(0x80)])
        with patch.object(airgrep.time, "monotonic", side_effect=[0, 3]):
            with self.assertRaisesRegex(airgrep.ProtocolError, "timed out"):
                airgrep.read_response(port, 6, 3)

    def test_generic_ok_cannot_replace_hello(self):
        with self.assertRaisesRegex(airgrep.ProtocolError, "missing"):
            airgrep.decode_response(response(0x80), 1)

    def test_reboot_and_bootloader_require_status_only(self):
        for command in (6, 7):
            self.assertEqual(airgrep.decode_response(response(0x80), command), b"")
            for invalid in (response(command), response(0x80, payload=b"\0")):
                with self.assertRaises(airgrep.ProtocolError):
                    airgrep.decode_response(invalid, command)

    def test_short_frame_is_rejected(self):
        with patch.object(airgrep.time, "monotonic", return_value=0):
            with self.assertRaisesRegex(airgrep.ProtocolError, "smaller than"):
                airgrep.read_response(ChunkReader([b"\x01\x06\x00"]), 1, 3)

    def test_reset_drains_late_bytes_until_quiet(self):
        port = ChunkReader([b"old reply", b"tail", b""])
        with patch.object(airgrep.time, "monotonic", return_value=0):
            airgrep.drain_stale(port)
        self.assertEqual(list(port.chunks), [])
        self.assertEqual(port.timeout, 0.1)

    def test_reset_drain_is_bounded(self):
        port = ChunkReader([b"x", b"y", b"z"])
        with patch.object(airgrep.time, "monotonic", side_effect=[0, 0, 1, 2, 3]):
            with self.assertRaisesRegex(airgrep.ProtocolError, "did not become quiet"):
                airgrep.drain_stale(port)

    def test_hello_with_board_and_chip(self):
        payload = bytearray(456)
        payload[72:79] = b"airgrep"
        payload[200:207] = b"S3 Plus"
        chip = b"ESP32-S3 (QFN56) rev v0.2, 2 cores @ 80 MHz"
        payload[328:328 + len(chip)] = chip
        self.assertEqual(airgrep.decode_response(response(1, payload=payload), 1), payload)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            airgrep.print_hello(payload)
        self.assertIn("Board                S3 Plus\n", output.getvalue())
        self.assertIn("Chip                 ESP32-S3 (QFN56) rev v0.2, 2 cores @ 80 MHz\n", output.getvalue())

    def test_hello_before_board_reporting_has_no_board(self):
        hello = airgrep.decode_hello(bytes(200))
        self.assertEqual((hello.board, hello.chip), ("", ""))
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            airgrep.print_hello(bytes(200))
        self.assertNotIn("Board", output.getvalue())

    def test_hello_field_offsets_and_health(self):
        payload = bytearray(200)
        struct.pack_into("<Q", payload, 0, 456)
        payload[8:12] = b"test"
        struct.pack_into("<6I", payload, 40, 292427, 256132, 372115, 20480, 19456, 204800)
        struct.pack_into("<H", payload, 64, (1 << 6) | (1 << 9))
        payload[66:70] = bytes([2, 20, 0xFF, 10])
        description = b"airgrep test (Nov 14 2026 20:15:30)"
        payload[72:72 + len(description)] = description
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            airgrep.print_hello(payload, "68ee8f4d0c74")
        self.assertIn("Firmware             airgrep test (Nov 14 2026 20:15:30)", output.getvalue())
        self.assertIn("Protocol             1", output.getvalue())
        self.assertIn("MAC                  68:EE:8F:4D:0C:74", output.getvalue())
        self.assertIn("Device ID            airgrep-68ee8f4d0c74", output.getvalue())
        self.assertIn("Uptime               0d 00:07:36", output.getvalue())
        self.assertIn("Mode                 \033[38;2;146;179;244mCHARGER\033[0m", output.getvalue())
        self.assertIn("Load                 core0 20 %, core1 UNAVAILABLE, overall 10 %", output.getvalue())
        self.assertIn("Internal RAM         285 KB free, 250 KB minimum, 363 KB total", output.getvalue())
        self.assertIn("PSRAM                20 KB free, 19 KB minimum, 200 KB total", output.getvalue())
        self.assertIn("Health               \033[38;5;208mALERT (0x240 gnss, cpu)\033[0m", output.getvalue())


class UsbTests(unittest.TestCase):
    def setUp(self):
        airgrep.conversation_ns = 0
        self.record = usb_record()
        self.core = SimpleNamespace(USBTimeoutError=TimeoutError)
        self.util = Mock()
        self.record.device.write.return_value = 6

    def test_conversation_timing_excludes_reset_drain_and_response_decoding(self):
        clock = 0

        def advance(ns):
            nonlocal clock
            clock += ns

        def write(frame):
            advance(2_000_000)
            return len(frame)

        frame = response(0x80)
        chunks = iter([frame[:3], frame[3:]])

        def read(size):
            advance(3_000_000)
            return next(chunks)

        decode = airgrep.decode_response

        def slow_decode(frame, command):
            advance(20_000_000)
            return decode(frame, command)

        port = SimpleNamespace(read=read, write=write)
        with patch.object(airgrep.time, "perf_counter_ns", side_effect=lambda: clock), \
                patch.object(airgrep, "drain_stale", side_effect=lambda port: advance(100_000_000)), \
                patch.object(airgrep, "decode_response", side_effect=slow_decode):
            self.assertEqual(airgrep.transact(port, 6, lambda: advance(50_000_000)), b"")
        self.assertEqual(airgrep.conversation_ns, 8_000_000)

    def test_complete_error_reply_accumulates_time(self):
        airgrep.conversation_ns = 2_000_000
        self.record.device.read.side_effect = [TimeoutError(), response(0x80, 2)]
        with patch.object(airgrep.time, "perf_counter_ns", side_effect=[10_000_000, 13_000_000]):
            with self.assertRaisesRegex(airgrep.ProtocolError, "VERSION_MISMATCH"):
                airgrep.exchange(self.core, self.util, self.record, 1)
        self.assertEqual(airgrep.conversation_ns, 5_000_000)

    def test_hello_uses_full_usb_packets_and_only_vendor_interface(self):
        frame = response(1, payload=bytes(200))
        self.assertEqual(len(frame), 207)
        self.record.device.read.side_effect = [TimeoutError(), frame[:64], frame[64:128], frame[128:192], frame[192:]]
        with patch.object(airgrep.time, "monotonic", return_value=0):
            self.assertEqual(airgrep.exchange(self.core, self.util, self.record, 1), bytes(200))
        self.util.claim_interface.assert_called_once_with(self.record.device, 2)
        self.util.release_interface.assert_called_once_with(self.record.device, 2)

        self.util.dispose_resources.assert_called_once_with(self.record.device)
        self.record.device.ctrl_transfer.assert_called_once_with(0x41, 1, 0, 2, b"", timeout=3000)
        self.record.device.write.assert_called_once_with(0x03, airgrep.request(1), timeout=3000)
        for call in self.record.device.read.call_args_list:
            self.assertEqual(call.args, (0x83, 64))
        self.record.device.set_configuration.assert_not_called()
        self.record.device.reset.assert_not_called()
        self.record.device.detach_kernel_driver.assert_not_called()

    def test_timeout_resets_and_releases_without_retry(self):
        with patch.object(airgrep, "drain_stale"):
            with patch.object(airgrep, "read_response", side_effect=airgrep.ProtocolError("timeout")):
                with self.assertRaisesRegex(airgrep.ProtocolError, "timeout"):
                    airgrep.exchange(self.core, self.util, self.record, 7)
        self.assertEqual(self.record.device.ctrl_transfer.call_count, 2)
        self.record.device.write.assert_called_once()
        self.util.release_interface.assert_called_once_with(self.record.device, 2)
        self.assertEqual(airgrep.conversation_ns, 0)

    def test_version_mismatch_never_changes_versions_or_retries_dfu(self):
        for command in (1, 2, 3, 4, 5, 6, 7):
            with self.subTest(command=command):
                self.record.device.write.reset_mock()
                self.record.device.read.side_effect = [TimeoutError(), response(0x80, 2, version=2)]
                with patch.object(airgrep.time, "monotonic", return_value=0):
                    with self.assertRaises(airgrep.ProtocolError):
                        airgrep.exchange(self.core, self.util, self.record, command)
                self.record.device.write.assert_called_once()

    def test_dfu_corruption_or_unrecognized_version_is_not_retried(self):
        corrupt = response(0x80, 2, version=1)
        corrupt[4] ^= 1
        for frame in (corrupt, response(0x80, 2, version=3), response(0x80, version=2),
                      response(0x80, 2, version=1) + b"unexpected"):
            with self.subTest(frame=frame):
                self.record.device.write.reset_mock()
                self.record.device.read.side_effect = [TimeoutError(), frame]
                with patch.object(airgrep.time, "monotonic", return_value=0):
                    with self.assertRaises(airgrep.ProtocolError):
                        airgrep.exchange(self.core, self.util, self.record, 7)
                self.record.device.write.assert_called_once()

    def test_dfu_incomplete_write_is_never_retried(self):
        self.record.device.write.return_value = 2
        self.record.device.read.side_effect = [TimeoutError()]
        with patch.object(airgrep.time, "monotonic", return_value=0):
            with self.assertRaisesRegex(airgrep.ProtocolError, "Incomplete request"):
                airgrep.exchange(self.core, self.util, self.record, 7)
        self.record.device.write.assert_called_once()

    def test_zero_length_usb_packet_is_not_a_quiet_timeout(self):
        self.record.device.read.side_effect = [b"", b"old reply"]
        channel = airgrep.UsbChannel(self.record.device, self.record.vendor, TimeoutError)
        with patch.object(airgrep.time, "monotonic", return_value=0):
            self.assertEqual(channel.read(1024), b"old reply")
        self.assertEqual(self.record.device.read.call_count, 2)

    def test_busy_interface_does_not_send_reset_or_detach_console(self):
        self.util.claim_interface.side_effect = OSError("busy")
        with self.assertRaisesRegex(OSError, "busy"):
            airgrep.exchange(self.core, self.util, self.record, 1)
        self.record.device.ctrl_transfer.assert_not_called()
        self.record.device.detach_kernel_driver.assert_not_called()
        self.util.release_interface.assert_not_called()
        self.util.dispose_resources.assert_called_once()

    def test_endpoints_are_discovered_not_hardcoded(self):
        control = Interface(0, 2, [endpoint(0x81, 3)])
        data = Interface(1, 10, [endpoint(0x82), endpoint(0x02)])
        vendor = Interface(2, 0xFF, [endpoint(0x85), endpoint(0x04)])
        number, incoming, outgoing = airgrep.vendor_interface([control, data, vendor])
        self.assertEqual((number, incoming.bEndpointAddress, outgoing.bEndpointAddress), (2, 0x85, 0x04))

    def test_legacy_dual_cdc_is_not_a_vendor_transport(self):
        with self.assertRaises(airgrep.ProtocolError):
            airgrep.vendor_interface([Interface(2, 2, [endpoint(0x83), endpoint(0x03)])])
        self.record.vendor = None
        self.record.error = "legacy CDC firmware"
        with self.assertRaisesRegex(airgrep.ProtocolError, "Upgrade the firmware"):
            airgrep.exchange(self.core, self.util, self.record, 1)
        self.util.claim_interface.assert_not_called()

    def test_duplicate_vendor_interfaces_are_rejected(self):
        with self.assertRaises(airgrep.ProtocolError):
            airgrep.vendor_interface([Interface(n, 0xFF, [endpoint(0x83), endpoint(0x03)]) for n in (2, 3)])

    def test_unknown_products_are_not_usb_candidates(self):
        device = self.record.device
        device.idVendor, device.idProduct = airgrep.APP_USB
        device.product = "unrelated"
        core = SimpleNamespace(find=lambda **kwargs: [device], NoBackendError=ValueError)
        self.assertEqual(airgrep.usb_devices(core), [])

    def test_foreign_usb_ids_are_never_opened(self):
        device = Mock(idVendor=0x05AC, idProduct=0x8104, spec=["idVendor", "idProduct"])
        core = SimpleNamespace(find=lambda **kwargs: [device], NoBackendError=ValueError)
        self.assertEqual(airgrep.usb_devices(core), [])

    def test_application_and_legacy_ids_are_candidates(self):
        for ids in (airgrep.APP_USB, airgrep.LEGACY_USB):
            device = Mock(product="airgrep-a1b2c3d4e5f6", serial_number="a1b2c3d4e5f6")
            device.idVendor, device.idProduct = ids
            device.get_active_configuration.return_value = [
                Interface(2, 0xFF, [endpoint(0x83), endpoint(0x03)])]
            core = SimpleNamespace(find=lambda **kwargs: [device], NoBackendError=ValueError)
            self.assertEqual([record.serial_number for record in airgrep.usb_devices(core)], ["a1b2c3d4e5f6"])

    def test_macos_homebrew_backend_fallback(self):
        backend = object()
        libusb1 = SimpleNamespace(get_backend=Mock(side_effect=[None, backend]))
        with patch.object(airgrep.sys, "platform", "darwin"):
            with patch.object(airgrep.Path, "exists", return_value=True):
                self.assertIs(airgrep.load_backend(libusb1), backend)
        finder = libusb1.get_backend.call_args.kwargs["find_library"]
        self.assertEqual(finder("usb-1.0"), "/opt/homebrew/lib/libusb-1.0.dylib")

    def test_missing_backend_has_install_guidance(self):
        libusb1 = SimpleNamespace(get_backend=lambda: None)
        with patch.object(airgrep.sys, "platform", "linux"):
            with self.assertRaisesRegex(airgrep.ProtocolError, "Install it"):
                airgrep.load_backend(libusb1)


class MigrationTests(unittest.TestCase):
    def setUp(self):
        self.record = usb_record()
        self.record.vendor = None
        self.record.device.idVendor = 0x303A
        self.record.device.idProduct = 0x4002
        self.port_info = SimpleNamespace(device="/dev/cu.legacy", product=self.record.product,
                                         serial_number=self.record.serial_number, vid=0x303A, pid=0x4002)

    def test_legacy_port_matches_serial_and_data_interface(self):
        other = SimpleNamespace(device="/dev/cu.other", product="airgrep", serial_number="other",
                                vid=0x303A, pid=0x4002)
        ports = [other, self.port_info]
        for interface in (2, 3):
            interfaces = {other.device: interface, self.port_info.device: interface}
            self.assertEqual(airgrep.legacy_data_port(self.record, ports, interfaces), self.port_info.device)
        for interface in (0, 1, None):
            with self.assertRaises(airgrep.ProtocolError):
                airgrep.legacy_data_port(self.record, ports, {other.device: 2, self.port_info.device: interface})

    def test_missing_serial_or_wrong_identity_is_not_guessed(self):
        for field, value in (("serial_number", None), ("product", "other")):
            saved = getattr(self.record, field)
            setattr(self.record, field, value)
            with self.assertRaises(airgrep.ProtocolError):
                airgrep.legacy_data_port(self.record, [self.port_info], {self.port_info.device: 2})
            setattr(self.record, field, saved)
        self.port_info.pid = 0x4021
        with self.assertRaises(airgrep.ProtocolError):
            airgrep.legacy_data_port(self.record, [self.port_info], {self.port_info.device: 2})

    def test_legacy_dfu_validates_ack_and_closes_without_retry(self):
        port = Mock(is_open=True)
        port.write.return_value = 6
        port.read.side_effect = [b"", response(0x80)]
        serial = SimpleNamespace(Serial=Mock(return_value=port))
        tools = SimpleNamespace(list_ports=SimpleNamespace(comports=lambda: [self.port_info]))
        with patch.dict(airgrep.sys.modules, {"serial": serial, "serial.tools": tools}):
            with patch.object(airgrep, "interface_numbers", return_value={self.port_info.device: 3}):
                with patch.object(airgrep.time, "monotonic", return_value=0):
                    self.assertEqual(airgrep.legacy_dfu(self.record), b"")
        self.assertEqual(port.port, self.port_info.device)
        self.assertTrue(port.dtr)
        port.reset_input_buffer.assert_called_once()
        port.reset_output_buffer.assert_called_once()
        port.write.assert_called_once_with(airgrep.request(7))
        port.close.assert_called_once()

    def test_legacy_dfu_version_rejection_is_not_retried(self):
        port = Mock(is_open=True)
        port.write.return_value = 6
        port.read.side_effect = [b"", response(0x80, 2, version=2)]
        serial = SimpleNamespace(Serial=Mock(return_value=port))
        tools = SimpleNamespace(list_ports=SimpleNamespace(comports=lambda: [self.port_info]))
        with patch.dict(airgrep.sys.modules, {"serial": serial, "serial.tools": tools}):
            with patch.object(airgrep, "interface_numbers", return_value={self.port_info.device: 3}):
                with patch.object(airgrep.time, "monotonic", return_value=0):
                    with self.assertRaisesRegex(airgrep.ProtocolError, "VERSION_MISMATCH"):
                        airgrep.legacy_dfu(self.record)
        port.write.assert_called_once_with(airgrep.request(7))
        self.assertEqual(port.reset_input_buffer.call_count, 2)
        self.assertEqual(port.reset_output_buffer.call_count, 2)
        port.close.assert_called_once()

    def test_legacy_dfu_timeout_does_not_retry(self):
        port = Mock(is_open=True)
        port.write.return_value = 6
        port.read.return_value = b""
        serial = SimpleNamespace(Serial=Mock(return_value=port))
        tools = SimpleNamespace(list_ports=SimpleNamespace(comports=lambda: [self.port_info]))
        with patch.dict(airgrep.sys.modules, {"serial": serial, "serial.tools": tools}):
            with patch.object(airgrep, "interface_numbers", return_value={self.port_info.device: 3}):
                with patch.object(airgrep, "read_response", side_effect=airgrep.ProtocolError("timeout")):
                    with self.assertRaisesRegex(airgrep.ProtocolError, "timeout"):
                        airgrep.legacy_dfu(self.record)
        port.write.assert_called_once_with(airgrep.request(7))
        self.assertFalse(port.dtr)
        port.close.assert_called_once()

    def test_only_legacy_dfu_routes_to_cdc(self):
        usb = SimpleNamespace(core=Mock(), util=Mock(), backend=SimpleNamespace(libusb1=Mock()))
        modules = {"usb": usb, "usb.core": usb.core, "usb.util": usb.util,
                   "usb.backend": usb.backend, "usb.backend.libusb1": usb.backend.libusb1}
        for command, vendor in (("dfu", None), ("hello", None), ("reboot", None),
                                ("dfu", (2, endpoint(0x83), endpoint(0x03)))):
            self.record.vendor = vendor
            with self.subTest(command=command, vendor=vendor), patch.dict(airgrep.sys.modules, modules):
                with patch.object(airgrep, "usb_devices", return_value=[self.record]):
                    with patch.object(airgrep, "legacy_dfu", return_value=b"") as legacy:
                        with patch.object(airgrep, "exchange", return_value=bytes(200)) as exchange:
                            with contextlib.redirect_stdout(io.StringIO()):
                                self.assertEqual(airgrep.main([command]), 0)
                            self.assertEqual(legacy.call_count, int(command == "dfu" and vendor is None))
                            self.assertEqual(exchange.call_count, int(command != "dfu" or vendor is not None))


class DiscoveryTests(unittest.TestCase):
    def setUp(self):
        self.record = usb_record()
        self.ports = [SimpleNamespace(device="/dev/cu.usbmodem1", product=self.record.product,
                                      serial_number=self.record.serial_number, interface="airgrep Console")]
        self.interfaces = {self.ports[0].device: 0}

    def test_selected_device_precedes_command_and_stats_follow_reply(self):
        tools = SimpleNamespace(list_ports=SimpleNamespace(comports=lambda: self.ports))
        core = SimpleNamespace(USBTimeoutError=TimeoutError)
        usb = SimpleNamespace(core=core, util=Mock(), backend=SimpleNamespace(libusb1=Mock()))
        modules = {"serial.tools": tools, "usb": usb, "usb.core": core,
                   "usb.util": usb.util, "usb.backend": usb.backend,
                   "usb.backend.libusb1": usb.backend.libusb1}
        for status in (0, 2):
            output = io.StringIO()
            self.record.device.read.side_effect = [TimeoutError(), response(0x80, status)]

            def write(endpoint, frame, **kwargs):
                self.assertTrue(output.getvalue().startswith(f"Device    {self.record.product}\n"))
                self.assertIn("Console /dev/cu.usbmodem1 (Interface 0)", output.getvalue())
                self.assertIn("Data    Vendor interface 2, bulk OUT 0x03, IN 0x83\n\n", output.getvalue())
                self.assertNotIn("--serial", output.getvalue())
                self.assertNotIn("Stats:", output.getvalue())
                return len(frame)

            self.record.device.write.side_effect = write
            with self.subTest(status=status), patch.dict(airgrep.sys.modules, modules), \
                    patch.object(airgrep, "usb_devices", return_value=[self.record]), \
                    patch.object(airgrep, "interface_numbers", return_value=self.interfaces), \
                    patch.object(airgrep.time, "perf_counter_ns", side_effect=[0, 1_234_567]), \
                    contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
                self.assertEqual(airgrep.main(["reboot"]), int(status != 0))
            self.assertTrue(output.getvalue().endswith("\nStats: 1 ms USB I/O\n"))
            reply = "VERSION_MISMATCH" if status else "Reboot acknowledged."
            self.assertLess(output.getvalue().index(reply), output.getvalue().index("Stats:"))

    def test_console_selected_by_interface(self):
        self.assertEqual(airgrep.select_port(self.ports, self.interfaces), self.ports[0].device)

    def test_unknown_interface_is_never_guessed(self):
        with self.assertRaisesRegex(airgrep.ProtocolError, "Cannot identify"):
            airgrep.select_port(self.ports, {})

    def test_console_data_interface_is_also_accepted(self):
        interfaces = {self.ports[0].device: 1}
        self.assertEqual(airgrep.select_port(self.ports, interfaces), self.ports[0].device)
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            airgrep.print_devices([self.record], self.ports, interfaces)
        self.assertIn("Console /dev/cu.usbmodem1 (Interface 1)", output.getvalue())
        self.assertIn("Data    Vendor interface 2, bulk OUT 0x03, IN 0x83", output.getvalue())

    def test_multiple_boards_require_selection(self):
        second = usb_record("airgrep-001122334455")
        records = [self.record, second]
        with self.assertRaisesRegex(airgrep.ProtocolError, "Multiple AIRGREP"):
            airgrep.select_usb(records)
        self.assertIs(airgrep.select_usb(records, second.serial_number), second)
        self.assertIs(airgrep.select_usb(records, "00:11:22:33:44:55"), second)
        with self.assertRaisesRegex(airgrep.ProtocolError, "No AIRGREP"):
            airgrep.select_usb(records, "ffffffffffff")

    def test_other_products_are_ignored(self):
        for product in ("Espressif Device", "airgrep-other", "airgrep-12345678901",
                        "airgrep-1234567890123", "airgrep-12345678901z", "airgrep-123456789012\n"):
            with self.subTest(product=product):
                self.ports[0].product = product
                with self.assertRaises(airgrep.ProtocolError):
                    airgrep.select_port(self.ports, self.interfaces)

    def test_device_id_product_names_are_selected(self):
        for product in ("airgrep-a1b2c3d4e5f6", "airgrep-A1B2C3D4E5F6"):
            with self.subTest(product=product):
                for port in self.ports:
                    port.product = product
                    port.serial_number = product
                self.assertEqual(airgrep.select_port(self.ports, self.interfaces,
                                                    serial_number=product), self.ports[0].device)

    def test_ioreg_uses_nearest_interface_not_device_location(self):
        tree = [{"IORegistryEntryChildren": [
            {"bInterfaceNumber": number, "IORegistryEntryChildren": [
                {"IORegistryEntryChildren": [{"IOCalloutDevice": device}]}]}
            for number, device in ((0, "console"), (2, "data"))]}]
        self.assertEqual(airgrep.macos_interfaces(tree), {"console": 0, "data": 2})

    def test_no_command_lists_devices_without_opening_serial(self):
        serial = SimpleNamespace(Serial=lambda **kwargs: self.fail("Listing must not open a port"))
        tools = SimpleNamespace(list_ports=SimpleNamespace(comports=lambda: self.ports))
        usb = SimpleNamespace(core=Mock(), util=Mock(), backend=SimpleNamespace(libusb1=Mock()))
        output = io.StringIO()
        modules = {"serial": serial, "serial.tools": tools, "usb": usb,
                   "usb.core": usb.core, "usb.util": usb.util,
                   "usb.backend": usb.backend, "usb.backend.libusb1": usb.backend.libusb1}
        with patch.dict(airgrep.sys.modules, modules):
            with patch.object(airgrep, "usb_devices", return_value=[self.record]):
                with patch.object(airgrep, "interface_numbers", return_value=self.interfaces):
                    with contextlib.redirect_stdout(output):
                        self.assertEqual(airgrep.main([]), 0)
        self.assertTrue(output.getvalue().startswith("usage: "))
        self.assertIn("{hello,reboot,dfu,console}", output.getvalue())
        self.assertNotIn("positional arguments:", output.getvalue())
        self.assertEqual(output.getvalue().count(f"--serial {self.record.serial_number}"), 1)
        self.assertIn("Console /dev/cu.usbmodem1 (Interface 0)", output.getvalue())
        self.assertIn("Data    Vendor interface 2", output.getvalue())
        self.assertNotIn("Stats:", output.getvalue())
        usb.util.claim_interface.assert_not_called()
        self.record.device.ctrl_transfer.assert_not_called()

    def test_listing_filters_devices_and_preserves_exact_serial(self):
        second = usb_record("airgrep-001122334455")
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            airgrep.print_devices([self.record, second], self.ports, self.interfaces, self.record.serial_number)
        self.assertIn("--serial airgrep-a1b2c3d4e5f6", output.getvalue())
        self.assertNotIn(second.serial_number, output.getvalue())

    def test_listing_reports_missing_identity(self):
        self.record.serial_number = None
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            airgrep.print_devices([self.record], self.ports, {})
        self.assertIn("Serial unavailable", output.getvalue())
        self.assertIn("Console Path unavailable", output.getvalue())

    def test_listing_no_devices_is_clean(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            airgrep.print_devices([], self.ports, {})
        self.assertEqual(output.getvalue(), "No AIRGREP USB devices found.\n")

    def test_port_is_console_only(self):
        for command in ("hello", "reboot", "dfu"):
            with self.subTest(command=command), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    airgrep.main([command, "--port", "/dev/cu.usbmodem1"])

    def test_explicit_console_port_execs_picocom_without_pyserial(self):
        with patch.object(airgrep.shutil, "which", return_value="/bin/picocom"):
            with patch.object(airgrep.os, "execvp", side_effect=SystemExit) as execute:
                with self.assertRaises(SystemExit):
                    airgrep.main(["console", "--port", "/dev/ttyUSB0"])
        execute.assert_called_once_with("picocom", ["picocom", "--baud", "115200", "/dev/ttyUSB0"])


if __name__ == "__main__":
    unittest.main()
