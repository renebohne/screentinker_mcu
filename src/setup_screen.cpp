#include "setup_screen.h"
#include <string.h>
#include <qrcode.h>

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
  if (!str) return;
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

// ─── ScreenTinker Monitor Logo ───────────────────────────────────────────────
void gfx_draw_screentinker_logo(uint8_t* buf, int x, int y, int scale) {
  int w = 24 * scale;
  int h = 15 * scale;
  int border = max(1, scale / 2);
  for (int b = 0; b < border; b++) {
    gfx_draw_rect(buf, x + b, y + b, w - 2 * b, h - 2 * b, 0);
  }
  // Stand neck
  int neckW = max(2, scale);
  int neckH = 4 * scale;
  int neckX = x + (w - neckW) / 2;
  int neckY = y + h;
  gfx_fill_rect(buf, neckX, neckY, neckW, neckH, 0);
  // Stand base
  int baseW = 12 * scale;
  int baseH = max(2, scale);
  int baseX = x + (w - baseW) / 2;
  int baseY = neckY + neckH;
  gfx_fill_rect(buf, baseX, baseY, baseW, baseH, 0);
}

// ─── QR Code Generator Helper ────────────────────────────────────────────────
void gfx_draw_qr(uint8_t* buf, int x, int y, const char* text, int scale) {
  if (!text || strlen(text) == 0) return;
  QRCode qrcode;
  int len = strlen(text);
  int version = (len > 70) ? 6 : ((len > 40) ? 4 : 3);
  uint8_t qrcodeData[qrcode_getBufferSize(6)];
  int res = qrcode_initText(&qrcode, qrcodeData, version, ECC_LOW, text);
  if (res != 0) {
    uint8_t qrcodeData8[qrcode_getBufferSize(8)];
    qrcode_initText(&qrcode, qrcodeData8, 8, ECC_LOW, text);
  }

  int qrSizePx = qrcode.size * scale;
  int pad = 8;
  gfx_fill_rect(buf, x - pad, y - pad, qrSizePx + 2 * pad, qrSizePx + 2 * pad, 1);
  gfx_draw_rect(buf, x - pad, y - pad, qrSizePx + 2 * pad, qrSizePx + 2 * pad, 0);

  for (uint8_t r = 0; r < qrcode.size; r++) {
    for (uint8_t c = 0; c < qrcode.size; c++) {
      if (qrcode_getModule(&qrcode, c, r)) {
        gfx_fill_rect(buf, x + c * scale, y + r * scale, scale, scale, 0);
      }
    }
  }
}

// ─── 1. Boot / Splash Screen ─────────────────────────────────────────────────
void renderSplashScreen(uint8_t* buffer, const char* version, const char* statusMsg, Language lang) {
  gfx_fill(buffer, 1);

  // Outer double border
  gfx_draw_rect(buffer, 10, 10, EPD_WIDTH - 20, EPD_HEIGHT - 20, 0);
  gfx_draw_rect(buffer, 14, 14, EPD_WIDTH - 28, EPD_HEIGHT - 28, 0);

  // ScreenTinker Monitor Logo (Scale 4: 96x80)
  gfx_draw_screentinker_logo(buffer, 352, 45, 4);

  // "ScreenTinker" Header (Scale 4: 12 chars * 32px = 384px -> centered at 208)
  gfx_draw_string(buffer, 208, 145, "ScreenTinker", 0, 4);

  // Subtitle
  gfx_draw_string(buffer, 224, 195, "Smart Digital Signage", 0, 2);

  // Hardware Model line
  const char* hwModel = "Seeed Studio reTerminal Sticky";
  int hwLen = strlen(hwModel);
  int hwX = (EPD_WIDTH - hwLen * 16) / 2;
  gfx_draw_string(buffer, hwX, 235, hwModel, 0, 2);

  // Firmware Version Pill
  char verStr[32];
  snprintf(verStr, sizeof(verStr), "Firmware v%s", version ? version : FIRMWARE_VERSION);
  int verLen = strlen(verStr);
  int verW = verLen * 16 + 32;
  int verX = (EPD_WIDTH - verW) / 2;
  gfx_draw_rect(buffer, verX, 272, verW, 34, 0);
  gfx_draw_string(buffer, verX + 16, 281, verStr, 0, 2);

  // Status message bar (Inverted)
  gfx_fill_rect(buffer, 50, 350, EPD_WIDTH - 100, 56, 0);
  const char* defMsg = (lang == LANG_DE) ? "Verbindung wird aufgebaut..." : "Connecting to network...";
  const char* msg = statusMsg ? statusMsg : defMsg;
  int msgLen = strlen(msg);
  int msgX = max(60, (EPD_WIDTH - msgLen * 16) / 2);
  gfx_draw_string(buffer, msgX, 368, msg, 1, 2);
}

