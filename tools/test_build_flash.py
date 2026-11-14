"""Host orchestration checks. No firmware builds, serial opens or USB writes."""

from contextlib import contextmanager, redirect_stdout, redirect_stderr
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch


TOOL = Path(__file__).resolve().parents[1] / "firmware/tools/build-flash.py"
spec = importlib.util.spec_from_file_location("build_flash", TOOL)
build_flash = importlib.util.module_from_spec(spec)
with patch.object(sys, "path", [str(TOOL.parent), *sys.path]):
    spec.loader.exec_module(build_flash)

MAC = "a1b2c3d4e5f6"
OTHER = "001122334455"


def port(path, serial, rom=False):
    return SimpleNamespace(device=path, serial_number=serial, product="airgrep-" + MAC,
                           vid=0x303A if rom else 0xFA00, pid=0x1001 if rom else 0xFA01)


@contextmanager
def snapshot(records=(), ports=()):
    yield records, ports


def application(serial, legacy=False):
    ids = build_flash.airgrep.LEGACY_USB if legacy else build_flash.airgrep.APP_USB
    return SimpleNamespace(device=SimpleNamespace(idVendor=ids[0], idProduct=ids[1]), product="airgrep-" + MAC,
                           serial_number=serial, error="legacy dual-CDC firmware" if legacy else None,
                           vendor=None if legacy else (2, SimpleNamespace(bEndpointAddress=0x83),
                                                       SimpleNamespace(bEndpointAddress=0x03)))


def hello(board=b"S3 Plus"):
    payload = bytearray(456 if board else 200)
    payload[72:79] = b"airgrep"
    if board:
        payload[200:200 + len(board)] = board
        payload[328:336] = b"ESP32-S3"
    return bytes(payload)


def target(confirmed=True):
    return SimpleNamespace(mac=MAC, confirmed=confirmed)


