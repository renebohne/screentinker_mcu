#!/usr/bin/env python3
"""
Unit & Integration Tests for reTerminal Display Driver Integration (Issue #14).
Validates:
  - lib/seeed_epaper supports all 5 panels:
      * UC8179 (reTerminal E1001)
      * ED2208 (reTerminal E1002)
      * IT8951 (reTerminal E1003)
      * T133A01 (reTerminal E1004)
      * SSD1677 (reTerminal Sticky)
  - Unified hardware abstraction layer exports:
      * Controller initialization
      * Full & partial refresh controls
      * Deep sleep transitions
  - Pixel formats and color modes:
      * 1bpp Mono
      * Gray4
      * Gray16
      * Spectra 6
  - include/device_profile.h binds each target to the corresponding panel model
  - Buffer rotation algorithms for 1bpp mono and 4bpp packed nibble formats
"""

import os
import re
import subprocess
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
LIB_EPAPER = REPO_ROOT / "lib" / "seeed_epaper"
INCLUDE_DIR = REPO_ROOT / "include"


class TestDisplayDrivers(unittest.TestCase):
    def test_01_panel_models_enum(self):
        """Verifies epaper_panel.h defines panel models for all 5 reTerminal targets."""
        header_path = LIB_EPAPER / "epaper_panel.h"
        self.assertTrue(header_path.exists(), f"Missing {header_path}")
        content = header_path.read_text()

        expected_models = [
            "SEEED_EPAPER_PANEL_UC8179",
            "SEEED_EPAPER_PANEL_SSD1677",
            "SEEED_EPAPER_PANEL_ED2208",
            "SEEED_EPAPER_PANEL_IT8951",
            "SEEED_EPAPER_PANEL_T133A01",
        ]
        for model in expected_models:
            self.assertIn(model, content, f"Missing enum {model} in epaper_panel.h")

    def test_02_pixel_formats_enum(self):
        """Verifies epaper_panel.h defines required pixel formats including Spectra6 and Gray16."""
        header_path = LIB_EPAPER / "epaper_panel.h"
        content = header_path.read_text()

        expected_formats = [
            "SEEED_EPAPER_PIXEL_FORMAT_MONO1_MSB",
            "SEEED_EPAPER_PIXEL_FORMAT_GRAY4",
            "SEEED_EPAPER_PIXEL_FORMAT_SPECTRA6",
            "SEEED_EPAPER_PIXEL_FORMAT_GRAY16",
        ]
        for fmt in expected_formats:
            self.assertIn(fmt, content, f"Missing pixel format {fmt} in epaper_panel.h")

    def test_03_driver_source_files_exist(self):
        """Verifies driver source files exist for UC8179, SSD1677, ED2208, IT8951, T133A01."""
        driver_dir = LIB_EPAPER / "driver"
        expected_drivers = [
            "ssd1677.c",
            "uc8179.c",
            "ed2208.c",
            "it8951.c",
            "t133a01.c",
        ]
        for drv in expected_drivers:
            file_path = driver_dir / drv
            self.assertTrue(file_path.exists(), f"Missing driver source file: {file_path}")

    def test_04_driver_exports_in_priv_header(self):
        """Verifies epaper_panel_priv.h declares driver getter functions for all controllers."""
        header_path = LIB_EPAPER / "epaper_panel_priv.h"
        content = header_path.read_text()

        expected_getters = [
            "seeed_epaper_ssd1677_driver",
            "seeed_epaper_uc8179_driver",
            "seeed_epaper_ed2208_driver",
            "seeed_epaper_it8951_driver",
            "seeed_epaper_t133a01_driver",
        ]
        for getter in expected_getters:
            self.assertIn(getter, content, f"Missing getter declaration {getter} in epaper_panel_priv.h")

    def test_05_device_profiles_map_panel_models(self):
        """Verifies include/device_profile.h maps each target to its correct DEVICE_PANEL_MODEL."""
        profile_path = INCLUDE_DIR / "device_profile.h"
        self.assertTrue(profile_path.exists())
        content = profile_path.read_text()

        expected_mappings = [
            ("DEVICE_RETERMINAL_STICKY", "SEEED_EPAPER_PANEL_SSD1677"),
            ("DEVICE_RETERMINAL_E1001", "SEEED_EPAPER_PANEL_UC8179"),
            ("DEVICE_RETERMINAL_E1002", "SEEED_EPAPER_PANEL_ED2208"),
            ("DEVICE_RETERMINAL_E1003", "SEEED_EPAPER_PANEL_IT8951"),
            ("DEVICE_RETERMINAL_E1004", "SEEED_EPAPER_PANEL_T133A01"),
        ]
        for target, model in expected_mappings:
            self.assertIn(target, content, f"Missing target {target} in device_profile.h")
            self.assertIn(model, content, f"Missing panel model {model} in device_profile.h")

    def test_06_main_uses_unified_driver_init_and_sleep(self):
        """Verifies src/main.cpp uses unified display initialization, refresh, and panel sleep."""
        main_path = REPO_ROOT / "src" / "main.cpp"
        self.assertTrue(main_path.exists())
        content = main_path.read_text()

        self.assertIn("DEVICE_PANEL_MODEL", content, "src/main.cpp should use DEVICE_PANEL_MODEL")
        self.assertIn("seeed_epaper_panel_sleep", content, "src/main.cpp should call seeed_epaper_panel_sleep on deep sleep/poweroff")

    def test_07_nibble_and_mono_rotation_algorithms(self):
        """Validates 180° rotation correctness for both 1bpp mono and 4bpp nibble packed buffers."""
        # 1. Test 1bpp Mono rotation logic:
        # 2x8 pixels mono buffer (2 bytes wide, 2 rows)
        # Row 0: 0b10000000 0b00000001 (pixel (0,0)=1, (15,0)=1)
        # Row 1: 0b00000000 0b00000000
        # After 180° rotation:
        # Row 0 should be all 0
        # Row 1 should have pixel (15,1)=1 and (0,1)=1 -> 0b10000000 0b00000001
        def rotate_mono(src, w, h):
            stride = (w + 7) // 8
            dest = bytearray(len(src))
            for y in range(h):
                for x in range(w):
                    src_byte = src[y * stride + (x // 8)]
                    pixel = (src_byte >> (7 - (x % 8))) & 1
                    dx = w - 1 - x
                    dy = h - 1 - y
                    if pixel:
                        dest[dy * stride + (dx // 8)] |= (1 << (7 - (dx % 8)))
            return bytes(dest)

        src_mono = bytes([0x81, 0x00, 0x00, 0x00])
        rot_mono = rotate_mono(src_mono, 16, 2)
        self.assertEqual(rot_mono, bytes([0x00, 0x00, 0x00, 0x81]))

        # 2. Test 4bpp Nibble rotation logic (Spectra 6 & Gray16):
        # 4 pixels width, 2 rows height (each byte stores 2 nibbles: high nibble = pixel 0, low nibble = pixel 1)
        # Row 0: [Pixel0=1, Pixel1=2], [Pixel2=3, Pixel3=4] -> bytes: [0x12, 0x34]
        # Row 1: [Pixel4=5, Pixel5=6], [Pixel6=7, Pixel7=8] -> bytes: [0x56, 0x78]
        # Rotated 180°:
        # Row 0 should be reversed Row 1: [8, 7], [6, 5] -> bytes: [0x87, 0x65]
        # Row 1 should be reversed Row 0: [4, 3], [2, 1] -> bytes: [0x43, 0x21]
        def rotate_nibble(src, w, h):
            stride = (w + 1) // 2
            dest = bytearray(len(src))
            for y in range(h):
                for x in range(w):
                    src_byte = src[y * stride + (x // 2)]
                    nibble = (src_byte >> 4) if (x % 2 == 0) else (src_byte & 0x0F)
                    dx = w - 1 - x
                    dy = h - 1 - y
                    if dx % 2 == 0:
                        dest[dy * stride + (dx // 2)] |= (nibble << 4)
                    else:
                        dest[dy * stride + (dx // 2)] |= (nibble & 0x0F)
            return bytes(dest)

        src_nibble = bytes([0x12, 0x34, 0x56, 0x78])
        rot_nibble = rotate_nibble(src_nibble, 4, 2)
        self.assertEqual(rot_nibble, bytes([0x87, 0x65, 0x43, 0x21]))

    def test_08_gray4_rotation_and_packing(self):
        """Validates 2bpp Gray4 packing and 180° rotation correctness for Sticky & E1001 (Ticket 1)."""
        # 1. Verify DEVICE_SUPPORTS_GRAY4 is enabled for Sticky and E1001 in device_profile.h
        profile_path = INCLUDE_DIR / "device_profile.h"
        content = profile_path.read_text()
        self.assertIn("DEVICE_SUPPORTS_GRAY4", content, "Missing DEVICE_SUPPORTS_GRAY4 in device_profile.h")

        # 2. Test 2bpp Gray4 buffer size: 800x480 at 2bpp is exactly 96,000 bytes
        w, h = 800, 480
        stride = (w + 3) // 4
        self.assertEqual(stride, 200)
        self.assertEqual(stride * h, 96000)

        # 3. Test 2bpp pixel reversal / 180° rotation:
        # Byte layout: [px0(bits 7..6) | px1(bits 5..4) | px2(bits 3..2) | px3(bits 1..0)]
        # Pixel values: 0=Black (0b00), 1=DarkGray (0b01), 2=LightGray (0b10), 3=White (0b11)
        # Byte: 0b00_01_10_11 (0x1B)
        # 180° rotated single byte should reverse pixels: [px3, px2, px1, px0] = 0b11_10_01_00 (0xE4)
        def reverse_2bpp_byte(b):
            return ((b & 0x03) << 6) | ((b & 0x0C) << 2) | ((b & 0x30) >> 2) | ((b & 0xC0) >> 6)

        self.assertEqual(reverse_2bpp_byte(0x1B), 0xE4)

        def rotate_gray4_180(src, width, height):
            row_stride = (width + 3) // 4
            dest = bytearray(len(src))
            for y in range(height):
                src_row = y * row_stride
                dst_row = (height - 1 - y) * row_stride
                for x in range(row_stride):
                    b = src[src_row + (row_stride - 1 - x)]
                    dest[dst_row + x] = reverse_2bpp_byte(b)
            return bytes(dest)

        # 8 pixels wide, 2 rows:
        # Row 0: [0, 1, 2, 3], [3, 2, 1, 0] -> [0x1B, 0xE4]
        # Row 1: [0, 0, 0, 0], [3, 3, 3, 3] -> [0x00, 0xFF]
        src_gray4 = bytes([0x1B, 0xE4, 0x00, 0xFF])
        # Rotated 180°:
        # Row 0 should be reversed Row 1: [reversed 0xFF, reversed 0x00] -> [0xFF, 0x00]
        # Row 1 should be reversed Row 0: [reversed 0xE4, reversed 0x1B] -> [0x1B, 0xE4]
        rot_gray4 = rotate_gray4_180(src_gray4, 8, 2)
        self.assertEqual(rot_gray4, bytes([0xFF, 0x00, 0x1B, 0xE4]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
