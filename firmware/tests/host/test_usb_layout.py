import struct
import unittest
from tempfile import TemporaryDirectory
from pathlib import Path
from unittest.mock import MagicMock, Mock, call, patch

import airgrep


class UsbLayout(unittest.TestCase):
    def test_hello(self):
        payload = bytearray(488)
        struct.pack_into("<Q", payload, 0, 0x01020304)
        payload[40:47] = b"Variant"
        struct.pack_into("<6I", payload, 72, 1300, 1100, 1500, 2300, 2100, 2500)
        payload[96:103] = bytes.fromhex("02 41 01 11 3f 28 02")
        payload[104:112] = b"Firmware"
        payload[232:237] = b"Board"
        payload[360:364] = b"Chip"
        frame = bytearray(bytes.fromhex("01 ef 01 01 00 00 00") + payload)
        struct.pack_into("<H", frame, 4, airgrep.crc(frame))
        hello = airgrep.decode_hello(airgrep.decode_response(bytes(frame), airgrep.COMMANDS["hello"]))
        self.assertEqual(hello.variant, "Variant")
        self.assertEqual(hello.uptime, 0x01020304)
        self.assertEqual(hello.internal, [1300, 1100, 1500])
        self.assertEqual(hello.psram, [2300, 2100, 2500])
        self.assertEqual((hello.health, hello.mode, hello.service), (0x4102, 1, 2))
        self.assertEqual(hello.load, (17, 63, 40))
        self.assertEqual((hello.firmware, hello.board, hello.chip), ("Firmware", "Board", "Chip"))

    def test_bridge_refuses_service_commands_before_opening_the_port(self):
        with patch.object(airgrep, "load_serial") as serial:
            with self.assertRaisesRegex(airgrep.ProtocolError, "native USB"):
                airgrep.bridge_command(Mock(), "write", [])
            serial.assert_not_called()

    def test_bridge_refuses_a_mismatched_installed_partition_even_with_force(self):
        link = MagicMock()
        link.readline.return_value = b"AIRGREP/1\tP4 Nano\te8f60ae7f6b1\t1\tp4\t65536\t8388608\t4096\t65536\n"
        driver = Mock()
        driver.Serial.return_value = link
        with TemporaryDirectory() as folder, patch.object(airgrep, "load_serial", return_value=(driver, None)), \
                patch.object(airgrep, "esptool") as flash:
            table = Path(folder) / "tzdata.bin"
            data = bytearray(airgrep.TZDATA_HEADER)
            data[:4] = b"TZL2"
            struct.pack_into("<I", data, 40, len(data))
            table.write_bytes(data)
            with self.assertRaisesRegex(airgrep.ProtocolError, "partition"):
                airgrep.bridge_command(Mock(), "flash-tzdata", [table], True)
            flash.assert_not_called()

    def test_adsbee_write_preserves_other_bytes(self):
        frame = bytes.fromhex("01 f7 00 02 34 87 00") + bytes(range(240))
        state = airgrep.decode_response(frame, airgrep.COMMANDS["read"])
        expected = bytearray(range(240))
        expected[233] = 1
        send = Mock(side_effect=[state, None, bytes(expected)])
        airgrep.write_state(send, "adsbee", 233, b"\x01")
        self.assertEqual(send.call_args_list, [
            call(2, b"\x01"),
            call(3, bytes.fromhex("01 e9 00 01 00") + expected),
            call(2, b"\x01"),
        ])


if __name__ == "__main__":
    unittest.main()
