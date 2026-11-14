"""Host orchestration checks. No firmware builds, serial opens or USB writes."""

from contextlib import contextmanager, redirect_stdout, redirect_stderr
import importlib.util
import io
import os
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


class IdentityTests(unittest.TestCase):
    def test_identity_survives_both_application_formats_and_rom(self):
        for value in (MAC, "airgrep-" + MAC, "A1:B2:C3:D4:E5:F6"):
            self.assertEqual(build_flash.identity(value), MAC)
        for value in (None, "airgrep-", "a1:b2c3d4e5f6", MAC + "0", "not-a-mac"):
            self.assertIsNone(build_flash.identity(value))

    def test_first_application_is_default_and_selector_overrides(self):
        records = [SimpleNamespace(serial_number=OTHER), SimpleNamespace(serial_number="airgrep-" + MAC)]
        with patch.object(build_flash, "snapshot", side_effect=lambda usb: snapshot(records)):
            self.assertEqual(build_flash.choose_device(None, None), OTHER)
            self.assertEqual(build_flash.choose_device(None, "A1:B2:C3:D4:E5:F6"), MAC)
            with self.assertRaisesRegex(build_flash.airgrep.ProtocolError, "No matching"):
                build_flash.choose_device(None, "123456789abc")

    def test_already_rom_device_is_selectable(self):
        ports = [port("/dev/rom", "A1:B2:C3:D4:E5:F6", rom=True)]
        with patch.object(build_flash, "snapshot", side_effect=lambda usb: snapshot(ports=ports)):
            self.assertEqual(build_flash.choose_device(None, None), MAC)
            with patch.object(build_flash.airgrep, "exchange") as exchange:
                build_flash.enter_bootloader(None, MAC)
            exchange.assert_not_called()

    def test_first_device_without_mac_is_not_silently_skipped(self):
        records = [SimpleNamespace(serial_number=None), SimpleNamespace(serial_number=MAC)]
        with patch.object(build_flash, "snapshot", side_effect=lambda usb: snapshot(records)):
            with self.assertRaisesRegex(build_flash.airgrep.ProtocolError, "first device"):
                build_flash.choose_device(None, None)

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
    def test_build_dfu_flash_monitor_order_and_scoped_monitor_config(self):
        events = []
        config_paths = []

        def run(command, cwd, check, env):
            self.assertEqual(cwd, TOOL.parents[1])
            self.assertTrue(check)
            self.assertEqual(command[:4], ["python", "idf.py", "--preset", "display"])
            action = command[-1]
            events.append(action)
            if action == "monitor":
                self.assertEqual(command[-3:], ["-p", "/dev/new-app", "monitor"])
                self.assertNotIn("--no-reset", command)
                path = Path(env["ESP_IDF_MONITOR_CFGFILE"])
                config_paths.append(path)
                self.assertEqual(path.read_text(), "[esp-idf-monitor]\ncustom_hard_reset_sequence = D1\n")
            else:
                self.assertIsNone(env)
                if action == "flash":
                    self.assertEqual(command[-3:], ["-p", "/dev/new-rom", "flash"])

        original = os.environ.get("ESP_IDF_MONITOR_CFGFILE")
        with patch.object(build_flash, "choose_device", return_value=MAC):
            with patch.object(build_flash, "enter_bootloader", side_effect=lambda usb, selected: events.append("dfu")):
                with patch.object(build_flash, "wait_port", side_effect=["/dev/new-rom", "/dev/new-app"]):
                    with patch.object(build_flash.subprocess, "run", side_effect=run):
                        with redirect_stdout(io.StringIO()):
                            build_flash.build_flash_monitor(["python", "idf.py"], None, "display", None, 15)
        self.assertEqual(events, ["build", "dfu", "flash", "monitor"])
        self.assertEqual(os.environ.get("ESP_IDF_MONITOR_CFGFILE"), original)
        self.assertFalse(config_paths[0].exists())

    def test_build_failure_stops_before_dfu(self):
        with patch.object(build_flash, "choose_device", return_value=MAC):
            with patch.object(build_flash, "run_idf", side_effect=subprocess.CalledProcessError(1, "build")):
                with patch.object(build_flash, "enter_bootloader") as dfu:
                    with redirect_stdout(io.StringIO()), self.assertRaises(subprocess.CalledProcessError):
                        build_flash.build_flash_monitor([], None, "s3", None, 15)
        dfu.assert_not_called()

    def test_dfu_failure_stops_before_flash(self):
        with patch.object(build_flash, "choose_device", return_value=MAC):
            with patch.object(build_flash, "run_idf") as run:
                with patch.object(build_flash, "enter_bootloader", side_effect=build_flash.airgrep.ProtocolError("no ack")):
                    with redirect_stdout(io.StringIO()), self.assertRaises(build_flash.airgrep.ProtocolError):
                        build_flash.build_flash_monitor([], None, "s3", None, 15)
        run.assert_called_once_with([], "s3", "build")

    def test_flash_failure_stops_before_monitor(self):
        with patch.object(build_flash, "choose_device", return_value=MAC):
            with patch.object(build_flash, "enter_bootloader"):
                with patch.object(build_flash, "wait_port", return_value="/dev/rom"):
                    with patch.object(build_flash, "run_idf", side_effect=[None, subprocess.CalledProcessError(1, "flash")]):
                        with patch.object(build_flash, "monitor") as monitor:
                            with redirect_stdout(io.StringIO()), self.assertRaises(subprocess.CalledProcessError):
                                build_flash.build_flash_monitor([], None, "s3", None, 15)
        monitor.assert_not_called()

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
                with patch.object(build_flash, "build_flash_monitor") as workflow:
                    self.assertEqual(build_flash.main([]), 0)
        workflow.assert_called_once_with(["idf.py"], None, "s3", None, 15)
        with patch.object(build_flash, "idf_command") as sdk:
            with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                build_flash.main(["--profile", "s3-test"])
        sdk.assert_not_called()


if __name__ == "__main__":
    unittest.main()