// ─── 2. Pairing Code Screen (Setup / PIN + QR Code) ─────────────────────────
void renderPairingCodeScreen(uint8_t* buffer, const char* code, const char* serverUrl, Language lang) {
  gfx_fill(buffer, 1);

  // Top header banner
  gfx_fill_rect(buffer, 0, 0, EPD_WIDTH, 54, 0);
  gfx_draw_screentinker_logo(buffer, 20, 12, 1);
  gfx_draw_string(buffer, 56, 16, "ScreenTinker — Setup", 1, 3);

  // Left Column: QR Code (Direct URL to dashboard / claim)
  char claimUrl[128];
  if (serverUrl && strlen(serverUrl) > 0) {
    snprintf(claimUrl, sizeof(claimUrl), "%s/app", serverUrl);
  } else {
    snprintf(claimUrl, sizeof(claimUrl), "http://localhost:3001/app");
  }

  // QR Code placed centrally in left half (x = 100, y = 72)
  gfx_draw_qr(buffer, 100, 72, claimUrl, 5);

  // Left Column text instructions (max 22 chars per line for Scale 2 in 360px)
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 36, 255, "1. QR-Code scannen", 0, 2);
    gfx_draw_string(buffer, 36, 285, "   oder URL oeffnen:", 0, 2);
  } else {
    gfx_draw_string(buffer, 36, 255, "1. Scan QR code", 0, 2);
    gfx_draw_string(buffer, 36, 285, "   or open URL:", 0, 2);
  }

  // URL display box
  gfx_draw_rect(buffer, 24, 320, 330, 26, 0);
  int urlLen = strlen(claimUrl);
  int urlX = max(30, 24 + (330 - urlLen * 8) / 2);
  gfx_draw_string(buffer, urlX, 329, claimUrl, 0, 1);

  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 36, 365, "Auto-Kopplung aktiv...", 0, 2);
  } else {
    gfx_draw_string(buffer, 36, 365, "Auto-sync active...", 0, 2);
  }

  // Vertical Separator between columns
  gfx_draw_vline(buffer, 380, 68, 344, 0);

  // Right Column: Huge 6-Digit PIN Code Box
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 410, 72, "2. PIN-CODE EINGEBEN:", 0, 2);
  } else {
    gfx_draw_string(buffer, 410, 72, "2. ENTER PIN CODE:", 0, 2);
  }

  gfx_fill_rect(buffer, 410, 104, 360, 92, 0);
  gfx_draw_rect(buffer, 414, 108, 352, 84, 1);

  char formattedCode[16];
  if (code && strlen(code) == 6) {
    snprintf(formattedCode, sizeof(formattedCode), "%c%c%c %c%c%c",
             code[0], code[1], code[2], code[3], code[4], code[5]);
  } else {
    snprintf(formattedCode, sizeof(formattedCode), "%s", code ? code : "------");
  }
  // 7 chars * 40px = 280px wide -> centered in 360px box: x = 410 + (360 - 280) / 2 = 450
  gfx_draw_string(buffer, 450, 130, formattedCode, 1, 5);

  // Step 3 & 4 (Clean lines within 22 chars limit in Scale 2)
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 410, 220, "3. '+ Add Display'", 0, 2);
    gfx_draw_string(buffer, 410, 250, "   im Dashboard waehlen", 0, 2);
    gfx_draw_string(buffer, 410, 295, "4. Code eingeben &", 0, 2);
    gfx_draw_string(buffer, 410, 325, "   Display koppeln", 0, 2);
    gfx_draw_string(buffer, 410, 365, "-> Startet automatisch!", 0, 2);
  } else {
    gfx_draw_string(buffer, 410, 220, "3. Click '+ Add Display'", 0, 2);
    gfx_draw_string(buffer, 410, 250, "   in your Dashboard", 0, 2);
    gfx_draw_string(buffer, 410, 295, "4. Enter PIN code &", 0, 2);
    gfx_draw_string(buffer, 410, 325, "   pair display", 0, 2);
    gfx_draw_string(buffer, 410, 365, "-> Starts automatically!", 0, 2);
  }

  // Bottom Navigation Bar
  gfx_fill_rect(buffer, 0, 424, EPD_WIDTH, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 30, 442, "[ OK 1.5s ]: Einstellungen", 1, 2);
    gfx_draw_string(buffer, 460, 442, "[ OK kurz ]: Refresh", 1, 2);
  } else {
    gfx_draw_string(buffer, 30, 442, "[ OK 1.5s ]: Settings", 1, 2);
    gfx_draw_string(buffer, 460, 442, "[ OK short ]: Refresh", 1, 2);
  }
}

