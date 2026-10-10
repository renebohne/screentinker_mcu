#!/usr/bin/env python3
"""
ScreenTinker Host Simulator E2E Integration Test Suite.
Validates end-to-end HTTP synchronization, ETag caching, telemetry headers,
bilingual diagnostic screens, 6-digit PIN pairing, and BMP output.
"""

import json
import os
import struct
import subprocess
import tempfile
import unittest
import urllib.request
from pathlib import Path

from mock_server import MockServer

REPO_ROOT = Path(__file__).resolve().parent.parent
SIMULATOR_BIN = REPO_ROOT / "screentinker_sim"


def parse_bmp_header(filepath: str):
    """Parses a BMP file and returns (width, height, bits_per_pixel, filesize)."""
    with open(filepath, "rb") as f:
        data = f.read(54)
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError("Invalid BMP signature")
    filesize, _, offset = struct.unpack("<III", data[2:14])
    _, width, height, planes, bpp = struct.unpack("<IiiHH", data[14:30])
    return {
        "filesize": filesize,
        "width": width,
        "height": height,
        "bpp": bpp,
        "data_offset": offset
    }


class TestScreenTinkerSimulator(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Ensure simulator binary exists or build it
        if not SIMULATOR_BIN.exists():
            print(f"[TestSetup] screentinker_sim binary not found at {SIMULATOR_BIN}. Building...")
            # Try make first, fallback to pio
            try:
                subprocess.run(["make", "-C", str(REPO_ROOT / "simulator")], check=True, capture_output=True)
            except Exception:
                subprocess.run(["pio", "run", "-e", "simulator"], check=True, cwd=str(REPO_ROOT), capture_output=True)

        if not SIMULATOR_BIN.exists():
            raise FileNotFoundError(f"Failed to find or build simulator binary at {SIMULATOR_BIN}")

        # Start mock server on dynamic ephemeral port
        cls.mock_server = MockServer(host="127.0.0.1", port=0, verbose=False)
        cls.port = cls.mock_server.start()
        cls.server_url = f"http://127.0.0.1:{cls.port}"

    @classmethod
    def tearDownClass(cls):
        cls.mock_server.stop()

    def setUp(self):
        # Reset mock server state before each test
        self.mock_server.state.reset()
        self.temp_dir = tempfile.TemporaryDirectory()

    def tearDown(self):
        self.temp_dir.cleanup()

    def run_simulator(self, extra_args: list, expect_success: bool = True) -> subprocess.CompletedProcess:
        """Helper to invoke screentinker_sim with standard test flags."""
        cmd = [
            str(SIMULATOR_BIN),
            "--headless",
            "--server", self.server_url,
        ]
        if not any(arg.startswith("--cycles") for arg in extra_args):
            cmd.append("--oneshot")
        cmd.extend(extra_args)

        res = subprocess.run(cmd, cwd=str(REPO_ROOT), capture_output=True, text=True, timeout=10)
        if expect_success:
            self.assertEqual(
                res.returncode, 0,
                f"Simulator exited with code {res.returncode}.\nSTDOUT:\n{res.stdout}\nSTDERR:\n{res.stderr}"
            )
        return res

    def test_01_normal_sync_200_and_bmp_snapshot(self):
        """Tests standard 200 OK frame download, BMP snapshot formatting, and telemetry."""
        snapshot_path = os.path.join(self.temp_dir.name, "snap_200.bmp")
        args = [
            "--token", "valid-test-bearer-token",
            "--device", "dev-uuid-test-01",
            "--battery", "84",
            "--power", "battery",
            "--rssi", "-64",
            "--snapshot", snapshot_path
        ]
        res = self.run_simulator(args)

        # 1. Assert Console Output
        self.assertIn("[Sync] Server response status: 200", res.stdout)
        self.assertIn("200 OK — Downloaded new frame (48000 bytes)", res.stdout)
        self.assertIn("[Simulator] One-shot run complete. Exiting.", res.stdout)

        # 2. Assert Snapshot File & BMP Header
        self.assertTrue(os.path.exists(snapshot_path), "Snapshot BMP was not generated")
        bmp_info = parse_bmp_header(snapshot_path)
        self.assertEqual(bmp_info["width"], 800)
        self.assertEqual(bmp_info["height"], 480)
        self.assertEqual(bmp_info["bpp"], 24)
        self.assertGreaterEqual(bmp_info["filesize"], 48054)

        # 3. Assert Telemetry Received by Mock Server
        last = self.mock_server.state.last_telemetry
        self.assertIsNotNone(last)
        headers = {k.lower(): v for k, v in last["headers"].items()}
        self.assertEqual(headers.get("x-st-firmware-version"), "1.2.0")
        self.assertEqual(headers.get("x-st-device-battery"), "84")
        self.assertEqual(headers.get("x-st-device-rssi"), "-64")
        self.assertEqual(headers.get("x-st-power-source"), "battery")
        self.assertEqual(headers.get("x-st-charging"), "0")
        self.assertEqual(headers.get("authorization"), "Bearer valid-test-bearer-token")

    def test_02_usb_power_and_charging_telemetry(self):
        """Tests USB power reporting and charging flag telemetry."""
        snapshot_path = os.path.join(self.temp_dir.name, "snap_usb.bmp")
        args = [
            "--token", "test-token",
            "--device", "dev-uuid-usb",
            "--usb",
            "--battery", "90",
            "--snapshot", snapshot_path
        ]
        self.run_simulator(args)

        last = self.mock_server.state.last_telemetry
        headers = {k.lower(): v for k, v in last["headers"].items()}
        self.assertEqual(headers.get("x-st-power-source"), "usb")
        self.assertEqual(headers.get("x-st-charging"), "1")

    def test_03_empty_playlist_screen_404(self):
        """Tests informative empty playlist diagnostic screen on HTTP 404."""
        self.mock_server.state.scenario = "empty_playlist"
        self.mock_server.state.device_name = "Reception Display"

        snapshot_path = os.path.join(self.temp_dir.name, "snap_404.bmp")
        args = [
            "--token", "test-token",
            "--device", "dev-empty-404",
            "--snapshot", snapshot_path
        ]
        res = self.run_simulator(args)

        self.assertIn("[Sync] Server response status: 404", res.stdout)
        self.assertIn("No Active Playlist", res.stdout)
        self.assertIn("Reception Display", res.stdout)
        self.assertTrue(os.path.exists(snapshot_path))
        bmp_info = parse_bmp_header(snapshot_path)
        self.assertEqual(bmp_info["width"], 800)
        self.assertEqual(bmp_info["height"], 480)

    def test_04_unsupported_content_screen_501(self):
        """Tests HTTP 501 Unsupported Content fallback diagnostic screen."""
        self.mock_server.state.scenario = "unsupported"

        snapshot_path = os.path.join(self.temp_dir.name, "snap_501.bmp")
        args = [
            "--token", "test-token",
            "--device", "dev-501",
            "--snapshot", snapshot_path
        ]
        res = self.run_simulator(args)

        self.assertIn("[Sync] Server response status: 501", res.stdout)
        self.assertIn("501 Unsupported Content", res.stdout)
        self.assertTrue(os.path.exists(snapshot_path))

    def test_05_remote_reboot_command(self):
        """Tests X-ST-Command: reboot reception and state reset."""
        self.mock_server.state.scenario = "reboot"

        args = [
            "--token", "test-token",
            "--device", "dev-reboot",
        ]
        res = self.run_simulator(args)

        self.assertIn("Remote command received from server: reboot", res.stdout)
        self.assertIn("Remote reboot executed!", res.stdout)

    def test_06_device_pairing_flow(self):
        """Tests automatic 6-digit PIN device registration and claiming flow."""
        config_path = os.path.join(self.temp_dir.name, "test_pair_cfg.json")
        # Create minimal initial config without deviceToken
        with open(config_path, "w") as f:
            json.dump({
                "server": {"url": self.server_url},
                "device": {"id": "unpaired-device-123", "token": ""},
                "simulator": {"headless": True, "pair": True}
            }, f)

        args = [
            "--config", config_path,
            "--pair",
            "--snapshot", os.path.join(self.temp_dir.name, "snap_pair.bmp")
        ]
        res = self.run_simulator(args)

        self.assertIn("STARTING AUTOMATIC 6-DIGIT DEVICE PAIRING FLOW", res.stdout)
        self.assertIn("Pairing PIN:  412309", res.stdout)
        self.assertIn("SUCCESS! Device claimed by user in ScreenTinker UI!", res.stdout)
        self.assertIn("Device Token: mock-device-token-123", res.stdout)

        # Verify token was saved to config file
        with open(config_path, "r") as f:
            saved_cfg = json.load(f)
        self.assertEqual(saved_cfg["server"]["device_token"], "mock-device-token-123")

    def test_07_connection_failure_exit_code(self):
        """Tests that unreachable server returns exit code 1 in oneshot mode."""
        args = [
            "--server", "http://127.0.0.1:59998",
            "--token", "test-token",
            "--device", "dev-unreachable",
        ]
        res = self.run_simulator(args, expect_success=False)
        self.assertEqual(res.returncode, 1, "Simulator must exit with code 1 on network failure")
        self.assertIn("Connection failed (code 0)", res.stderr + res.stdout)

    def test_08_etag_caching_304(self):
        """Tests conditional ETag caching and 304 Not Modified behavior over multiple cycles."""
        args = [
            "--token", "test-token",
            "--device", "dev-caching-test",
            "--cycles", "2",
        ]
        res = self.run_simulator(args)

        self.assertIn("Server response status: 200", res.stdout)
        self.assertIn("304 Not Modified", res.stdout)
        self.assertIn("Completed 2 poll cycle(s). Exiting.", res.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