class IdentityTests(unittest.TestCase):
    def discover(self, records=(), ports=(), selector=None, profile="s3", reply=None):
        """Returns the target and what was printed. Without a reply the device stays silent."""
        usb = SimpleNamespace(core=None, util=None)
        exchange = Mock(side_effect=[reply] if reply else build_flash.airgrep.ProtocolError("timeout"))
        output = io.StringIO()
        with patch.object(build_flash, "snapshot", side_effect=lambda usb: snapshot(records, ports)):
            with patch.object(build_flash.airgrep, "exchange", exchange), redirect_stdout(output), \
                    patch.object(build_flash.airgrep, "interface_numbers", return_value={"/dev/console": 1}):
                return build_flash.discover(usb, selector, profile), output.getvalue()

    def test_identity_survives_both_application_formats_and_rom(self):
        for value in (MAC, "airgrep-" + MAC, "A1:B2:C3:D4:E5:F6"):
            self.assertEqual(build_flash.identity(value), MAC)
        for value in (None, "airgrep-", "a1:b2c3d4e5f6", MAC + "0", "not-a-mac"):
            self.assertIsNone(build_flash.identity(value))

    def test_first_application_is_default_and_selector_overrides(self):
        records = [application(OTHER), application("airgrep-" + MAC)]
        found, output = self.discover(records, reply=hello())
        self.assertEqual(found.mac, OTHER)
        self.assertIn(f"first of 2 devices. Others: {MAC}", output)
        self.assertEqual(self.discover(records, selector="A1:B2:C3:D4:E5:F6", reply=hello())[0].mac, MAC)
        self.assertIsNone(self.discover(records, selector="123456789abc")[0])
        self.assertIsNone(self.discover()[0])

    def test_hello_matching_the_profile_confirms_and_shows_the_device(self):
        found, output = self.discover([application(MAC)], [port("/dev/console", MAC)], reply=hello())
        self.assertTrue(found.confirmed)
        self.assertTrue(output.startswith(
            f"Device    airgrep-{MAC}\n  Console /dev/console (Interface 1)\n"
            "  Data    Vendor interface 2, bulk OUT 0x03, IN 0x83\n\nFirmware             airgrep\n"
            "Board                S3 Plus\nChip                 ESP32-S3\n"), output)
        self.assertIn(f"Device ID            airgrep-{MAC}\n", output)
        self.assertTrue(self.discover([application(MAC)], reply=hello(board=None))[0].confirmed)

    def test_other_board_stops(self):
        display = hello(b"Seeed Display 1.47")
        with self.assertRaisesRegex(build_flash.airgrep.ProtocolError, "board Seeed Display 1.47, profile s3"):
            self.discover([application(MAC)], reply=display)
        self.assertTrue(self.discover([application(MAC)], profile="display", reply=display)[0].confirmed)

    def test_silent_device_is_not_confirmed(self):
        found, output = self.discover([application(MAC)])
        self.assertEqual((found.mac, found.confirmed), (MAC, False))
        self.assertIn("No HELLO reply: timeout", output)

    def test_legacy_firmware_is_confirmed_without_hello(self):
        self.assertTrue(self.discover([application(MAC, legacy=True)])[0].confirmed)

    def test_already_rom_device_is_selectable(self):
        ports = [port("/dev/rom", "A1:B2:C3:D4:E5:F6", rom=True)]
        self.assertEqual(self.discover(ports=ports)[0].mac, MAC)
        self.assertEqual(self.discover(ports=ports)[1], f"Device    ESP32-S3 {MAC}\n  ROM     /dev/rom (download mode)\n\n")
        self.assertFalse(self.discover(ports=ports)[0].confirmed)
        self.assertTrue(self.discover(ports=ports, selector=MAC)[0].confirmed)
        with patch.object(build_flash, "snapshot", side_effect=lambda usb: snapshot(ports=ports)):
            with patch.object(build_flash.airgrep, "exchange") as exchange:
                build_flash.enter_bootloader(None, MAC)
            exchange.assert_not_called()

    def test_first_device_without_mac_is_not_silently_skipped(self):
        records = [application(None), application(MAC)]
        with self.assertRaisesRegex(build_flash.airgrep.ProtocolError, "first device"):
            self.discover(records)

    def test_rom_wait_ignores_another_devices_new_port(self):
        usb = SimpleNamespace(ports=Mock(side_effect=[
            [port("/dev/wrong-new-port", OTHER, rom=True)],
            [port("/dev/selected-rom", "A1:B2:C3:D4:E5:F6", rom=True)],
        ]))
        with patch.object(build_flash.time, "monotonic", return_value=0):
            with patch.object(build_flash.time, "sleep") as sleep:
                self.assertEqual(build_flash.wait_port(usb, MAC, 15, rom=True), "/dev/selected-rom")
        sleep.assert_called_once()

    def test_console_wait_matches_changed_path_and_normalized_serial(self):
        ports = [port("/dev/other-console", OTHER), port("/dev/new-console", MAC), port("/dev/not-console", MAC)]
        usb = SimpleNamespace(ports=lambda: ports)
        interfaces = {ports[0].device: 0, ports[1].device: 1, ports[2].device: 2}
        with patch.object(build_flash.airgrep, "interface_numbers", return_value=interfaces):
            with patch.object(build_flash.time, "monotonic", return_value=0):
                self.assertEqual(build_flash.wait_port(usb, MAC, 15, rom=False), "/dev/new-console")

    def test_transition_wait_is_finite(self):
        usb = SimpleNamespace(ports=lambda: [])
        with patch.object(build_flash.time, "monotonic", side_effect=[0, 1]):
            with self.assertRaisesRegex(build_flash.airgrep.ProtocolError, "Timed out"):
                build_flash.wait_port(usb, MAC, 1, rom=True)

    def test_duplicate_transition_identity_is_rejected(self):
        usb = SimpleNamespace(ports=lambda: [port("/dev/one", MAC, True), port("/dev/two", MAC, True)])
        with self.assertRaisesRegex(build_flash.airgrep.ProtocolError, "Multiple ports"):
            build_flash.wait_port(usb, MAC, 15, rom=True)