// ─── 3. Settings Menu (7 Items with Language Selection) ──────────────────────
// ─── 3. Settings Menu (7 Items with Wi-Fi Power & Language) ──────────────────
void renderSystemMenu(uint8_t* buffer, int selectedIndex, Language lang, uint8_t wifiTxLevel) {
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, EPD_WIDTH, 54, 0);
  gfx_draw_screentinker_logo(buffer, 20, 12, 1);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 56, 16, "ScreenTinker — Einstellungen", 1, 3);
  } else {
    gfx_draw_string(buffer, 56, 16, "ScreenTinker — Settings", 1, 3);
  }

  char wifiPwrDE[64];
  char wifiPwrEN[64];
  if (wifiTxLevel == 1) {
    snprintf(wifiPwrDE, sizeof(wifiPwrDE), "2. WLAN-LEISTUNG: MITTEL [15 dBm]");
    snprintf(wifiPwrEN, sizeof(wifiPwrEN), "2. WI-FI POWER: MED [15 dBm]");
  } else if (wifiTxLevel == 2) {
    snprintf(wifiPwrDE, sizeof(wifiPwrDE), "2. WLAN-LEISTUNG: NIEDRIG [11 dBm]");
    snprintf(wifiPwrEN, sizeof(wifiPwrEN), "2. WI-FI POWER: LOW [11 dBm]");
  } else {
    snprintf(wifiPwrDE, sizeof(wifiPwrDE), "2. WLAN-LEISTUNG: MAX [19.5 dBm]");
    snprintf(wifiPwrEN, sizeof(wifiPwrEN), "2. WI-FI POWER: MAX [19.5 dBm]");
  }

  const char* menuItemsDE[7] = {
    "1. STATUS & INFO",
    wifiPwrDE,
    "3. SPRACHE: DEUTSCH [DE]",
    "4. GERAET ENTKOPPELN (UNPAIR)",
    "5. AUSSCHALTEN / STANDBY",
    "6. WERKSEINSTELLUNGEN (RESET)",
    "7. ZURUECK / SCHLIESSEN"
  };

  const char* menuItemsEN[7] = {
    "1. STATUS & INFO",
    wifiPwrEN,
    "3. LANGUAGE: ENGLISH [EN]",
    "4. UNPAIR DEVICE",
    "5. POWER OFF / STANDBY",
    "6. FACTORY RESET",
    "7. BACK / CLOSE"
  };

  const char** menuItems = (lang == LANG_DE) ? menuItemsDE : menuItemsEN;

  int startY = 66;
  int itemH = 43;
  int itemW = EPD_WIDTH - 60;
  int itemX = 30;

  for (int i = 0; i < 7; i++) {
    int y = startY + i * (itemH + 6);
    if (i == selectedIndex) {
      gfx_fill_rect(buffer, itemX, y, itemW, itemH, 0);
      char selText[64];
      snprintf(selText, sizeof(selText), "> %s", menuItems[i]);
      gfx_draw_string(buffer, itemX + 20, y + 12, selText, 1, 2);
    } else {
      gfx_draw_rect(buffer, itemX, y, itemW, itemH, 0);
      char normText[64];
      snprintf(normText, sizeof(normText), "  %s", menuItems[i]);
      gfx_draw_string(buffer, itemX + 20, y + 12, normText, 0, 2);
    }
  }

  // Footer Navigation Hint
  gfx_fill_rect(buffer, 0, 424, EPD_WIDTH, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 30, 442, "[ UP / DOWN ]: Navigieren", 1, 2);
    gfx_draw_string(buffer, 500, 442, "[ OK ]: Auswaehlen", 1, 2);
  } else {
    gfx_draw_string(buffer, 30, 442, "[ UP / DOWN ]: Navigate", 1, 2);
    gfx_draw_string(buffer, 520, 442, "[ OK ]: Select", 1, 2);
  }
}

