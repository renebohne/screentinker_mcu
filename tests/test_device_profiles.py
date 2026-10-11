#!/usr/bin/env python3
"""
Unit & Integration Tests for Modular Device Profiles & Pinouts (Issue #12).
Validates:
  - include/pins_e1001.h
  - include/pins_e1002.h
  - include/pins_e1003.h
  - include/pins_e1004.h
  - include/device_profile.h
"""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
INCLUDE_DIR = REPO_ROOT / "include"


def run_cpp_test(code: str, define_flag: str = "") -> subprocess.CompletedProcess:
    """Compiles and executes a small C++ test snippet against include/."""
    with tempfile.TemporaryDirectory() as tmpdir:
        src_path = Path(tmpdir) / "test_prog.cpp"
        bin_path = Path(tmpdir) / "test_prog"

        with open(src_path, "w") as f:
            f.write(code)

        compiler_cmd = ["g++", "-I", str(INCLUDE_DIR), "-std=c++17", str(src_path), "-o", str(bin_path)]
        if define_flag:
            compiler_cmd.insert(1, define_flag)

        if not shutil.which("g++") and shutil.which("nix-shell"):
            cmd_str = " ".join(f'"{c}"' for c in compiler_cmd)
            exec_cmd = ["nix-shell", "--run", cmd_str]
        else:
            exec_cmd = compiler_cmd

        comp_res = subprocess.run(exec_cmd, capture_output=True, text=True, cwd=str(REPO_ROOT))
        if comp_res.returncode != 0:
            return comp_res

        run_res = subprocess.run([str(bin_path)], capture_output=True, text=True)
        return run_res


