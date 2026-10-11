#!/usr/bin/env python3
"""
Unit & Integration Tests for PlatformIO Build Environments (Issue #13).
Validates:
  - platformio.ini declares all required reTerminal E-Series environments
  - default_envs remains seeed_reterminal_sticky
  - Each environment defines its model flag and PSRAM configuration
  - PlatformIO parses the environments without configuration errors
"""

import configparser
import subprocess
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
PIO_INI = REPO_ROOT / "platformio.ini"


class TestPlatformIOEnvironments(unittest.TestCase):
    def setUp(self):
        self.config = configparser.ConfigParser()
        self.config.read(PIO_INI)

    def test_01_default_env_preserved(self):
        """Validates that default_envs is set to seeed_reterminal_sticky."""
        self.assertTrue(self.config.has_section("platformio"), "platformio section missing")
        default_envs = self.config.get("platformio", "default_envs", fallback="")
        self.assertEqual(default_envs.strip(), "seeed_reterminal_sticky")

    def test_02_all_environments_declared(self):
        """Validates that envs for e1001, e1002, e1003, and e1004 are present."""
        expected_envs = [
            "env:seeed_reterminal_sticky",
            "env:seeed_reterminal_e1001",
            "env:seeed_reterminal_e1002",
            "env:seeed_reterminal_e1003",
            "env:seeed_reterminal_e1004",
        ]
        for env in expected_envs:
            self.assertTrue(self.config.has_section(env), f"Missing PlatformIO environment section: [{env}]")

    def test_03_environment_flags_and_psram(self):
        """Validates that each environment has BOARD_HAS_PSRAM and correct DEVICE_RETERMINAL_* flag."""
        env_flag_map = {
            "env:seeed_reterminal_sticky": "DEVICE_RETERMINAL_STICKY",
            "env:seeed_reterminal_e1001": "DEVICE_RETERMINAL_E1001",
            "env:seeed_reterminal_e1002": "DEVICE_RETERMINAL_E1002",
            "env:seeed_reterminal_e1003": "DEVICE_RETERMINAL_E1003",
            "env:seeed_reterminal_e1004": "DEVICE_RETERMINAL_E1004",
        }
        for env, exp_flag in env_flag_map.items():
            build_flags = self.config.get(env, "build_flags", fallback="")
            self.assertIn("-DBOARD_HAS_PSRAM", build_flags, f"Missing -DBOARD_HAS_PSRAM in [{env}]")
            self.assertIn(f"-D{exp_flag}", build_flags, f"Missing -D{exp_flag} in [{env}]")

    def test_04_firmware_compilation_all_targets(self):
        """Validates that each environment can be compiled by PlatformIO."""
        environments = [
            "seeed_reterminal_sticky",
            "seeed_reterminal_e1001",
            "seeed_reterminal_e1002",
            "seeed_reterminal_e1003",
            "seeed_reterminal_e1004",
        ]
        for env_name in environments:
            with self.subTest(env=env_name):
                cmd = ["pio", "run", "-e", env_name]
                # If running on NixOS where pio is in nix-shell
                import shutil
                if not shutil.which("pio") and shutil.which("nix-shell"):
                    cmd = ["nix-shell", "--run", f"pio run -e {env_name}"]
                res = subprocess.run(cmd, capture_output=True, text=True, cwd=str(REPO_ROOT))
                self.assertEqual(res.returncode, 0, f"Compilation failed for [{env_name}]:\n{res.stderr}\n{res.stdout}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