// ─── 4. Status & Diagnostic Info Screen (5 Full-Width Spacious Rows) ─────────
void renderStatusInfoScreen(uint8_t* buffer, const char* ip, const char* ssid, int rssi, const char* serverUrl, const char* devId, const char* version, Language lang) {
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, EPD_WIDTH, 52, 0);
  gfx_draw_string(buffer, 20, 15, "ScreenTinker — Status & Info", 1, 3);

  int y = 64;
  int rowH = 54;
  int rowGap = 12;
  int boxW = EPD_WIDTH - 20; // 780px wide (x = 10 to 790)
  int boxX = 10;
  int labelX = boxX + 10;   // x = 20
  int valueX = 212;         // x = 212 (guarantees a full space after "IP Address:" and "Server URL:")

  // Row 1: IP Address
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 18, (lang == LANG_DE) ? "IP-Adresse:" : "IP Address:", 0, 2);
  const char* defIp = (lang == LANG_DE) ? "Standby (Power Save)" : "Standby (Power Save)";
  const char* ipDisplay = (ip && strlen(ip) > 0 && strcmp(ip, "0.0.0.0") != 0) ? ip : defIp;
  gfx_draw_string(buffer, valueX, y + 18, ipDisplay, 0, 2);

  // Row 2: Wi-Fi SSID & RSSI
  y += rowH + rowGap;
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 18, (lang == LANG_DE) ? "WLAN-Netz:" : "Wi-Fi:", 0, 2);
  char wifiStr[64];
  if (rssi != 0) {
    snprintf(wifiStr, sizeof(wifiStr), "%s (%d dBm)", (ssid && strlen(ssid) > 0) ? ssid : "-", rssi);
  } else {
    snprintf(wifiStr, sizeof(wifiStr), "%s", (ssid && strlen(ssid) > 0) ? ssid : "-");
  }
  gfx_draw_string(buffer, valueX, y + 18, wifiStr, 0, 2);

  // Row 3: Server URL
  y += rowH + rowGap;
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 18, (lang == LANG_DE) ? "Server-URL:" : "Server URL:", 0, 2);
  gfx_draw_string(buffer, valueX, y + 18, (serverUrl && strlen(serverUrl) > 0) ? serverUrl : "-", 0, 2);

  // Row 4: Firmware Version
  y += rowH + rowGap;
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 18, "Firmware:", 0, 2);
  char verStr[64];
  snprintf(verStr, sizeof(verStr), "v%s (Seeed Sticky E-Paper)", version ? version : FIRMWARE_VERSION);
  gfx_draw_string(buffer, valueX, y + 18, verStr, 0, 2);

  // Row 5: Device ID
  y += rowH + rowGap;
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 18, (lang == LANG_DE) ? "Device-ID:" : "Device ID:", 0, 2);
  gfx_draw_string(buffer, valueX, y + 18, (devId && strlen(devId) > 0) ? devId : "-", 0, 2);

  // Footer Navigation Hint
  gfx_fill_rect(buffer, 0, 424, EPD_WIDTH, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 200, 442, "[ OK / UP / DOWN ]: Zurueck", 1, 2);
  } else {
    gfx_draw_string(buffer, 220, 442, "[ OK / UP / DOWN ]: Back", 1, 2);
  }
}