class WorkflowTests(unittest.TestCase):
    def flow(self, targets, answer=None):
        """Runs the workflow and returns its steps. Without an answer there is no terminal to ask."""
        events = []
        with patch.object(build_flash, "discover", side_effect=targets), \
                patch.object(build_flash, "run_idf", side_effect=lambda command, profile, *a: events.append(a[-1])), \
                patch.object(build_flash, "enter_bootloader", side_effect=lambda usb, selected: events.append("dfu")), \
                patch.object(build_flash, "wait_port", return_value="/dev/port"), \
                patch.object(build_flash.airgrep, "open_console", side_effect=lambda path: events.append("console")), \
                patch.object(build_flash.sys, "stdin", Mock(isatty=lambda: answer is not None)), \
                patch("builtins.input", return_value=answer), redirect_stdout(io.StringIO()):
            build_flash.build_flash([], None, "s3", None, 15)
        return events

    def test_build_dfu_flash_console_order(self):
        events = []

        def run(command, cwd, check):
            self.assertEqual(cwd, TOOL.parents[1])
            self.assertTrue(check)
            self.assertEqual(command[:4], ["python", "idf.py", "--preset", "display"])
            events.append(command[-1])
            if command[-1] == "flash":
                self.assertEqual(command[-3:], ["-p", "/dev/new-rom", "flash"])

        with patch.object(build_flash, "discover", return_value=target()):
            with patch.object(build_flash, "enter_bootloader", side_effect=lambda usb, selected: events.append("dfu")):
                with patch.object(build_flash, "wait_port", side_effect=["/dev/new-rom", "/dev/new-app"]):
                    with patch.object(build_flash.subprocess, "run", side_effect=run):
                        with patch.object(build_flash.airgrep.os, "execvp") as execute:
                            with redirect_stdout(io.StringIO()):
                                build_flash.build_flash(["python", "idf.py"], None, "display", None, 15)
        self.assertEqual(events, ["build", "dfu", "flash"])
        execute.assert_called_once_with("picocom", ["picocom", "--baud", "115200", "/dev/new-app"])

    def test_unconfirmed_device_is_flashed_only_on_yes(self):
        self.assertEqual(self.flow([target(False)], answer="y"), ["build", "dfu", "flash", "console"])
        for answer in ("", "n", None):
            self.assertEqual(self.flow([target(False)], answer=answer), ["build"])

    def test_no_device_builds_then_stops(self):
        with patch.object(build_flash, "run_idf") as run:
            with patch.object(build_flash, "discover", return_value=None):
                with redirect_stdout(io.StringIO()):
                    with self.assertRaisesRegex(build_flash.airgrep.ProtocolError, "Built, not flashed"):
                        build_flash.build_flash([], None, "s3", None, 15)
        run.assert_called_once_with([], "s3", "build")

    def test_device_connected_during_the_build_is_always_asked(self):
        self.assertEqual(self.flow([None, target()], answer="n"), ["build"])
        self.assertEqual(self.flow([None, target()], answer="yes"), ["build", "dfu", "flash", "console"])

    def test_board_mismatch_stops_before_the_build(self):
        with patch.object(build_flash, "discover", side_effect=build_flash.airgrep.ProtocolError("board")):
            with patch.object(build_flash, "run_idf") as run:
                with self.assertRaises(build_flash.airgrep.ProtocolError):
                    build_flash.build_flash([], None, "s3", None, 15)
        run.assert_not_called()

    def test_build_failure_stops_before_dfu(self):
        with patch.object(build_flash, "discover", return_value=target()):
            with patch.object(build_flash, "run_idf", side_effect=subprocess.CalledProcessError(1, "build")):
                with patch.object(build_flash, "enter_bootloader") as dfu:
                    with redirect_stdout(io.StringIO()), self.assertRaises(subprocess.CalledProcessError):
                        build_flash.build_flash([], None, "s3", None, 15)
        dfu.assert_not_called()

    def test_dfu_failure_stops_before_flash(self):
        with patch.object(build_flash, "discover", return_value=target()):
            with patch.object(build_flash, "run_idf") as run:
                with patch.object(build_flash, "enter_bootloader", side_effect=build_flash.airgrep.ProtocolError("no ack")):
                    with redirect_stdout(io.StringIO()), self.assertRaises(build_flash.airgrep.ProtocolError):
                        build_flash.build_flash([], None, "s3", None, 15)
        run.assert_called_once_with([], "s3", "build")

    def test_flash_failure_stops_before_console(self):
        with patch.object(build_flash, "discover", return_value=target()):
            with patch.object(build_flash, "enter_bootloader"):
                with patch.object(build_flash, "wait_port", return_value="/dev/rom"):
                    with patch.object(build_flash, "run_idf", side_effect=[None, subprocess.CalledProcessError(1, "flash")]):
                        with patch.object(build_flash.airgrep, "open_console") as console:
                            with redirect_stdout(io.StringIO()), self.assertRaises(subprocess.CalledProcessError):
                                build_flash.build_flash([], None, "s3", None, 15)
        console.assert_not_called()

    def test_missing_preset_is_reported(self):
        with self.assertRaisesRegex(build_flash.airgrep.ProtocolError, "no preset missing"):
            build_flash.variation("missing", {"hwtest": "sdkconfig.hwtest"})

    def test_missing_sdk_fails_before_discovery(self):
        with patch.dict(build_flash.os.environ, {}, clear=True):
            with patch.object(build_flash.shutil, "which", return_value=None):
                with patch.object(build_flash, "dependencies") as dependencies:
                    with redirect_stderr(io.StringIO()):
                        self.assertEqual(build_flash.main([]), 1)
        dependencies.assert_not_called()

    def test_profile_defaults_and_invalid_profile_fails_before_sdk(self):
        with patch.object(build_flash, "idf_command", return_value=["idf.py"]):
            with patch.object(build_flash, "dependencies", return_value=None):
                with patch.object(build_flash, "build_flash") as workflow:
                    self.assertEqual(build_flash.main([]), 0)
        workflow.assert_called_once_with(["idf.py"], None, "s3", None, 15)
        with patch.object(build_flash, "idf_command") as sdk:
            with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                build_flash.main(["--profile", "s3-test"])
        sdk.assert_not_called()

    def test_service_profile_is_forwarded(self):
        with patch.object(build_flash, "idf_command", return_value=["idf.py"]):
            with patch.object(build_flash, "dependencies", return_value=None):
                with patch.object(build_flash, "build_flash") as workflow:
                    self.assertEqual(build_flash.main(["--profile", "s3-debug"]), 0)
        workflow.assert_called_once_with(["idf.py"], None, "s3-debug", None, 15)

    def test_flags_derive_a_build_directory_from_the_profile_preset(self):
        with patch.object(build_flash, "idf_command", return_value=["idf.py"]):
            with patch.object(build_flash, "dependencies", return_value=None):
                with patch.object(build_flash, "build_flash") as workflow:
                    self.assertEqual(build_flash.main(
                        ["--profile", "display", "--hwtest", "--debug-uart", "--display", "seeed-172x320-touch"]), 0)
        build = "build/display+debug-uart+hwtest+seeed-172x320-touch"
        workflow.assert_called_once_with(
            ["idf.py", "-B", build, f"-DSDKCONFIG={build}/sdkconfig",
             "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.board.display;sdkconfig.uart0;sdkconfig.hwtest;"
             "components/display/variants/seeed-172x320-touch/sdkconfig"], None, "display", None, 15)


if __name__ == "__main__":
    unittest.main()