class TestDeviceProfiles(unittest.TestCase):
    def test_01_headers_exist(self):
        """Verifies that all required pinout and profile headers exist."""
        required_headers = [
            "pins_sticky.h",
            "pins_e1001.h",
            "pins_e1002.h",
            "pins_e1003.h",
            "pins_e1004.h",
            "device_profile.h",
        ]
        for header in required_headers:
            path = INCLUDE_DIR / header
            self.assertTrue(path.exists(), f"Required header missing: {header}")

    def test_02_default_profile_is_sticky(self):
        """Without any define flags, device_profile.h defaults to reTerminal Sticky."""
        code = r"""
        #include "device_profile.h"
        #include <cassert>
        #include <cstring>
        int main() {
            assert(DEVICE_DISPLAY_WIDTH == 800);
            assert(DEVICE_DISPLAY_HEIGHT == 480);
            assert(DEVICE_COLOR_MODE == COLOR_MODE_1BPP_MONO);
            assert(DEVICE_SUPPORTS_PARTIAL_REFRESH == true);
            assert(DEVICE_SUPPORTS_TOUCH == true);
            assert(PIN_EPD_SCK == 13);
            assert(PIN_EPD_MOSI == 14);
            assert(PIN_EPD_CS == 15);
            assert(PIN_EPD_DC == 16);
            assert(PIN_EPD_RST == 17);
            assert(PIN_EPD_BUSY == 18);
            assert(PIN_TOUCH_SDA == 3);
            assert(PIN_TOUCH_SCL == 2);
            assert(strcmp(DEVICE_PROFILE_NAME, "sticky") == 0);
            return 0;
        }
        """
        res = run_cpp_test(code)
        self.assertEqual(res.returncode, 0, f"Compilation or assertion failed:\n{res.stderr}\n{res.stdout}")

    def test_03_profile_e1001(self):
        """Tests DEVICE_RETERMINAL_E1001 profile defines and pinouts."""
        code = r"""
        #include "device_profile.h"
        #include <cassert>
        #include <cstring>
        int main() {
            assert(DEVICE_DISPLAY_WIDTH == 800);
            assert(DEVICE_DISPLAY_HEIGHT == 480);
            assert(DEVICE_COLOR_MODE == COLOR_MODE_1BPP_MONO);
            assert(DEVICE_SUPPORTS_PARTIAL_REFRESH == true);
            assert(DEVICE_SUPPORTS_TOUCH == false);
            assert(PIN_EPD_SCK == 7);
            assert(PIN_EPD_MOSI == 9);
            assert(PIN_EPD_CS == 10);
            assert(PIN_EPD_DC == 11);
            assert(PIN_EPD_RST == 12);
            assert(PIN_EPD_BUSY == 13);
            assert(PIN_LED == 6);
            assert(PIN_BUZZER == 45);
            assert(PIN_BATT_EN == 21);
            assert(strcmp(DEVICE_PROFILE_NAME, "e1001") == 0);
            return 0;
        }
        """
        res = run_cpp_test(code, "-DDEVICE_RETERMINAL_E1001")
        self.assertEqual(res.returncode, 0, f"Compilation or assertion failed:\n{res.stderr}\n{res.stdout}")

    def test_04_profile_e1002(self):
        """Tests DEVICE_RETERMINAL_E1002 profile defines and pinouts."""
        code = r"""
        #include "device_profile.h"
        #include <cassert>
        #include <cstring>
        int main() {
            assert(DEVICE_DISPLAY_WIDTH == 800);
            assert(DEVICE_DISPLAY_HEIGHT == 480);
            assert(DEVICE_COLOR_MODE == COLOR_MODE_SPECTRA6);
            assert(DEVICE_SUPPORTS_PARTIAL_REFRESH == false);
            assert(DEVICE_BUFFER_SIZE == 192000);
            assert(PIN_EPD_SCK == 7);
            assert(PIN_EPD_MOSI == 9);
            assert(PIN_EPD_CS == 10);
            assert(PIN_EPD_DC == 11);
            assert(PIN_EPD_RST == 12);
            assert(PIN_EPD_BUSY == 13);
            assert(PIN_LED == 6);
            assert(PIN_BUZZER == 45);
            assert(PIN_BATT_EN == 21);
            assert(strcmp(DEVICE_PROFILE_NAME, "e1002") == 0);
            return 0;
        }
        """
        res = run_cpp_test(code, "-DDEVICE_RETERMINAL_E1002")
        self.assertEqual(res.returncode, 0, f"Compilation or assertion failed:\n{res.stderr}\n{res.stdout}")

    def test_05_profile_e1003(self):
        """Tests DEVICE_RETERMINAL_E1003 profile defines and pinouts."""
        code = r"""
        #include "device_profile.h"
        #include <cassert>
        #include <cstring>
        int main() {
            assert(DEVICE_DISPLAY_WIDTH == 1872);
            assert(DEVICE_DISPLAY_HEIGHT == 1404);
            assert(DEVICE_COLOR_MODE == COLOR_MODE_GRAY16);
            assert(DEVICE_SUPPORTS_PARTIAL_REFRESH == true);
            assert(DEVICE_SUPPORTS_TOUCH == true);
            assert(DEVICE_BUFFER_SIZE == 1314144);
            assert(PIN_EPD_SCK == 7);
            assert(PIN_EPD_MISO == 8);
            assert(PIN_EPD_MOSI == 9);
            assert(PIN_EPD_CS == 10);
            assert(PIN_EPD_RST == 12);
            assert(PIN_EPD_BUSY == 13);
            assert(PIN_EPD_TFT_EN == 11);
            assert(PIN_EPD_ITE_EN == 21);
            assert(PIN_LED == 16);
            assert(PIN_BATT_EN == 40);
            assert(strcmp(DEVICE_PROFILE_NAME, "e1003") == 0);
            return 0;
        }
        """
        res = run_cpp_test(code, "-DDEVICE_RETERMINAL_E1003")
        self.assertEqual(res.returncode, 0, f"Compilation or assertion failed:\n{res.stderr}\n{res.stdout}")

    def test_06_profile_e1004(self):
        """Tests DEVICE_RETERMINAL_E1004 profile defines and pinouts."""
        code = r"""
        #include "device_profile.h"
        #include <cassert>
        #include <cstring>
        int main() {
            assert(DEVICE_DISPLAY_WIDTH == 1200);
            assert(DEVICE_DISPLAY_HEIGHT == 1600);
            assert(DEVICE_COLOR_MODE == COLOR_MODE_SPECTRA6);
            assert(DEVICE_SUPPORTS_PARTIAL_REFRESH == false);
            assert(DEVICE_BUFFER_SIZE == 960000);
            assert(PIN_EPD_SCK == 7);
            assert(PIN_EPD_MISO == 8);
            assert(PIN_EPD_MOSI == 9);
            assert(PIN_EPD_CS == 10);
            assert(PIN_EPD_DC == 11);
            assert(PIN_EPD_CS1 == 2);
            assert(PIN_EPD_RST == 38);
            assert(PIN_EPD_BUSY == 13);
            assert(PIN_EPD_ENABLE == 12);
            assert(PIN_LED == 48);
            assert(PIN_BATT_EN == 21);
            assert(strcmp(DEVICE_PROFILE_NAME, "e1004") == 0);
            return 0;
        }
        """
        res = run_cpp_test(code, "-DDEVICE_RETERMINAL_E1004")
        self.assertEqual(res.returncode, 0, f"Compilation or assertion failed:\n{res.stderr}\n{res.stdout}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