// ─── 5. Prominent No Wi-Fi Screen ────────────────────────────────────────────
void renderNoWifiScreen(uint8_t* buffer, const char* ssid, Language lang) {
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, EPD_WIDTH, 54, 0);
  gfx_draw_string(buffer, 30, 16, "ScreenTinker — Offline", 1, 3);

  // Big Warning Box
  gfx_fill_rect(buffer, 40, 80, EPD_WIDTH - 80, 70, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 120, 100, "[!] KEINE WLAN-VERBINDUNG", 1, 3);
  } else {
    gfx_draw_string(buffer, 120, 100, "[!] NO WI-FI CONNECTION", 1, 3);
  }

  // Details
  gfx_draw_rect(buffer, 40, 170, EPD_WIDTH - 80, 80, 0);
  gfx_draw_string(buffer, 60, 185, (lang == LANG_DE) ? "Konfigurierte SSID:" : "Configured SSID:   ", 0, 2);
  gfx_draw_string(buffer, 320, 185, (ssid && strlen(ssid) > 0) ? ssid : "-", 0, 2);
  gfx_draw_string(buffer, 60, 218, (lang == LANG_DE) ? "Status: Verbindungsversuch fehlgeschlagen" : "Status: Connection attempt failed", 0, 2);

  // Actions Box
  gfx_draw_string(buffer, 40, 280, (lang == LANG_DE) ? "AKTIONEN:" : "ACTIONS:", 0, 2);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 60, 315, "* [ OK kurz ]  : Sofort erneut verbinden", 0, 2);
    gfx_draw_string(buffer, 60, 350, "* [ OK 1.5s ]  : Einstellungen oeffnen", 0, 2);
  } else {
    gfx_draw_string(buffer, 60, 315, "* [ OK short ] : Retry connection now", 0, 2);
    gfx_draw_string(buffer, 60, 350, "* [ OK 1.5s ]  : Open settings menu", 0, 2);
  }

  // Footer Navigation Hint
  gfx_fill_rect(buffer, 0, 424, EPD_WIDTH, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 30, 442, "[ OK ]: Neu verbinden", 1, 2);
    gfx_draw_string(buffer, 460, 442, "[ OK 1.5s ]: Einstellungen", 1, 2);
  } else {
    gfx_draw_string(buffer, 30, 442, "[ OK ]: Retry", 1, 2);
    gfx_draw_string(buffer, 480, 442, "[ OK 1.5s ]: Settings", 1, 2);
  }
}

