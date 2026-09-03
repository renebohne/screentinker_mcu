#include "setup_screen.h"
#include <string.h>

// 8x8 Basic ASCII Font (characters 32 to 126)
const uint8_t font8x8_basic[95][8] = {
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 32 (space)
  {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // 33 !
  {0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00}, // 34 "
  {0x6C, 0x6C, 0xFE, 0x6C, 0xFE, 0x6C, 0x6C, 0x00}, // 35 #
  {0x18, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x18, 0x00}, // 36 $
  {0x00, 0x66, 0xA6, 0xD4, 0x28, 0x65, 0x66, 0x00}, // 37 %
  {0x38, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0x76, 0x00}, // 38 &
  {0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00}, // 39 '
  {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00}, // 40 (
  {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00}, // 41 )
  {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // 42 *
  {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00}, // 43 +
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30}, // 44 ,
  {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00}, // 45 -
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00}, // 46 .
  {0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x80, 0x00}, // 47 /
  {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00}, // 48 0
  {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // 49 1
  {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00}, // 50 2
  {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00}, // 51 3
  {0x0C, 0x1C, 0x3C, 0x6C, 0xFE, 0x0C, 0x0C, 0x00}, // 52 4
  {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00}, // 53 5
  {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00}, // 54 6
  {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00}, // 55 7
  {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00}, // 56 8
  {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00}, // 57 9
  {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, // 58 :
  {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x30, 0x00}, // 59 ;
  {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // 60 <
  {0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00}, // 61 =
  {0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00}, // 62 >
  {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00}, // 63 ?
  {0x3C, 0x66, 0x6E, 0x6E, 0x60, 0x62, 0x3C, 0x00}, // 64 @
  {0x18, 0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x00}, // 65 A
  {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00}, // 66 B
  {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00}, // 67 C
  {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00}, // 68 D
  {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00}, // 69 E
  {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00}, // 70 F
  {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00}, // 71 G
  {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00}, // 72 H
  {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 73 I
  {0x0E, 0x06, 0x06, 0x06, 0x06, 0x66, 0x3C, 0x00}, // 74 J
  {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00}, // 75 K
  {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00}, // 76 L
  {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, // 77 M
  {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00}, // 78 N
  {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // 79 O
  {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00}, // 80 P
  {0x3C, 0x66, 0x66, 0x66, 0x6E, 0x3C, 0x0E, 0x00}, // 81 Q
  {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00}, // 82 R
  {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00}, // 83 S
  {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // 84 T
  {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // 85 U
  {0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, // 86 V
  {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // 87 W
  {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00}, // 88 X
  {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00}, // 89 Y
  {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00}, // 90 Z
  {0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00}, // 91 [
  {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x02, 0x00}, // 92 backslash
  {0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00}, // 93 ]
  {0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00}, // 94 ^
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // 95 _
  {0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00}, // 96 `
  {0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00}, // 97 a
  {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00}, // 98 b
  {0x00, 0x00, 0x3C, 0x66, 0x60, 0x66, 0x3C, 0x00}, // 99 c
  {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00}, // 100 d
  {0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00}, // 101 e
  {0x0E, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x00}, // 102 f
  {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x7C}, // 103 g
  {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00}, // 104 h
  {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 105 i
  {0x06, 0x00, 0x0E, 0x06, 0x06, 0x66, 0x3C, 0x00}, // 106 j
  {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00}, // 107 k
  {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00}, // 108 l
  {0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // 109 m
  {0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00}, // 110 n
  {0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00}, // 111 o
  {0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60}, // 112 p
  {0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06}, // 113 q
  {0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00}, // 114 r
  {0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00}, // 115 s
  {0x18, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x0E, 0x00}, // 116 t
  {0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00}, // 117 u
  {0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00}, // 118 v
  {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x3E, 0x36, 0x00}, // 119 w
  {0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00}, // 120 x
  {0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x7C}, // 121 y
  {0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00}, // 122 z
  {0x0E, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0E, 0x00}, // 123 {
  {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // 124 |
  {0x70, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x70, 0x00}, // 125 }
  {0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 126 ~
};

// ─── Graphics Primitives for 800x480 1-Bit Monochrom ─────────────────────────
// 1 = White (0xFF), 0 = Black (0x00)

void gfx_fill(uint8_t* buf, uint8_t color) {
  memset(buf, color ? 0xFF : 0x00, EPD_BUFFER_SIZE);
}

void gfx_draw_pixel(uint8_t* buf, int x, int y, uint8_t color) {
  if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) return;
  size_t byte_idx = ((size_t)y * (EPD_WIDTH / 8)) + (x / 8);
  uint8_t bit_mask = 0x80 >> (x & 7);
  if (color) {
    buf[byte_idx] |= bit_mask;  // White
  } else {
    buf[byte_idx] &= ~bit_mask; // Black
  }
}

void gfx_draw_hline(uint8_t* buf, int x, int y, int w, uint8_t color) {
  for (int i = 0; i < w; i++) {
    gfx_draw_pixel(buf, x + i, y, color);
  }
}

void gfx_draw_vline(uint8_t* buf, int x, int y, int h, uint8_t color) {
  for (int i = 0; i < h; i++) {
    gfx_draw_pixel(buf, x, y + i, color);
  }
}

void gfx_draw_rect(uint8_t* buf, int x, int y, int w, int h, uint8_t color) {
  gfx_draw_hline(buf, x, y, w, color);
  gfx_draw_hline(buf, x, y + h - 1, w, color);
  gfx_draw_vline(buf, x, y, h, color);
  gfx_draw_vline(buf, x + w - 1, y, h, color);
}

void gfx_fill_rect(uint8_t* buf, int x, int y, int w, int h, uint8_t color) {
  for (int j = 0; j < h; j++) {
    gfx_draw_hline(buf, x, y + j, w, color);
  }
}

void gfx_draw_char(uint8_t* buf, int x, int y, char c, uint8_t color, int scale) {
  if (c < 32 || c > 126) c = ' ';
  const uint8_t* char_data = font8x8_basic[c - 32];

  for (int row = 0; row < 8; row++) {
    uint8_t b = char_data[row];
    for (int col = 0; col < 8; col++) {
      if ((b >> (7 - col)) & 0x01) {
        if (scale == 1) {
          gfx_draw_pixel(buf, x + col, y + row, color);
        } else {
          gfx_fill_rect(buf, x + col * scale, y + row * scale, scale, scale, color);
        }
      }
    }
  }
}

void gfx_draw_string(uint8_t* buf, int x, int y, const char* str, uint8_t color, int scale) {
  int cur_x = x;
  int cur_y = y;
  while (*str) {
    if (*str == '\n') {
      cur_x = x;
      cur_y += 10 * scale;
    } else {
      gfx_draw_char(buf, cur_x, cur_y, *str, color, scale);
      cur_x += 8 * scale;
    }
    str++;
  }
}

// ─── Onboarding / Instruction Screen (Optimized for 800x480 E-Paper) ──────────
void renderOnboardingScreen(uint8_t* buffer, Language lang) {
  // 1. Fill entire screen white
  gfx_fill(buffer, 1);

  // 2. Outer decorative frame
  gfx_draw_rect(buffer, 12, 12, EPD_WIDTH - 24, EPD_HEIGHT - 24, 0);
  gfx_draw_rect(buffer, 15, 15, EPD_WIDTH - 30, EPD_HEIGHT - 30, 0);

  // 3. Header Banner (Black background with white text)
  gfx_fill_rect(buffer, 18, 18, EPD_WIDTH - 36, 62, 0);
  gfx_draw_string(buffer, 35, 32, "ScreenTinker", 1, 3);
  gfx_draw_string(buffer, 480, 40, "reTerminal Sticky", 1, 2);

  if (lang == LANG_DE) {
    // 4. Status badge (German)
    gfx_draw_rect(buffer, 35, 92, EPD_WIDTH - 70, 42, 0);
    gfx_draw_string(buffer, 48, 100, "[!] Geraet bereit zur Einrichtung (Kein WLAN hinterlegt)", 0, 1);
    gfx_draw_string(buffer, 48, 116, "    [ UP: Deutsch | DOWN: English ]", 0, 1);

    // 5. Instruction Steps Heading
    gfx_draw_string(buffer, 35, 148, "SCHRITTE ZUR EINRICHTUNG:", 0, 2);
    gfx_draw_hline(buffer, 35, 168, EPD_WIDTH - 70, 0);

    // Step 1: Smartphone Hotspot
    gfx_fill_rect(buffer, 35, 180, 26, 26, 0);
    gfx_draw_string(buffer, 43, 185, "A", 1, 2);
    gfx_draw_string(buffer, 70, 180, "Option A: Smartphone / WLAN-Hotspot", 0, 2);
    gfx_draw_string(buffer, 70, 200, "Mit WLAN 'ScreenTinker-Setup' verbinden -> Browser: http://192.168.4.1", 0, 1);

    // Step 2: USB Web-Flasher
    gfx_fill_rect(buffer, 35, 222, 26, 26, 0);
    gfx_draw_string(buffer, 43, 227, "B", 1, 2);
    gfx_draw_string(buffer, 70, 222, "Option B: USB-C & Web-Installer", 0, 2);
    gfx_draw_string(buffer, 70, 242, "Per USB verbinden -> tools/sticky-installer.html in Chrome/Edge oeffnen", 0, 1);

    // Step 3: ScreenTinker Pairing
    gfx_fill_rect(buffer, 35, 264, 26, 26, 0);
    gfx_draw_string(buffer, 43, 269, "C", 1, 2);
    gfx_draw_string(buffer, 70, 264, "Kopplung: 6-stelligen Code eingeben", 0, 2);
    gfx_draw_string(buffer, 70, 284, "Im ScreenTinker Dashboard auf '+ Add Display' klicken & Code eingeben.", 0, 1);

    // 6. Footer Information Box
    gfx_fill_rect(buffer, 35, 316, EPD_WIDTH - 70, 134, 0);
    gfx_draw_string(buffer, 48, 328, "TASTEN-FUNKTIONEN / BUTTONS:", 1, 1);
    gfx_draw_string(buffer, 48, 348, "* UP / DOWN Tasten         : Sprache wechseln [ Deutsch / English ]", 1, 1);
    gfx_draw_string(buffer, 48, 368, "* OK-Taste (kurzer Druck)  : Bildschirm aktualisieren / Status abfragen", 1, 1);
    gfx_draw_string(buffer, 48, 388, "* OK-Taste (5 Sek. halten) : Factory Reset / NVS-Konfiguration loeschen", 1, 1);
    gfx_draw_string(buffer, 48, 408, "* WebSerial / CLI          : JSON-Befehle (config, status, cache, reset, next, prev)", 1, 1);
    gfx_draw_string(buffer, 48, 428, "* Energie-Management       : Automatischer WLAN-Ruhezustand aktiv", 1, 1);
  } else {
    // 4. Status badge (English - Default)
    gfx_draw_rect(buffer, 35, 92, EPD_WIDTH - 70, 42, 0);
    gfx_draw_string(buffer, 48, 100, "[!] Device ready for setup (No Wi-Fi credentials configured)", 0, 1);
    gfx_draw_string(buffer, 48, 116, "    [ DOWN: English | UP: Deutsch ]", 0, 1);

    // 5. Instruction Steps Heading
    gfx_draw_string(buffer, 35, 148, "SETUP INSTRUCTIONS:", 0, 2);
    gfx_draw_hline(buffer, 35, 168, EPD_WIDTH - 70, 0);

    // Step 1: Smartphone Hotspot
    gfx_fill_rect(buffer, 35, 180, 26, 26, 0);
    gfx_draw_string(buffer, 43, 185, "A", 1, 2);
    gfx_draw_string(buffer, 70, 180, "Option A: Smartphone / Wi-Fi Hotspot", 0, 2);
    gfx_draw_string(buffer, 70, 200, "Connect to Wi-Fi 'ScreenTinker-Setup' -> Browser: http://192.168.4.1", 0, 1);

    // Step 2: USB Web-Flasher
    gfx_fill_rect(buffer, 35, 222, 26, 26, 0);
    gfx_draw_string(buffer, 43, 227, "B", 1, 2);
    gfx_draw_string(buffer, 70, 222, "Option B: USB-C & Web-Installer", 0, 2);
    gfx_draw_string(buffer, 70, 242, "Connect via USB -> Open tools/sticky-installer.html in Chrome/Edge", 0, 1);

    // Step 3: ScreenTinker Pairing
    gfx_fill_rect(buffer, 35, 264, 26, 26, 0);
    gfx_draw_string(buffer, 43, 269, "C", 1, 2);
    gfx_draw_string(buffer, 70, 264, "Pairing: Enter 6-digit code in Dashboard", 0, 2);
    gfx_draw_string(buffer, 70, 284, "In ScreenTinker Dashboard, click '+ Add Display' and enter the on-screen code.", 0, 1);

    // 6. Footer Information Box
    gfx_fill_rect(buffer, 35, 316, EPD_WIDTH - 70, 134, 0);
    gfx_draw_string(buffer, 48, 328, "BUTTON FUNCTIONS:", 1, 1);
    gfx_draw_string(buffer, 48, 348, "* UP / DOWN Buttons        : Switch Language [ English / Deutsch ]", 1, 1);
    gfx_draw_string(buffer, 48, 368, "* OK Button (Short press)  : Refresh display / check status", 1, 1);
    gfx_draw_string(buffer, 48, 388, "* OK Button (Hold 5 sec)   : Factory Reset / wipe NVS configuration", 1, 1);
    gfx_draw_string(buffer, 48, 408, "* WebSerial / CLI          : JSON commands (config, status, cache, reset, next, prev)", 1, 1);
    gfx_draw_string(buffer, 48, 428, "* Power Management         : Automatic Wi-Fi sleep enabled", 1, 1);
  }
}

// ─── Offline Badge for Cache-Rotation Overlay (Upright buffer coordinates) ─────
void gfx_draw_offline_badge(uint8_t* buffer) {
  // Top-Right corner badge (x: 642 to 780, y: 14 to 44)
  gfx_fill_rect(buffer, 642, 14, 142, 30, 0); // Black box
  gfx_draw_rect(buffer, 644, 16, 138, 26, 1); // White inner outline

  // Exclamation mark icon in white box
  gfx_fill_rect(buffer, 648, 19, 18, 18, 1);
  gfx_draw_string(buffer, 654, 20, "!", 0, 2);

  // Text label
  gfx_draw_string(buffer, 672, 24, "NO WI-FI", 1, 1);
}

// ─── Prominent "No Wi-Fi Connection" Screen (When Cache is Empty) ──────────────
void renderNoWifiScreen(uint8_t* buffer, const char* ssid, Language lang) {
  // 1. Fill screen white
  gfx_fill(buffer, 1);

  // 2. Outer double border
  gfx_draw_rect(buffer, 12, 12, EPD_WIDTH - 24, EPD_HEIGHT - 24, 0);
  gfx_draw_rect(buffer, 15, 15, EPD_WIDTH - 30, EPD_HEIGHT - 30, 0);

  // 3. Header Banner
  gfx_fill_rect(buffer, 18, 18, EPD_WIDTH - 36, 62, 0);
  gfx_draw_string(buffer, 35, 32, "ScreenTinker", 1, 3);
  gfx_draw_string(buffer, 480, 40, "reTerminal Sticky", 1, 2);

  if (lang == LANG_DE) {
    // 4. Alert Box (Inverted black)
    gfx_fill_rect(buffer, 35, 90, EPD_WIDTH - 70, 48, 0);
    gfx_draw_string(buffer, 48, 104, "[!] KEINE WLAN-VERBINDUNG (OFFLINE)", 1, 2);

    // 5. Details Box
    gfx_draw_rect(buffer, 35, 148, EPD_WIDTH - 70, 48, 0);
    gfx_draw_string(buffer, 48, 156, "Konfigurierte SSID: ", 0, 1);
    gfx_draw_string(buffer, 215, 156, (ssid && strlen(ssid) > 0) ? ssid : "(Keine SSID)", 0, 1);
    gfx_draw_string(buffer, 48, 174, "Status: Verbindungsversuch fehlgeschlagen (Timeout)", 0, 1);

    // 6. Troubleshooting steps
    gfx_draw_string(buffer, 35, 208, "MOEGLICHE URSACHEN & SCHRITTE:", 0, 2);
    gfx_draw_hline(buffer, 35, 228, EPD_WIDTH - 70, 0);
    gfx_draw_string(buffer, 48, 238, "1. WLAN-Router / Access Point und Signalstaerke pruefen", 0, 1);
    gfx_draw_string(buffer, 48, 256, "2. Pruefen, ob WLAN-Name (SSID) und Passwort korrekt sind", 0, 1);
    gfx_draw_string(buffer, 48, 274, "3. Sicherstellen, dass der ScreenTinker Server erreichbar ist", 0, 1);
    gfx_draw_string(buffer, 48, 292, "4. Zum Neukonfigurieren: per USB verbinden und Web-Flasher oeffnen", 0, 1);

    // 7. Footer Actions
    gfx_fill_rect(buffer, 35, 320, EPD_WIDTH - 70, 130, 0);
    gfx_draw_string(buffer, 48, 332, "AKTIONEN / TASTEN:", 1, 1);
    gfx_draw_string(buffer, 48, 352, "* OK-Taste (kurz)          : Sofort erneut versuchen zu verbinden", 1, 1);
    gfx_draw_string(buffer, 48, 372, "* OK-Taste (5 Sek. halten) : Factory Reset / NVS-Konfiguration loeschen", 1, 1);
    gfx_draw_string(buffer, 48, 392, "* UP / DOWN Tasten         : Sprache umschalten [ Deutsch / English ]", 1, 1);
    gfx_draw_string(buffer, 48, 412, "* Automatischer Retry      : Neuer Verbindungsversuch alle 30 Sekunden", 1, 1);
    gfx_draw_string(buffer, 48, 430, "* WebSerial / CLI          : USB-Befehle (config, status, reset)", 1, 1);
  } else {
    // 4. Alert Box (Inverted black)
    gfx_fill_rect(buffer, 35, 90, EPD_WIDTH - 70, 48, 0);
    gfx_draw_string(buffer, 48, 104, "[!] NO WI-FI CONNECTION (OFFLINE)", 1, 2);

    // 5. Details Box
    gfx_draw_rect(buffer, 35, 148, EPD_WIDTH - 70, 48, 0);
    gfx_draw_string(buffer, 48, 156, "Configured SSID: ", 0, 1);
    gfx_draw_string(buffer, 195, 156, (ssid && strlen(ssid) > 0) ? ssid : "(None)", 0, 1);
    gfx_draw_string(buffer, 48, 174, "Status: Connection attempt timed out / Host unreachable", 0, 1);

    // 6. Troubleshooting steps
    gfx_draw_string(buffer, 35, 208, "TROUBLESHOOTING & STEPS:", 0, 2);
    gfx_draw_hline(buffer, 35, 228, EPD_WIDTH - 70, 0);
    gfx_draw_string(buffer, 48, 238, "1. Check your Wi-Fi router / Access Point and signal range", 0, 1);
    gfx_draw_string(buffer, 48, 256, "2. Verify that Wi-Fi SSID and password are correct", 0, 1);
    gfx_draw_string(buffer, 48, 274, "3. Ensure ScreenTinker Server is online and reachable", 0, 1);
    gfx_draw_string(buffer, 48, 292, "4. To reconfigure: Connect via USB-C and open Web-Flasher", 0, 1);

    // 7. Footer Actions
    gfx_fill_rect(buffer, 35, 320, EPD_WIDTH - 70, 130, 0);
    gfx_draw_string(buffer, 48, 332, "ACTIONS & BUTTONS:", 1, 1);
    gfx_draw_string(buffer, 48, 352, "* OK Button (Short press)  : Retry connecting to Wi-Fi now", 1, 1);
    gfx_draw_string(buffer, 48, 372, "* OK Button (Hold 5 sec)   : Factory Reset / Wipe NVS configuration", 1, 1);
    gfx_draw_string(buffer, 48, 392, "* UP / DOWN Buttons        : Switch Language [ English / Deutsch ]", 1, 1);
    gfx_draw_string(buffer, 48, 412, "* Automatic Retry          : Background reconnection attempt every 30s", 1, 1);
    gfx_draw_string(buffer, 48, 430, "* WebSerial / CLI          : USB commands (config, status, reset)", 1, 1);
  }
}

// ─── Big 6-Digit Pairing Code Screen ──────────────────────────────────────────
void renderPairingCodeScreen(uint8_t* buffer, const char* code, Language lang) {
  // 1. Fill screen white
  gfx_fill(buffer, 1);

  // 2. Outer double border
  gfx_draw_rect(buffer, 12, 12, EPD_WIDTH - 24, EPD_HEIGHT - 24, 0);
  gfx_draw_rect(buffer, 15, 15, EPD_WIDTH - 30, EPD_HEIGHT - 30, 0);

  // 3. Header Banner
  gfx_fill_rect(buffer, 18, 18, EPD_WIDTH - 36, 52, 0);
  gfx_draw_string(buffer, 35, 28, "ScreenTinker", 1, 3);
  gfx_draw_string(buffer, 480, 34, "reTerminal Sticky", 1, 2);

  // 4. Subtitle / Pairing Mode Indicator
  gfx_draw_rect(buffer, 35, 78, EPD_WIDTH - 70, 32, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 48, 88, "GERAET BEREIT ZUM KOPPELN — KOPPLUNGSCODE:", 0, 1);
    gfx_draw_string(buffer, 540, 88, "[ DOWN: EN | UP: DE ]", 0, 1);
  } else {
    gfx_draw_string(buffer, 48, 88, "READY TO PAIR — 6-DIGIT PAIRING CODE:", 0, 1);
    gfx_draw_string(buffer, 540, 88, "[ DOWN: EN | UP: DE ]", 0, 1);
  }

  // 5. Big Center Code Box (Black with huge white digits)
  gfx_fill_rect(buffer, 150, 118, 500, 110, 0);
  gfx_draw_rect(buffer, 154, 122, 492, 102, 1);

  // Format code with space in middle: e.g. "482 915"
  char formattedCode[16];
  if (code && strlen(code) == 6) {
    snprintf(formattedCode, sizeof(formattedCode), "%c%c%c %c%c%c",
             code[0], code[1], code[2], code[3], code[4], code[5]);
  } else {
    snprintf(formattedCode, sizeof(formattedCode), "%s", code ? code : "------");
  }

  // Draw huge code in center: 7 chars * (8 * 5 = 40px) = 280px width
  // Center in 800px: (800 - 280) / 2 = 260px
  gfx_draw_string(buffer, 260, 150, formattedCode, 1, 5);

  // 6. Step-by-Step Instructions
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 35, 238, "SO KOPPELST DU DIESEN BILDSCHIRM:", 0, 2);
    gfx_draw_hline(buffer, 35, 258, EPD_WIDTH - 70, 0);

    gfx_draw_string(buffer, 48, 268, "1. Oeffne ScreenTinker im Browser am PC, Mac oder Smartphone.", 0, 1);
    gfx_draw_string(buffer, 48, 286, "2. Klicke im Menue auf 'Displays' -> 'Bildschirm hinzufuegen' (+ Add Display).", 0, 1);
    gfx_draw_string(buffer, 48, 304, "3. Gib den obigen 6-stelligen Code ein und vergib einen Namen (z. B. 'Kueche').", 0, 1);
    gfx_draw_string(buffer, 48, 322, "4. Nach dem Bestaetigen startet die Bild-Wiedergabe in wenigen Sekunden!", 0, 1);

    // 7. Footer Box
    gfx_fill_rect(buffer, 35, 344, EPD_WIDTH - 70, 106, 0);
    gfx_draw_string(buffer, 48, 354, "TASTEN & HILFE:", 1, 1);
    gfx_draw_string(buffer, 48, 372, "* UP / DOWN Tasten         : Sprache umschalten [ Deutsch / English ]", 1, 1);
    gfx_draw_string(buffer, 48, 390, "* OK-Taste (kurz)          : Neuen Pairing-Code generieren & pruefen", 1, 1);
    gfx_draw_string(buffer, 48, 408, "* OK-Taste (5 Sek. halten) : Factory Reset / NVS-Konfiguration loeschen", 1, 1);
    gfx_draw_string(buffer, 48, 426, "* Status                   : Automatisches Pruefen alle 4 Sekunden...", 1, 1);
  } else {
    gfx_draw_string(buffer, 35, 238, "HOW TO PAIR THIS DISPLAY:", 0, 2);
    gfx_draw_hline(buffer, 35, 258, EPD_WIDTH - 70, 0);

    gfx_draw_string(buffer, 48, 268, "1. Open ScreenTinker in your browser on PC, Mac or Smartphone.", 0, 1);
    gfx_draw_string(buffer, 48, 286, "2. Navigate to 'Displays' -> Click '+ Add Display' button.", 0, 1);
    gfx_draw_string(buffer, 48, 304, "3. Enter the 6-digit pairing code shown above and choose a display name.", 0, 1);
    gfx_draw_string(buffer, 48, 322, "4. Once submitted, your playlist content will display automatically!", 0, 1);

    // 7. Footer Box
    gfx_fill_rect(buffer, 35, 344, EPD_WIDTH - 70, 106, 0);
    gfx_draw_string(buffer, 48, 354, "BUTTONS & HELP:", 1, 1);
    gfx_draw_string(buffer, 48, 372, "* UP / DOWN Buttons        : Switch Language [ English / Deutsch ]", 1, 1);
    gfx_draw_string(buffer, 48, 390, "* OK Button (Short press)  : Refresh / Request new Pairing Code", 1, 1);
    gfx_draw_string(buffer, 48, 408, "* OK Button (Hold 5 sec)   : Factory Reset / Wipe NVS configuration", 1, 1);
    gfx_draw_string(buffer, 48, 426, "* Status                   : Automatic pairing check every 4 seconds...", 1, 1);
  }
}
