import struct
import unittest
from unittest.mock import Mock, call

import airgrep


class UsbLayout(unittest.TestCase):
    def test_hello(self):
        payload = bytearray(456)
        struct.pack_into("<Q", payload, 0, 0x01020304)
        struct.pack_into("<6I", payload, 40, 1300, 1100, 1500, 2300, 2100, 2500)
        payload[64:71] = bytes.fromhex("02 41 01 11 3f 28 02")
        payload[72:80] = b"Firmware"
        payload[200:205] = b"Board"
        payload[328:332] = b"Chip"
        frame = bytes.fromhex("01 cf 01 01 34 cd 00") + payload
        hello = airgrep.decode_hello(airgrep.decode_response(frame, airgrep.COMMANDS["hello"]))
        self.assertEqual(hello.uptime, 0x01020304)
        self.assertEqual(hello.internal, [1300, 1100, 1500])
        self.assertEqual(hello.psram, [2300, 2100, 2500])
        self.assertEqual((hello.health, hello.mode, hello.service), (0x4102, 1, 2))
        self.assertEqual(hello.load, (17, 63, 40))
        self.assertEqual((hello.firmware, hello.board, hello.chip), ("Firmware", "Board", "Chip"))

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