// ─── 6. Onboarding / Setup Screen ────────────────────────────────────────────
void renderOnboardingScreen(uint8_t* buffer, Language lang) {
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, EPD_WIDTH, 54, 0);
  gfx_draw_screentinker_logo(buffer, 20, 12, 1);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 56, 16, "ScreenTinker — Ersteinrichtung", 1, 3);
  } else {
    gfx_draw_string(buffer, 56, 16, "ScreenTinker — Initial Setup", 1, 3);
  }

  // QR Code to SoftAP portal (http://192.168.4.1)
  gfx_draw_qr(buffer, 60, 95, "http://192.168.4.1", 5);
  gfx_draw_string(buffer, 50, 300, (lang == LANG_DE) ? "WLAN: ScreenTinker-Setup" : "Wi-Fi: ScreenTinker-Setup", 0, 2);
  gfx_draw_string(buffer, 50, 330, "URL:  http://192.168.4.1", 0, 2);

  // Vertical Separator
  gfx_draw_vline(buffer, 380, 75, 330, 0);

  // Steps on Right Side
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 410, 75, "SCHRITTE ZUM SETUP:", 0, 2);
    gfx_draw_string(buffer, 410, 115, "1. WLAN verbinden mit:", 0, 2);
    gfx_draw_string(buffer, 410, 145, "   'ScreenTinker-Setup'", 0, 2);

    gfx_draw_string(buffer, 410, 200, "2. Browser oeffnen:", 0, 2);
    gfx_draw_string(buffer, 410, 230, "   http://192.168.4.1", 0, 2);

    gfx_draw_string(buffer, 410, 285, "3. WLAN auswaehlen &", 0, 2);
    gfx_draw_string(buffer, 410, 315, "   Passwort speichern", 0, 2);

    gfx_draw_string(buffer, 410, 365, "-> Display verbindet sich!", 0, 2);
  } else {
    gfx_draw_string(buffer, 410, 75, "SETUP STEPS:", 0, 2);
    gfx_draw_string(buffer, 410, 115, "1. Connect Wi-Fi to:", 0, 2);
    gfx_draw_string(buffer, 410, 145, "   'ScreenTinker-Setup'", 0, 2);

    gfx_draw_string(buffer, 410, 200, "2. Open web browser:", 0, 2);
    gfx_draw_string(buffer, 410, 230, "   http://192.168.4.1", 0, 2);

    gfx_draw_string(buffer, 410, 285, "3. Select your Wi-Fi &", 0, 2);
    gfx_draw_string(buffer, 410, 315, "   save credentials", 0, 2);

    gfx_draw_string(buffer, 410, 365, "-> Display connects now!", 0, 2);
  }

  // Footer Navigation Hint
  gfx_fill_rect(buffer, 0, 424, EPD_WIDTH, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 30, 442, "[ OK 1.5s ]: Einstellungen", 1, 2);
    gfx_draw_string(buffer, 480, 442, "Warte auf Setup...", 1, 2);
  } else {
    gfx_draw_string(buffer, 30, 442, "[ OK 1.5s ]: Settings", 1, 2);
    gfx_draw_string(buffer, 480, 442, "Waiting for setup...", 1, 2);
  }
}

// ─── 7. Power Off Screen ─────────────────────────────────────────────────────
void renderPowerOffScreen(uint8_t* buffer, Language lang) {
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, EPD_WIDTH, 54, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 180, 16, "ScreenTinker — Standby", 1, 3);
  } else {
    gfx_draw_string(buffer, 180, 16, "ScreenTinker — Power Off", 1, 3);
  }

  // QR Code to Repo
  gfx_draw_qr(buffer, 80, 95, "https://github.com/renebohne/screentinker_mcu", 5);

  // Details
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 380, 110, "GERAET IM STANDBY", 0, 3);
    gfx_draw_string(buffer, 380, 160, "Seeed reTerminal Sticky", 0, 2);
    gfx_draw_string(buffer, 380, 190, "Ultra-Low-Power E-Paper Client", 0, 2);

    gfx_draw_rect(buffer, 380, 240, 380, 80, 0);
    gfx_draw_string(buffer, 400, 265, "Druecke die [ OK ]-Taste", 0, 2);
    gfx_draw_string(buffer, 400, 290, "zum Einschalten!", 0, 2);
  } else {
    gfx_draw_string(buffer, 380, 110, "DEVICE IN STANDBY", 0, 3);
    gfx_draw_string(buffer, 380, 160, "Seeed reTerminal Sticky", 0, 2);
    gfx_draw_string(buffer, 380, 190, "Ultra-Low-Power E-Paper Client", 0, 2);

    gfx_draw_rect(buffer, 380, 240, 380, 80, 0);
    gfx_draw_string(buffer, 400, 265, "Press the [ OK ] button", 0, 2);
    gfx_draw_string(buffer, 400, 290, "to wake up / power on!", 0, 2);
  }

  // Footer Navigation Hint
  gfx_fill_rect(buffer, 0, 424, EPD_WIDTH, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 180, 442, "[ OK ]: Aufwecken & Einschalten", 1, 2);
  } else {
    gfx_draw_string(buffer, 180, 442, "[ OK ]: Wake Up & Turn On", 1, 2);
  }
}

// ─── Offline Badge for Cache-Rotation Overlay ────────────────────────────────
void gfx_draw_offline_badge(uint8_t* buffer) {
  gfx_fill_rect(buffer, 642, 14, 142, 30, 0);
  gfx_draw_rect(buffer, 644, 16, 138, 26, 1);
  gfx_draw_string(buffer, 660, 22, "OFFLINE", 1, 2);
}




