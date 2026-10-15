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


def get_bmp_pixel(filepath: str, x: int, y: int):
    """Returns (R, G, B) tuple of pixel at (x, y) where (0,0) is top-left."""
    with open(filepath, "rb") as f:
        data = f.read()
    hdr = parse_bmp_header(filepath)
    w = hdr["width"]
    h = hdr["height"]
    offset = hdr["data_offset"]
    bpp = hdr["bpp"]
    if bpp != 24:
        raise ValueError(f"Expected 24 bpp BMP, got {bpp}")
    row_stride = ((w * 3 + 3) // 4) * 4
    file_y = h - 1 - y  # BMP rows are stored bottom-to-top
    px_offset = offset + file_y * row_stride + x * 3
    b = data[px_offset]
    g = data[px_offset + 1]
    r = data[px_offset + 2]
    return (r, g, b)


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

    def test_09_multi_device_profile_selection(self):
        """Tests that --model / --profile dynamically configures resolution and snapshot dimensions."""
        models = [
            ("sticky", 800, 480),
            ("e1001", 800, 480),
            ("e1002", 800, 480),
            ("e1003", 1872, 1404),
            ("e1004", 1200, 1600),
        ]
        for model_name, expected_w, expected_h in models:
            with self.subTest(model=model_name):
                # Inform mock server of expected dimensions for this model
                self.mock_server.state.screen_width = expected_w
                self.mock_server.state.screen_height = expected_h

                snap_path = os.path.join(self.temp_dir.name, f"snap_{model_name}.bmp")
                args = [
                    "--model", model_name,
                    "--token", "test-token",
                    "--device", f"dev-{model_name}",
                    "--snapshot", snap_path,
                ]
                res = self.run_simulator(args)

                self.assertTrue(os.path.exists(snap_path), f"Snapshot for {model_name} was not generated")
                bmp_info = parse_bmp_header(snap_path)
                self.assertEqual(bmp_info["width"], expected_w, f"Width mismatch for {model_name}")
                self.assertEqual(bmp_info["height"], expected_h, f"Height mismatch for {model_name}")

    def test_10_device_pairing_payload_for_models(self):
        """Tests that --pair with --model sends the correct screen_profile, width, and height."""
        models = [
            ("e1001", "seeed-reterminal-e1001", 800, 480),
            ("e1002", "seeed-reterminal-e1002", 800, 480),
            ("e1003", "seeed-reterminal-e1003", 1872, 1404),
            ("e1004", "seeed-reterminal-e1004", 1200, 1600),
            ("sticky", "seeed-reterminal-sticky", 800, 480),
        ]
        for model_name, exp_profile, exp_w, exp_h in models:
            with self.subTest(model=model_name):
                cfg_path = os.path.join(self.temp_dir.name, f"pair_cfg_{model_name}.json")
                with open(cfg_path, "w") as f:
                    json.dump({
                        "server": {"url": self.server_url},
                        "device": {"id": f"unpaired-{model_name}", "token": ""},
                        "simulator": {"headless": True, "pair": True}
                    }, f)

                args = [
                    "--model", model_name,
                    "--config", cfg_path,
                    "--pair",
                    "--snapshot", os.path.join(self.temp_dir.name, f"snap_pair_{model_name}.bmp")
                ]
                res = self.run_simulator(args)

                last_reg = self.mock_server.state.last_pair_register
                self.assertEqual(last_reg.get("screen_profile"), exp_profile, f"Profile mismatch for {model_name}")
                self.assertEqual(last_reg.get("screen_width"), exp_w, f"Width mismatch for {model_name}")
                self.assertEqual(last_reg.get("screen_height"), exp_h, f"Height mismatch for {model_name}")

    def test_11_spectra6_color_decoding_and_snapshot(self):
        """Tests that e1002 and e1004 unpack 4bpp Spectra 6 frames into true RGB colors."""
        spectra_models = [
            ("e1002", 800, 480),
            ("e1004", 1200, 1600),
        ]
        for model_name, w, h in spectra_models:
            with self.subTest(model=model_name):
                self.mock_server.state.screen_width = w
                self.mock_server.state.screen_height = h
                self.mock_server.state.color_mode = "spectra6"
                self.mock_server.state.frame_pattern = "spectra_stripes"

                snap_path = os.path.join(self.temp_dir.name, f"snap_{model_name}_color.bmp")
                args = [
                    "--model", model_name,
                    "--token", "test-token",
                    "--device", f"dev-{model_name}-color",
                    "--snapshot", snap_path,
                ]
                res = self.run_simulator(args)

                self.assertTrue(os.path.exists(snap_path), f"Snapshot for {model_name} not found")
                hdr = parse_bmp_header(snap_path)
                self.assertEqual(hdr["width"], w)
                self.assertEqual(hdr["height"], h)

                # Check stripe pixel colors:
                stripe_h = h // 4
                # Top stripe: Yellow (R > 200, G > 180, B < 60)
                r_y, g_y, b_y = get_bmp_pixel(snap_path, w // 2, stripe_h // 2)
                self.assertGreater(r_y, 200, f"Yellow R too low on {model_name}: ({r_y},{g_y},{b_y})")
                self.assertGreater(g_y, 180, f"Yellow G too low on {model_name}: ({r_y},{g_y},{b_y})")
                self.assertLess(b_y, 60, f"Yellow B too high on {model_name}: ({r_y},{g_y},{b_y})")

                # Second stripe: Red (R > 180, G < 60, B < 60)
                r_r, g_r, b_r = get_bmp_pixel(snap_path, w // 2, stripe_h + stripe_h // 2)
                self.assertGreater(r_r, 180, f"Red R too low on {model_name}: ({r_r},{g_r},{b_r})")
                self.assertLess(g_r, 60, f"Red G too high on {model_name}: ({r_r},{g_r},{b_r})")
                self.assertLess(b_r, 60, f"Red B too high on {model_name}: ({r_r},{g_r},{b_r})")

                # Third stripe: Blue (R < 60, G < 150, B > 150)
                r_b, g_b, b_b = get_bmp_pixel(snap_path, w // 2, stripe_h * 2 + stripe_h // 2)
                self.assertLess(r_b, 60, f"Blue R too high on {model_name}: ({r_b},{g_b},{b_b})")
                self.assertGreater(b_b, 150, f"Blue B too low on {model_name}: ({r_b},{g_b},{b_b})")

                # Fourth stripe: Green (R < 60, G > 100, B < 80)
                r_g, g_g, b_g = get_bmp_pixel(snap_path, w // 2, stripe_h * 3 + stripe_h // 2)
                self.assertLess(r_g, 60, f"Green R too high on {model_name}: ({r_g},{g_g},{b_g})")
                self.assertGreater(g_g, 100, f"Green G too low on {model_name}: ({r_g},{g_g},{b_g})")

    def test_12_grayscale_decoding_and_snapshot(self):
        """Tests that e1003 decodes Gray16 frames into proportional 16-level grayscale RGB."""
        w, h = 1872, 1404
        self.mock_server.state.screen_width = w
        self.mock_server.state.screen_height = h
        self.mock_server.state.color_mode = "gray16"
        self.mock_server.state.frame_pattern = "gray16_ramp"

        snap_path = os.path.join(self.temp_dir.name, "snap_e1003_gray.bmp")
        args = [
            "--model", "e1003",
            "--token", "test-token",
            "--device", "dev-e1003-gray",
            "--snapshot", snap_path,
        ]
        res = self.run_simulator(args)

        self.assertTrue(os.path.exists(snap_path))
        hdr = parse_bmp_header(snap_path)
        self.assertEqual(hdr["width"], w)
        self.assertEqual(hdr["height"], h)

        stripe_h = h // 4
        # Zone 0: Black
        r0, g0, b0 = get_bmp_pixel(snap_path, w // 2, stripe_h // 2)
        self.assertLess(r0, 35)
        self.assertLess(g0, 35)

        # Zone 1: Dark Gray (~85)
        r1, g1, b1 = get_bmp_pixel(snap_path, w // 2, stripe_h + stripe_h // 2)
        self.assertTrue(60 <= r1 <= 110, f"Zone 1 gray out of range: {r1}")
        self.assertEqual(r1, g1)
        self.assertEqual(r1, b1)

        # Zone 2: Light Gray (~170)
        r2, g2, b2 = get_bmp_pixel(snap_path, w // 2, stripe_h * 2 + stripe_h // 2)
        self.assertTrue(145 <= r2 <= 195, f"Zone 2 gray out of range: {r2}")
        self.assertEqual(r2, g2)
        self.assertEqual(r2, b2)

        # Zone 3: White
        r3, g3, b3 = get_bmp_pixel(snap_path, w // 2, stripe_h * 3 + stripe_h // 2)
        self.assertGreater(r3, 240)
        self.assertGreater(g3, 240)

    def test_13_hardware_capabilities_pairing_registration(self):
        """Tests that --pair announces color_mode, supports_partial_refresh, and refresh durations."""
        capabilities_matrix = [
            ("sticky", "mono", True, 1800),
            ("e1001", "mono", True, 3000),
            ("e1002", "spectra6", False, 14000),
            ("e1003", "gray16", True, 1200),
            ("e1004", "spectra6", False, 22000),
        ]
        for model_name, exp_color, exp_partial, exp_full_ms in capabilities_matrix:
            with self.subTest(model=model_name):
                cfg_path = os.path.join(self.temp_dir.name, f"pair_cap_{model_name}.json")
                with open(cfg_path, "w") as f:
                    json.dump({
                        "server": {"url": self.server_url},
                        "device": {"id": f"unpaired-cap-{model_name}", "token": ""},
                        "simulator": {"headless": True, "pair": True}
                    }, f)

                args = [
                    "--model", model_name,
                    "--config", cfg_path,
                    "--pair",
                    "--snapshot", os.path.join(self.temp_dir.name, f"snap_cap_{model_name}.bmp")
                ]
                res = self.run_simulator(args)

                last_reg = self.mock_server.state.last_pair_register
                self.assertEqual(last_reg.get("color_mode"), exp_color, f"Color mode mismatch for {model_name}")
                self.assertEqual(last_reg.get("supports_partial_refresh"), exp_partial, f"Partial refresh support mismatch for {model_name}")
                self.assertEqual(last_reg.get("full_refresh_time_ms"), exp_full_ms, f"Full refresh time mismatch for {model_name}")

    def test_14_partial_refresh_promotion_for_spectra6(self):
        """Tests that simulator promotes partial refresh to full refresh on Spectra 6 panels with notice."""
        self.mock_server.state.screen_width = 1200
        self.mock_server.state.screen_height = 1600
        self.mock_server.state.color_mode = "spectra6"
        self.mock_server.state.refresh_mode_header = "partial"

        args = [
            "--model", "e1004",
            "--token", "test-token",
            "--device", "dev-e1004-partial",
            "--snapshot", os.path.join(self.temp_dir.name, "snap_e1004_part.bmp"),
        ]
        res = self.run_simulator(args)

        self.assertIn("does not support partial refresh", res.stdout.lower(),
                      "Expected warning when requesting partial refresh on Spectra 6 panel")

    def test_15_responsive_setup_screens_multi_resolution(self):
        """Tests that pairing screens render crisp and centered across 800x480, 1872x1404, and 1200x1600."""
        target_resolutions = [
            ("sticky", 800, 480),
            ("e1003", 1872, 1404),
            ("e1004", 1200, 1600),
        ]
        for model_name, exp_w, exp_h in target_resolutions:
            with self.subTest(model=model_name):
                cfg_path = os.path.join(self.temp_dir.name, f"resp_{model_name}.json")
                with open(cfg_path, "w") as f:
                    json.dump({
                        "server": {"url": self.server_url},
                        "device": {"id": f"dev-resp-{model_name}", "token": ""},
                        "simulator": {"headless": True, "pair": True}
                    }, f)

                snap_path = os.path.join(self.temp_dir.name, f"snap_resp_{model_name}.bmp")
                args = [
                    "--model", model_name,
                    "--config", cfg_path,
                    "--pair",
                    "--snapshot", snap_path,
                ]
                res = self.run_simulator(args)

                self.assertTrue(os.path.exists(snap_path), f"Snapshot missing for {model_name}")
                hdr = parse_bmp_header(snap_path)
                self.assertEqual(hdr["width"], exp_w)
                self.assertEqual(hdr["height"], exp_h)

                # Verify screen contains both black (ink) and white (paper) pixels
                # Header row (y=10) should be within black header banner (ink < 35)
                r_hdr, g_hdr, b_hdr = get_bmp_pixel(snap_path, exp_w // 2, 10)
                self.assertLess(r_hdr, 35, f"Header pixel not black on {model_name}: ({r_hdr},{g_hdr},{b_hdr})")

                # Content area around margin should have white background (> 240)
                r_bg, g_bg, b_bg = get_bmp_pixel(snap_path, 10, 100)
                self.assertGreater(r_bg, 240, f"Background pixel not white on {model_name}: ({r_bg},{g_bg},{b_bg})")

    def test_16_sht40_temperature_and_humidity_telemetry(self):
        """Tests that simulator reports SHT40 temperature and humidity headers."""
        args = [
            "--token", "test-token",
            "--device", "dev-sht40-test",
            "--mock-temp", "24.5",
            "--mock-humidity", "55.0",
        ]
        res = self.run_simulator(args)
        self.assertIn("Ambient SHT40=24.5°C / 55% RH", res.stdout)

        last = self.mock_server.state.last_telemetry
        headers = {k.lower(): v for k, v in last["headers"].items()}
        self.assertEqual(headers.get("x-st-device-temperature"), "24.5")
        self.assertEqual(headers.get("x-st-device-humidity"), "55.0")

        # Test --no-sht40 disables sensor telemetry
        args_no = [
            "--token", "test-token",
            "--device", "dev-no-sht40-test",
            "--no-sht40",
        ]
        self.run_simulator(args_no)
        last_no = self.mock_server.state.last_telemetry
        headers_no = {k.lower(): v for k, v in last_no["headers"].items()}
        self.assertNotIn("x-st-device-temperature", headers_no)
        self.assertNotIn("x-st-device-humidity", headers_no)

    def test_17_remote_diagnostic_log_upload_command(self):
        """Tests that server command 'upload_logs' triggers POST /api/embedded/logs."""
        self.mock_server.state.scenario = "upload_logs"

        args = [
            "--token", "test-token",
            "--device", "dev-diag-logs",
        ]
        res = self.run_simulator(args)

        self.assertIn("Remote command received from server: upload_logs", res.stdout)
        self.assertIn("Uploading diagnostic logs to server...", res.stdout)
        self.assertIn("Logs successfully uploaded (HTTP 200)", res.stdout)

        last_logs = self.mock_server.state.last_logs_upload
        self.assertIsNotNone(last_logs, "Server did not receive uploaded diagnostic logs")
        self.assertEqual(last_logs.get("device_id"), "dev-diag-logs")
        self.assertIn("logs", last_logs)
        self.assertGreater(len(last_logs["logs"]), 0)

    def test_18_remote_ota_update_command(self):
        """Tests that server command 'update <url>' triggers simulated OTA update and splash."""
        # 1. Test hashless update command fails (M3 requirement)
        self.mock_server.state.scenario = "normal"
        self.mock_server.state.pending_cmd = "update /api/embedded/firmware/seeed-reterminal-sticky/latest.bin"
        args_no_hash = [
            "--token", "test-token",
            "--device", "dev-ota-test-fail",
        ]
        res_fail = self.run_simulator(args_no_hash)
        self.assertIn("Remote OTA update command received:", res_fail.stdout)
        self.assertIn("Simulated OTA update failed!", res_fail.stderr + res_fail.stdout)
        self.assertNotIn("Simulated OTA update complete.", res_fail.stdout)

        # 2. Test valid OTA update command with SHA-256 succeeds
        self.mock_server.state.scenario = "ota_update"
        self.mock_server.state.pending_cmd = None
        snapshot_path = os.path.join(self.temp_dir.name, "snap_ota.bmp")

        args = [
            "--token", "test-token",
            "--device", "dev-ota-test",
            "--snapshot", snapshot_path,
        ]
        res = self.run_simulator(args)

        self.assertIn("Remote OTA update command received:", res.stdout)
        self.assertIn("Simulated OTA update complete. Resetting state.", res.stdout)
        self.assertTrue(os.path.exists(snapshot_path))
        hdr = parse_bmp_header(snapshot_path)
        self.assertEqual(hdr["width"], 800)
        self.assertEqual(hdr["height"], 480)

    def test_19_rotation_and_orientation_server_updates(self):
        """Tests that simulator reacts to X-ST-Rotation and X-ST-Orientation response headers."""
        self.mock_server.state.orientation = "portrait"
        self.mock_server.state.rotation = 90

        snapshot_path = os.path.join(self.temp_dir.name, "snap_portrait_90.bmp")
        args = [
            "--token", "test-token",
            "--device", "dev-orient-test",
            "--snapshot", snapshot_path,
        ]
        res = self.run_simulator(args)
        self.assertEqual(res.returncode, 0)
        self.assertIn("Server updated dimensions to 480x800", res.stdout)
        self.assertTrue(os.path.exists(snapshot_path), "Portrait snapshot not generated")
        hdr = parse_bmp_header(snapshot_path)
        self.assertEqual(hdr["width"], 480)
        self.assertEqual(hdr["height"], 800)

        # Verify pixel colors: checkerboard pattern contains ink (< 35) and paper (> 240)
        r_white, g_white, b_white = get_bmp_pixel(snapshot_path, 0, 0)
        self.assertGreater(r_white, 240, f"Pixel (0,0) not white: {r_white}")
        r_black, g_black, b_black = get_bmp_pixel(snapshot_path, 1, 0)
        self.assertLess(r_black, 35, f"Pixel (1,0) not black: {r_black}")

    def test_20_low_battery_icon_rendering(self):
        """Tests that low battery (< 10%) on battery power triggers warning log and renders icon badge."""
        self.mock_server.state.frame_pattern = "solid_white"

        # Case 1: Low battery (< 10%) discharging on battery power -> should warn and draw badge
        snap_low = os.path.join(self.temp_dir.name, "snap_low_batt.bmp")
        args_low = [
            "--token", "test-token",
            "--device", "dev-low-batt",
            "--battery", "8",
            "--power", "battery",
            "--snapshot", snap_low,
        ]
        res_low = self.run_simulator(args_low)
        self.assertIn("[Power] Low battery warning (8%)!", res_low.stdout)
        self.assertTrue(os.path.exists(snap_low))
        # Badge is at x = width - 58 = 742, y = 14, size 44x22.
        # Inside the solid black box at (743, 15), pixel should be black (< 35)
        r_box, g_box, b_box = get_bmp_pixel(snap_low, 743, 15)
        self.assertLess(r_box, 35, f"Expected black badge pixel at (743, 15), got {r_box}")
        # Outside badge area at (700, 15), pixel should remain solid white (> 240)
        r_bg, g_bg, b_bg = get_bmp_pixel(snap_low, 700, 15)
        self.assertGreater(r_bg, 240, f"Expected white background at (700, 15), got {r_bg}")

        # Case 2: Battery at 8% but plugged into USB power -> no warning, no badge
        snap_usb = os.path.join(self.temp_dir.name, "snap_usb_batt.bmp")
        args_usb = [
            "--token", "test-token",
            "--device", "dev-usb-batt",
            "--battery", "8",
            "--usb",
            "--snapshot", snap_usb,
        ]
        res_usb = self.run_simulator(args_usb)
        self.assertNotIn("Low battery warning", res_usb.stdout)
        self.assertTrue(os.path.exists(snap_usb))
        r_nobadge, g_nobadge, b_nobadge = get_bmp_pixel(snap_usb, 743, 15)
        self.assertGreater(r_nobadge, 240, f"Expected white at (743, 15) when on USB, got {r_nobadge}")

        # Case 3: Battery at 50% on battery power -> no warning, no badge
        snap_normal = os.path.join(self.temp_dir.name, "snap_normal_batt.bmp")
        args_normal = [
            "--token", "test-token",
            "--device", "dev-normal-batt",
            "--battery", "50",
            "--power", "battery",
            "--snapshot", snap_normal,
        ]
        res_normal = self.run_simulator(args_normal)
        self.assertNotIn("Low battery warning", res_normal.stdout)
        self.assertTrue(os.path.exists(snap_normal))
        r_norm, g_norm, b_norm = get_bmp_pixel(snap_normal, 743, 15)
        self.assertGreater(r_norm, 240, f"Expected white at (743, 15) with normal battery, got {r_norm}")


if __name__ == "__main__":
    unittest.main(verbosity=2)

