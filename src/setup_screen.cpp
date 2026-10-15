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

// ─── Graphics Primitives for 1-Bit Monochrom ─────────────────────────
// 1 = White (0xFF), 0 = Black (0x00)

static int s_epd_width = EPD_WIDTH;
static int s_epd_height = EPD_HEIGHT;

void gfx_set_screen_dimensions(int w, int h) {
  if (w > 0 && h > 0) {
    s_epd_width = w;
    s_epd_height = h;
  }
}

int gfx_get_width() { return s_epd_width; }
int gfx_get_height() { return s_epd_height; }
size_t gfx_get_buffer_size() { return ((size_t)s_epd_width * s_epd_height) / 8; }

void gfx_fill(uint8_t* buf, uint8_t color) {
  memset(buf, color ? 0xFF : 0x00, gfx_get_buffer_size());
}

void gfx_draw_pixel(uint8_t* buf, int x, int y, uint8_t color) {
  if (x < 0 || x >= s_epd_width || y < 0 || y >= s_epd_height) return;
  size_t byte_idx = ((size_t)y * (s_epd_width / 8)) + (x / 8);
  uint8_t bit_mask = 0x80 >> (x & 7);
  if (color) {
    buf[byte_idx] |= bit_mask;  // White
  } else {
    buf[byte_idx] &= ~bit_mask; // Black
  }
}

uint8_t gfx_get_pixel(const uint8_t* buf, int x, int y) {
  if (!buf || x < 0 || x >= s_epd_width || y < 0 || y >= s_epd_height) return 1;
  size_t byte_idx = ((size_t)y * (s_epd_width / 8)) + (x / 8);
  uint8_t bit_mask = 0x80 >> (x & 7);
  return (buf[byte_idx] & bit_mask) ? 1 : 0;
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

  int margin = max(10, min(24, s_epd_width / 80));
  // Outer double border
  gfx_draw_rect(buffer, margin, margin, s_epd_width - 2 * margin, s_epd_height - 2 * margin, 0);
  gfx_draw_rect(buffer, margin + 4, margin + 4, s_epd_width - 2 * (margin + 4), s_epd_height - 2 * (margin + 4), 0);

  int uiScale = (s_epd_width >= 1200) ? 2 : 1;
  int logoScale = 4 * uiScale;
  int titleScale = 4 * uiScale;
  int subScale = 2 * uiScale;

  // ScreenTinker Monitor Logo (Scale: 24*scale x 21*scale)
  int logoW = 24 * logoScale;
  int logoH = 21 * logoScale;

  // Calculate layout bounds
  int barH = 56 * uiScale;
  int barY = s_epd_height - margin - barH - (uiScale > 1 ? 40 : 20);
  int availTop = margin + (uiScale > 1 ? 40 : 20);
  int availH = barY - availTop;

  // Total content height in center
  int totalContentH = logoH + 20 * uiScale + (8 * titleScale) + 15 * uiScale + (8 * subScale) + 15 * uiScale + (8 * subScale) + 15 * uiScale + (34 * uiScale);
  int curY = availTop + max(10, (availH - totalContentH) / 2);

  // Logo
  gfx_draw_screentinker_logo(buffer, (s_epd_width - logoW) / 2, curY, logoScale);
  curY += logoH + 20 * uiScale;

  // Header "ScreenTinker"
  int titleLen = 12; // "ScreenTinker"
  int titleW = titleLen * 8 * titleScale;
  gfx_draw_string(buffer, (s_epd_width - titleW) / 2, curY, "ScreenTinker", 0, titleScale);
  curY += (8 * titleScale) + 15 * uiScale;

  // Subtitle
  const char* subTitle = "Smart Digital Signage";
  int subLen = strlen(subTitle);
  int subW = subLen * 8 * subScale;
  gfx_draw_string(buffer, (s_epd_width - subW) / 2, curY, subTitle, 0, subScale);
  curY += (8 * subScale) + 15 * uiScale;

  // Hardware Model line
  const char* hwModel = "Seeed Studio reTerminal Sticky";
  if (s_epd_width == 1872 && s_epd_height == 1404) {
    hwModel = "Seeed Studio reTerminal E1003";
  } else if (s_epd_width == 1200 && s_epd_height == 1600) {
    hwModel = "Seeed Studio reTerminal E1004";
  } else if (s_epd_width != 800 || s_epd_height != 480) {
    hwModel = "Seeed Studio reTerminal E-Series";
  }
  int hwLen = strlen(hwModel);
  int hwW = hwLen * 8 * subScale;
  gfx_draw_string(buffer, (s_epd_width - hwW) / 2, curY, hwModel, 0, subScale);
  curY += (8 * subScale) + 15 * uiScale;

  // Firmware Version Pill
  char verStr[32];
  snprintf(verStr, sizeof(verStr), "Firmware v%s", version ? version : FIRMWARE_VERSION);
  int verLen = strlen(verStr);
  int verPillW = verLen * 8 * subScale + 32 * uiScale;
  int verPillH = 34 * uiScale;
  int verX = (s_epd_width - verPillW) / 2;
  gfx_draw_rect(buffer, verX, curY, verPillW, verPillH, 0);
  gfx_draw_string(buffer, verX + 16 * uiScale, curY + 8 * uiScale, verStr, 0, subScale);

  // Status message bar (Inverted)
  int barW = s_epd_width - 2 * (margin + 30);
  int barX = (s_epd_width - barW) / 2;
  gfx_fill_rect(buffer, barX, barY, barW, barH, 0);
  const char* defMsg = (lang == LANG_DE) ? "Verbindung wird aufgebaut..." : "Connecting to network...";
  const char* msg = statusMsg ? statusMsg : defMsg;
  int msgLen = strlen(msg);
  int msgScale = subScale;
  int msgTextW = msgLen * 8 * msgScale;
  int msgX = max(barX + 16, (s_epd_width - msgTextW) / 2);
  int msgY = barY + (barH - 8 * msgScale) / 2;
  gfx_draw_string(buffer, msgX, msgY, msg, 1, msgScale);
}

// ─── 2. Pairing Code Screen (Setup / PIN + QR Code) ─────────────────────────
void renderPairingCodeScreen(uint8_t* buffer, const char* code, const char* serverUrl, Language lang) {
  gfx_fill(buffer, 1);

  int uiScale = (s_epd_width >= 1200) ? 2 : 1;
  int headerH = (uiScale > 1) ? 72 : 54;
  int footerH = (uiScale > 1) ? 72 : 56;

  // Top header banner
  gfx_fill_rect(buffer, 0, 0, s_epd_width, headerH, 0);
  int logoScale = uiScale;
  gfx_draw_screentinker_logo(buffer, 20 * uiScale, (headerH - 21 * logoScale) / 2, logoScale);
  int headerFontScale = 3;
  gfx_draw_string(buffer, 20 * uiScale + 32 * logoScale, (headerH - 8 * headerFontScale) / 2, "ScreenTinker — Setup", 1, headerFontScale);

  // Column split
  int midX = s_epd_width / 2;
  int contentTop = headerH + 14 * uiScale;
  int contentBottom = s_epd_height - footerH - 14 * uiScale;
  int contentH = contentBottom - contentTop;

  // Vertical Separator between columns
  gfx_draw_vline(buffer, midX, contentTop, contentH, 0);

  // Left Column: QR Code
  char claimUrl[128];
  if (serverUrl && strlen(serverUrl) > 0) {
    snprintf(claimUrl, sizeof(claimUrl), "%s/app", serverUrl);
  } else {
    snprintf(claimUrl, sizeof(claimUrl), "http://localhost:3001/app");
  }

  int qrScale = (s_epd_width >= 1600) ? 8 : ((s_epd_width >= 1200) ? 6 : 5);
  int qrPx = 33 * qrScale;
  int qrX = (midX - qrPx) / 2;
  int qrY = contentTop + 10 * uiScale;
  gfx_draw_qr(buffer, qrX, qrY, claimUrl, qrScale);

  // Left Column text instructions below QR
  int textScale = (uiScale > 1) ? 3 : 2;
  int textY = qrY + qrPx + 20 * uiScale;
  int textMargin = max(24, (midX - (22 * 8 * textScale)) / 2);

  if (lang == LANG_DE) {
    gfx_draw_string(buffer, textMargin, textY, "1. QR-Code scannen", 0, textScale);
    gfx_draw_string(buffer, textMargin, textY + 28 * uiScale, "   oder URL oeffnen:", 0, textScale);
  } else {
    gfx_draw_string(buffer, textMargin, textY, "1. Scan QR code", 0, textScale);
    gfx_draw_string(buffer, textMargin, textY + 28 * uiScale, "   or open URL:", 0, textScale);
  }

  // URL display box
  int urlBoxW = min(midX - 40, (uiScale > 1) ? 500 : 330);
  int urlBoxX = (midX - urlBoxW) / 2;
  int urlBoxY = textY + 62 * uiScale;
  gfx_draw_rect(buffer, urlBoxX, urlBoxY, urlBoxW, 26 * uiScale, 0);
  int urlLen = strlen(claimUrl);
  int urlScale = (urlLen * 8 * (uiScale > 1 ? 2 : 1) < urlBoxW - 16) ? (uiScale > 1 ? 2 : 1) : 1;
  int urlCharW = 8 * urlScale;
  int urlX = max(urlBoxX + 6, urlBoxX + (urlBoxW - urlLen * urlCharW) / 2);
  gfx_draw_string(buffer, urlX, urlBoxY + (26 * uiScale - 8 * urlScale) / 2, claimUrl, 0, urlScale);

  if (contentH > 320 * uiScale) {
    if (lang == LANG_DE) {
      gfx_draw_string(buffer, textMargin, urlBoxY + 36 * uiScale, "Auto-Kopplung aktiv...", 0, textScale);
    } else {
      gfx_draw_string(buffer, textMargin, urlBoxY + 36 * uiScale, "Auto-sync active...", 0, textScale);
    }
  }

  // Right Column: PIN Code
  int rightColW = s_epd_width - midX;
  int pinBoxW = min(rightColW - 40, (uiScale > 1) ? 500 : 360);
  int pinBoxX = midX + (rightColW - pinBoxW) / 2;
  int rightTextX = pinBoxX;

  if (lang == LANG_DE) {
    gfx_draw_string(buffer, rightTextX, contentTop + 10 * uiScale, "2. PIN-CODE EINGEBEN:", 0, textScale);
  } else {
    gfx_draw_string(buffer, rightTextX, contentTop + 10 * uiScale, "2. ENTER PIN CODE:", 0, textScale);
  }

  int pinBoxH = (uiScale > 1) ? 120 : 92;
  int pinBoxY = contentTop + 40 * uiScale;
  gfx_fill_rect(buffer, pinBoxX, pinBoxY, pinBoxW, pinBoxH, 0);
  gfx_draw_rect(buffer, pinBoxX + 4, pinBoxY + 4, pinBoxW - 8, pinBoxH - 8, 1);

  char formattedCode[16];
  if (code && strlen(code) == 6) {
    snprintf(formattedCode, sizeof(formattedCode), "%c%c%c %c%c%c",
             code[0], code[1], code[2], code[3], code[4], code[5]);
  } else {
    snprintf(formattedCode, sizeof(formattedCode), "%s", code ? code : "------");
  }

  int pinScale = (uiScale > 1) ? 6 : 5;
  int pinLen = strlen(formattedCode);
  int pinW = pinLen * 8 * pinScale;
  int pinX = pinBoxX + (pinBoxW - pinW) / 2;
  int pinY = pinBoxY + (pinBoxH - 8 * pinScale) / 2;
  gfx_draw_string(buffer, pinX, pinY, formattedCode, 1, pinScale);

  // Step 3 & 4
  int stepY = pinBoxY + pinBoxH + 20 * uiScale;
  int lineStep = 28 * uiScale;
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, rightTextX, stepY, "3. '+ Add Display'", 0, textScale);
    gfx_draw_string(buffer, rightTextX, stepY + lineStep, "   im Dashboard waehlen", 0, textScale);
    gfx_draw_string(buffer, rightTextX, stepY + lineStep * 2 + 10, "4. Code eingeben &", 0, textScale);
    gfx_draw_string(buffer, rightTextX, stepY + lineStep * 3 + 10, "   Display koppeln", 0, textScale);
    gfx_draw_string(buffer, rightTextX, stepY + lineStep * 4 + 20, "-> Startet automatisch!", 0, textScale);
  } else {
    gfx_draw_string(buffer, rightTextX, stepY, "3. Click '+ Add Display'", 0, textScale);
    gfx_draw_string(buffer, rightTextX, stepY + lineStep, "   in your Dashboard", 0, textScale);
    gfx_draw_string(buffer, rightTextX, stepY + lineStep * 2 + 10, "4. Enter PIN code &", 0, textScale);
    gfx_draw_string(buffer, rightTextX, stepY + lineStep * 3 + 10, "   pair display", 0, textScale);
    gfx_draw_string(buffer, rightTextX, stepY + lineStep * 4 + 20, "-> Starts automatically!", 0, textScale);
  }

  // Bottom Navigation Bar
  gfx_fill_rect(buffer, 0, s_epd_height - footerH, s_epd_width, footerH, 0);
  int navY = s_epd_height - footerH + (footerH - 16) / 2;
  int navScale = 2;
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 30 * uiScale, navY, "[ OK 1.5s ]: Einstellungen", 1, navScale);
    const char* rNav = "[ OK kurz ]: Refresh";
    int rNavW = strlen(rNav) * 8 * navScale;
    gfx_draw_string(buffer, s_epd_width - rNavW - 30 * uiScale, navY, rNav, 1, navScale);
  } else {
    gfx_draw_string(buffer, 30 * uiScale, navY, "[ OK 1.5s ]: Settings", 1, navScale);
    const char* rNav = "[ OK short ]: Refresh";
    int rNavW = strlen(rNav) * 8 * navScale;
    gfx_draw_string(buffer, s_epd_width - rNavW - 30 * uiScale, navY, rNav, 1, navScale);
  }
}

// ─── 3. Settings Menu (8 Items with Orientation, Wi-Fi Power & Language) ─────
void renderSystemMenu(uint8_t* buffer, int selectedIndex, Language lang, uint8_t wifiTxLevel, uint8_t orientMode, bool hasImu, int batteryPct, bool isCharging, bool isUsbPower) {
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, s_epd_width, 54, 0);
  gfx_draw_screentinker_logo(buffer, 20, 12, 1);
  int titleScale = (batteryPct >= 0 && s_epd_width <= 800) ? 2 : 3;
  int titleY = (titleScale == 2) ? 19 : 16;
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 56, titleY, "ScreenTinker — Einstellungen", 1, titleScale);
  } else {
    gfx_draw_string(buffer, 56, titleY, "ScreenTinker — Settings", 1, titleScale);
  }

  if (batteryPct >= 0) {
    char battBadge[32];
    if (isCharging) {
      snprintf(battBadge, sizeof(battBadge), "[ %d%% CHG ]", batteryPct);
    } else if (isUsbPower) {
      snprintf(battBadge, sizeof(battBadge), "[ %d%% USB ]", batteryPct);
    } else {
      snprintf(battBadge, sizeof(battBadge), "[ %d%% ]", batteryPct);
    }
    int bLen = strlen(battBadge);
    gfx_draw_string(buffer, s_epd_width - (bLen * 16) - 20, 18, battBadge, 1, 2);
  }

  char orientDE[64];
  char orientEN[64];
  if (hasImu) {
    if (orientMode == ORIENT_MODE_AUTO) {
      snprintf(orientDE, sizeof(orientDE), "2. AUSRICHTUNG: AUTO [IMU]");
      snprintf(orientEN, sizeof(orientEN), "2. ORIENTATION: AUTO [IMU]");
    } else if (orientMode == ORIENT_MODE_PORTRAIT) {
      snprintf(orientDE, sizeof(orientDE), "2. AUSRICHTUNG: HOCHFORMAT");
      snprintf(orientEN, sizeof(orientEN), "2. ORIENTATION: PORTRAIT");
    } else {
      snprintf(orientDE, sizeof(orientDE), "2. AUSRICHTUNG: QUERFORMAT");
      snprintf(orientEN, sizeof(orientEN), "2. ORIENTATION: LANDSCAPE");
    }
  } else {
    if (orientMode == ORIENT_MODE_PORTRAIT) {
      snprintf(orientDE, sizeof(orientDE), "2. AUSRICHTUNG: HOCHFORMAT");
      snprintf(orientEN, sizeof(orientEN), "2. ORIENTATION: PORTRAIT");
    } else {
      snprintf(orientDE, sizeof(orientDE), "2. AUSRICHTUNG: QUERFORMAT");
      snprintf(orientEN, sizeof(orientEN), "2. ORIENTATION: LANDSCAPE");
    }
  }

  char wifiPwrDE[64];
  char wifiPwrEN[64];
  if (wifiTxLevel == 1) {
    snprintf(wifiPwrDE, sizeof(wifiPwrDE), "3. WLAN-LEISTUNG: MITTEL [15 dBm]");
    snprintf(wifiPwrEN, sizeof(wifiPwrEN), "3. WI-FI POWER: MED [15 dBm]");
  } else if (wifiTxLevel == 2) {
    snprintf(wifiPwrDE, sizeof(wifiPwrDE), "3. WLAN-LEISTUNG: NIEDRIG [11 dBm]");
    snprintf(wifiPwrEN, sizeof(wifiPwrEN), "3. WI-FI POWER: LOW [11 dBm]");
  } else {
    snprintf(wifiPwrDE, sizeof(wifiPwrDE), "3. WLAN-LEISTUNG: MAX [19.5 dBm]");
    snprintf(wifiPwrEN, sizeof(wifiPwrEN), "3. WI-FI POWER: MAX [19.5 dBm]");
  }

  const char* menuItemsDE[8] = {
    "1. STATUS & INFO",
    orientDE,
    wifiPwrDE,
    "4. SPRACHE: DEUTSCH [DE]",
    "5. GERAET ENTKOPPELN (UNPAIR)",
    "6. AUSSCHALTEN / STANDBY",
    "7. WERKSEINSTELLUNGEN (RESET)",
    "8. ZURUECK / SCHLIESSEN"
  };

  const char* menuItemsEN[8] = {
    "1. STATUS & INFO",
    orientEN,
    wifiPwrEN,
    "4. LANGUAGE: ENGLISH [EN]",
    "5. UNPAIR DEVICE",
    "6. POWER OFF / STANDBY",
    "7. FACTORY RESET",
    "8. BACK / CLOSE"
  };

  const char** menuItems = (lang == LANG_DE) ? menuItemsDE : menuItemsEN;

  int startY = 60;
  int itemH = 38;
  int itemGap = 5;
  int itemW = s_epd_width - 60;
  int itemX = 30;

  for (int i = 0; i < 8; i++) {
    int y = startY + i * (itemH + itemGap);
    if (i == selectedIndex) {
      gfx_fill_rect(buffer, itemX, y, itemW, itemH, 0);
      char selText[64];
      snprintf(selText, sizeof(selText), "> %s", menuItems[i]);
      gfx_draw_string(buffer, itemX + 20, y + 11, selText, 1, 2);
    } else {
      gfx_draw_rect(buffer, itemX, y, itemW, itemH, 0);
      char normText[64];
      snprintf(normText, sizeof(normText), "  %s", menuItems[i]);
      gfx_draw_string(buffer, itemX + 20, y + 11, normText, 0, 2);
    }
  }

  // Footer Navigation Hint
  gfx_fill_rect(buffer, 0, s_epd_height - 56, s_epd_width, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 30, s_epd_height - 38, "[ UP / DOWN ]: Navigieren", 1, 2);
    gfx_draw_string(buffer, 500, s_epd_height - 38, "[ OK ]: Auswaehlen", 1, 2);
  } else {
    gfx_draw_string(buffer, 30, s_epd_height - 38, "[ UP / DOWN ]: Navigate", 1, 2);
    gfx_draw_string(buffer, 520, s_epd_height - 38, "[ OK ]: Select", 1, 2);
  }
}

// ─── 4. Status & Diagnostic Info Screen (6 Spacious Rows with Battery) ───────
void renderStatusInfoScreen(uint8_t* buffer, const char* ip, const char* ssid, int rssi, const char* serverUrl, const char* devId, const char* version, Language lang, float tempC, float humidityPct, int batteryPct, bool isCharging, bool isUsbPower) {
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, s_epd_width, 52, 0);
  int titleScale = (batteryPct >= 0 && s_epd_width <= 800) ? 2 : 3;
  int titleY = (titleScale == 2) ? 19 : 15;
  gfx_draw_string(buffer, 20, titleY, "ScreenTinker — Status & Info", 1, titleScale);

  if (batteryPct >= 0) {
    char battBadge[32];
    if (isCharging) {
      snprintf(battBadge, sizeof(battBadge), "[ %d%% CHG ]", batteryPct);
    } else if (isUsbPower) {
      snprintf(battBadge, sizeof(battBadge), "[ %d%% USB ]", batteryPct);
    } else {
      snprintf(battBadge, sizeof(battBadge), "[ %d%% ]", batteryPct);
    }
    int bLen = strlen(battBadge);
    gfx_draw_string(buffer, s_epd_width - (bLen * 16) - 20, 18, battBadge, 1, 2);
  }

  int y = 58;
  int rowH = 48;
  int rowGap = 10;
  int boxW = s_epd_width - 20; // x = 10 to (s_epd_width - 10)
  int boxX = 10;
  int labelX = boxX + 10;   // x = 20
  int valueX = 212;         // x = 212

  // Row 1: IP Address
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 16, (lang == LANG_DE) ? "IP-Adresse:" : "IP Address:", 0, 2);
  const char* defIp = (lang == LANG_DE) ? "Standby (Power Save)" : "Standby (Power Save)";
  const char* ipDisplay = (ip && strlen(ip) > 0 && strcmp(ip, "0.0.0.0") != 0) ? ip : defIp;
  gfx_draw_string(buffer, valueX, y + 16, ipDisplay, 0, 2);

  // Row 2: Wi-Fi SSID & RSSI
  y += rowH + rowGap;
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 16, (lang == LANG_DE) ? "WLAN-Netz:" : "Wi-Fi:", 0, 2);
  char wifiStr[64];
  if (rssi != 0) {
    snprintf(wifiStr, sizeof(wifiStr), "%s (%d dBm)", (ssid && strlen(ssid) > 0) ? ssid : "-", rssi);
  } else {
    snprintf(wifiStr, sizeof(wifiStr), "%s", (ssid && strlen(ssid) > 0) ? ssid : "-");
  }
  gfx_draw_string(buffer, valueX, y + 16, wifiStr, 0, 2);

  // Row 3: Server URL
  y += rowH + rowGap;
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 16, (lang == LANG_DE) ? "Server-URL:" : "Server URL:", 0, 2);
  gfx_draw_string(buffer, valueX, y + 16, (serverUrl && strlen(serverUrl) > 0) ? serverUrl : "-", 0, 2);

  // Row 4: Firmware Version & Ambient Environment
  y += rowH + rowGap;
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 16, "Firmware:", 0, 2);
  char verStr[64];
  if (tempC > -90.0f && humidityPct >= 0.0f && humidityPct <= 100.0f) {
    snprintf(verStr, sizeof(verStr), "v%s (%s) · %.1f C / %.0f%% RH",
             version ? version : FIRMWARE_VERSION, DEVICE_PROFILE_NAME, tempC, humidityPct);
  } else {
    snprintf(verStr, sizeof(verStr), "v%s (%s)",
             version ? version : FIRMWARE_VERSION, DEVICE_PROFILE_NAME);
  }
  gfx_draw_string(buffer, valueX, y + 16, verStr, 0, 2);

  // Row 5: Battery / Power Status
  y += rowH + rowGap;
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 16, (lang == LANG_DE) ? "Akku / Strom:" : "Battery / Power:", 0, 2);
  char battStr[64];
  if (batteryPct >= 0) {
    if (isCharging) {
      snprintf(battStr, sizeof(battStr), (lang == LANG_DE) ? "%d%% (Wird geladen)" : "%d%% (Charging)", batteryPct);
    } else if (isUsbPower) {
      snprintf(battStr, sizeof(battStr), (lang == LANG_DE) ? "%d%% (USB verbunden)" : "%d%% (USB Connected)", batteryPct);
    } else {
      snprintf(battStr, sizeof(battStr), (lang == LANG_DE) ? "%d%% (Batteriebetrieb)" : "%d%% (Discharging)", batteryPct);
    }
  } else {
    if (isUsbPower) {
      snprintf(battStr, sizeof(battStr), (lang == LANG_DE) ? "USB-Netzteil (Kein Akku)" : "USB Power (No Battery)");
    } else {
      snprintf(battStr, sizeof(battStr), (lang == LANG_DE) ? "Nicht verfuegbar" : "Not available");
    }
  }
  gfx_draw_string(buffer, valueX, y + 16, battStr, 0, 2);

  // Row 6: Device ID
  y += rowH + rowGap;
  gfx_draw_rect(buffer, boxX, y, boxW, rowH, 0);
  gfx_draw_string(buffer, labelX, y + 16, (lang == LANG_DE) ? "Device-ID:" : "Device ID:", 0, 2);
  gfx_draw_string(buffer, valueX, y + 16, (devId && strlen(devId) > 0) ? devId : "-", 0, 2);

  // Footer Navigation Hint
  gfx_fill_rect(buffer, 0, s_epd_height - 56, s_epd_width, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 200, s_epd_height - 38, "[ OK / UP / DOWN ]: Zurueck", 1, 2);
  } else {
    gfx_draw_string(buffer, 220, s_epd_height - 38, "[ OK / UP / DOWN ]: Back", 1, 2);
  }
}

// ─── 5. Prominent No Wi-Fi Screen ────────────────────────────────────────────
void renderNoWifiScreen(uint8_t* buffer, const char* ssid, Language lang) {
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, s_epd_width, 54, 0);
  gfx_draw_string(buffer, 30, 16, "ScreenTinker — Offline", 1, 3);

  // Big Warning Box
  gfx_fill_rect(buffer, 40, 80, s_epd_width - 80, 70, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 120, 100, "[!] KEINE WLAN-VERBINDUNG", 1, 3);
  } else {
    gfx_draw_string(buffer, 120, 100, "[!] NO WI-FI CONNECTION", 1, 3);
  }

  // Details
  gfx_draw_rect(buffer, 40, 170, s_epd_width - 80, 80, 0);
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
  gfx_fill_rect(buffer, 0, s_epd_height - 56, s_epd_width, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 30, s_epd_height - 38, "[ OK ]: Neu verbinden", 1, 2);
    gfx_draw_string(buffer, 460, s_epd_height - 38, "[ OK 1.5s ]: Einstellungen", 1, 2);
  } else {
    gfx_draw_string(buffer, 30, s_epd_height - 38, "[ OK ]: Retry", 1, 2);
    gfx_draw_string(buffer, 480, s_epd_height - 38, "[ OK 1.5s ]: Settings", 1, 2);
  }
}

// ─── 5b. Empty Playlist Diagnostic Screen ──────────────────────────────────
void renderEmptyPlaylistScreen(uint8_t* buffer, const char* deviceName, const char* serverUrl, Language lang) {
  gfx_fill(buffer, 1);

  // Top header banner (54px high)
  gfx_fill_rect(buffer, 0, 0, s_epd_width, 54, 0);
  gfx_draw_screentinker_logo(buffer, 20, 12, 1);
  gfx_draw_string(buffer, 56, 16, "ScreenTinker", 1, 3);

  // Device Name Badge in Header
  if (deviceName && strlen(deviceName) > 0) {
    char nameBadge[48];
    snprintf(nameBadge, sizeof(nameBadge), "[ %s ]", deviceName);
    int badgeLen = strlen(nameBadge);
    int badgeX = s_epd_width - (badgeLen * 16) - 20;
    gfx_draw_string(buffer, max(460, badgeX), 18, nameBadge, 1, 2);
  } else {
    const char* subTitle = (lang == LANG_DE) ? "Verbunden" : "Connected";
    gfx_draw_string(buffer, max(300, s_epd_width - 240), 16, subTitle, 1, 3);
  }

  // 1. Prominent Headline Banner (50px high)
  gfx_fill_rect(buffer, 30, 68, s_epd_width - 60, 46, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 48, 81, "KEINE PLAYLIST ZUGEWIESEN", 1, 2);
  } else {
    gfx_draw_string(buffer, 48, 81, "NO PLAYLIST ASSIGNED", 1, 2);
  }

  char dashboardUrl[128];
  if (serverUrl && strlen(serverUrl) > 0) {
    snprintf(dashboardUrl, sizeof(dashboardUrl), "%s", serverUrl);
  } else {
    snprintf(dashboardUrl, sizeof(dashboardUrl), "http://localhost:3001");
  }

  // 2. Left Side & Right Side Responsive Card Layout
  int rightCardW = (s_epd_width >= 1200) ? 360 : 230;
  int rightCardX = s_epd_width - rightCardW - 30;
  int leftCardX = 30;
  int leftCardW = rightCardX - leftCardX - 20;

  // Card A: Status & Guidance (y: 122, height: 154)
  gfx_draw_rect(buffer, leftCardX, 122, leftCardW, 154, 0);

  // Step 1: Device connection
  gfx_draw_string(buffer, leftCardX + 15, 134, (lang == LANG_DE) ? "1. Geraet ist verbunden:" : "1. Display is paired:", 0, 2);
  char devLine[64];
  snprintf(devLine, sizeof(devLine), "   Name: %s", (deviceName && strlen(deviceName) > 0) ? deviceName : "ScreenTinker");
  gfx_draw_string(buffer, leftCardX + 15, 156, devLine, 0, 2);

  // Step 2: Instruction
  gfx_draw_string(buffer, leftCardX + 15, 186, (lang == LANG_DE) ? "2. Playlist zuweisen:" : "2. Assign playlist:", 0, 2);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, leftCardX + 15, 208, "   Im Web-Dashboard unter", 0, 2);
    gfx_draw_string(buffer, leftCardX + 15, 228, "   'Displays' auswaehlen", 0, 2);
    gfx_draw_string(buffer, leftCardX + 15, 248, "   und 'Publish' klicken.", 0, 2);
  } else {
    gfx_draw_string(buffer, leftCardX + 15, 208, "   In dashboard under", 0, 2);
    gfx_draw_string(buffer, leftCardX + 15, 228, "   'Displays' select it,", 0, 2);
    gfx_draw_string(buffer, leftCardX + 15, 248, "   then click 'Publish'.", 0, 2);
  }

  // Card B: Server URL Card (y: 288, height: 76)
  gfx_draw_rect(buffer, leftCardX, 288, leftCardW, 76, 0);
  gfx_draw_string(buffer, leftCardX + 15, 298, (lang == LANG_DE) ? "3. Dashboard aufrufen:" : "3. Open Dashboard:", 0, 2);

  int urlLen = strlen(dashboardUrl);
  int urlScale = (urlLen > 28) ? 1 : 2;
  int charW = (urlScale == 2) ? 16 : 8;
  int urlX = (urlLen * charW < leftCardW - 30) ? (leftCardX + (leftCardW - urlLen * charW) / 2) : (leftCardX + 15);
  int urlY = (urlScale == 2) ? 328 : 332;
  gfx_draw_string(buffer, max(leftCardX + 15, urlX), urlY, dashboardUrl, 0, urlScale);


  // 3. Right Side: Clean QR Code Card
  gfx_draw_rect(buffer, rightCardX, 124, rightCardW, 274, 0);
  int qrScale = (rightCardW >= 300) ? 6 : 4;
  int qrPx = 33 * qrScale;
  int qrX = rightCardX + (rightCardW - qrPx) / 2;
  gfx_draw_qr(buffer, qrX, 144, dashboardUrl, qrScale);

  const char* qrLabel = (lang == LANG_DE) ? "DASHBOARD QR" : "DASHBOARD QR";
  int qrLabelLen = strlen(qrLabel);
  int qrLabelX = rightCardX + (rightCardW - qrLabelLen * 16) / 2;
  gfx_draw_string(buffer, max(rightCardX + 6, qrLabelX), 312, qrLabel, 0, 2);

  const char* qrSub = (lang == LANG_DE) ? "Mit Smartphone oeffnen" : "Scan to open on phone";
  int qrSubLen = strlen(qrSub);
  int qrSubX = rightCardX + (rightCardW - qrSubLen * 8) / 2;
  gfx_draw_string(buffer, max(rightCardX + 6, qrSubX), 350, qrSub, 0, 1);

  // Footer Navigation Bar
  gfx_fill_rect(buffer, 0, s_epd_height - 56, s_epd_width, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 30, s_epd_height - 38, "[ OK ]: Jetzt pruefen", 1, 2);
    gfx_draw_string(buffer, s_epd_width - 320, s_epd_height - 38, "[ OK 1.5s ]: Einstellungen", 1, 2);
  } else {
    gfx_draw_string(buffer, 30, s_epd_height - 38, "[ OK ]: Check now", 1, 2);
  }
}

// ─── 6. Onboarding / Setup Screen ────────────────────────────────────────────
void renderOnboardingScreen(uint8_t* buffer, Language lang) {
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, s_epd_width, 54, 0);
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
  gfx_fill_rect(buffer, 0, s_epd_height - 56, s_epd_width, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 30, s_epd_height - 38, "[ OK 1.5s ]: Einstellungen", 1, 2);
    gfx_draw_string(buffer, 480, s_epd_height - 38, "Warte auf Setup...", 1, 2);
  } else {
    gfx_draw_string(buffer, 30, s_epd_height - 38, "[ OK 1.5s ]: Settings", 1, 2);
    gfx_draw_string(buffer, 480, s_epd_height - 38, "Waiting for setup...", 1, 2);
  }
}

// ─── 7. Power Off & Battery Depleted Screen ──────────────────────────────────
void renderBatteryDepletedScreen(uint8_t* buffer, Language lang) {
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, s_epd_width, 54, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 140, 16, "ScreenTinker — Akku leer (Shutdown)", 1, 3);
  } else {
    gfx_draw_string(buffer, 140, 16, "ScreenTinker — Battery Depleted", 1, 3);
  }

  // Large Battery Icon Graphic on the left
  int bx = 70;
  int by = 130;
  int bw = 200;
  int bh = 110;
  gfx_draw_rect(buffer, bx, by, bw, bh, 0);
  gfx_draw_rect(buffer, bx + 1, by + 1, bw - 2, bh - 2, 0); // 2px outer border
  gfx_fill_rect(buffer, bx + bw, by + 35, 18, 40, 0);       // Positive terminal nipple

  // Critical sliver on left inside battery (14px wide)
  gfx_fill_rect(buffer, bx + 8, by + 8, 14, bh - 16, 0);

  // Prominent Exclamation mark inside battery center
  gfx_draw_string(buffer, bx + 85, by + 23, "!", 0, 8);

  // Details
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 340, 100, "AKKU LEER — SHUTDOWN", 0, 3);
    gfx_draw_string(buffer, 340, 150, "Der Akku ist vollstaendig entladen (<= 1%).", 0, 2);
    gfx_draw_string(buffer, 340, 180, "Das Geraet wurde zum Schutz abgeschaltet.", 0, 2);

    gfx_fill_rect(buffer, 340, 225, 420, 85, 0);
    gfx_draw_string(buffer, 360, 245, "Bitte USB-C Ladegeraet anschliessen,", 1, 2);
    gfx_draw_string(buffer, 360, 275, "um das Geraet wieder aufzuladen!", 1, 2);
  } else {
    gfx_draw_string(buffer, 340, 100, "BATTERY DEPLETED", 0, 3);
    gfx_draw_string(buffer, 340, 150, "The battery is completely depleted (<= 1%).", 0, 2);
    gfx_draw_string(buffer, 340, 180, "Device was shut down to protect the battery.", 0, 2);

    gfx_fill_rect(buffer, 340, 225, 420, 85, 0);
    gfx_draw_string(buffer, 360, 245, "Please connect a USB-C power source", 1, 2);
    gfx_draw_string(buffer, 360, 275, "to recharge and power on!", 1, 2);
  }

  // Footer Navigation Hint
  gfx_fill_rect(buffer, 0, s_epd_height - 56, s_epd_width, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 120, s_epd_height - 38, "[ USB-C ]: Kabel verbinden zum Einschalten & Laden", 1, 2);
  } else {
    gfx_draw_string(buffer, 120, s_epd_height - 38, "[ USB-C ]: Connect power cable to charge & turn on", 1, 2);
  }
}

void renderPowerOffScreen(uint8_t* buffer, Language lang, bool isBatteryDepleted) {
  if (isBatteryDepleted) {
    renderBatteryDepletedScreen(buffer, lang);
    return;
  }
  gfx_fill(buffer, 1);

  // Header Banner
  gfx_fill_rect(buffer, 0, 0, s_epd_width, 54, 0);
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
  gfx_fill_rect(buffer, 0, s_epd_height - 56, s_epd_width, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 180, s_epd_height - 38, "[ OK ]: Aufwecken & Einschalten", 1, 2);
  } else {
    gfx_draw_string(buffer, 180, s_epd_height - 38, "[ OK ]: Wake Up & Turn On", 1, 2);
  }
}

// ─── Offline Badge for Cache-Rotation Overlay ────────────────────────────────
void gfx_draw_offline_badge(uint8_t* buffer) {
  gfx_fill_rect(buffer, s_epd_width - 158, 14, 142, 30, 0);
  gfx_draw_rect(buffer, s_epd_width - 156, 16, 138, 26, 1);
  gfx_draw_string(buffer, s_epd_width - 140, 22, "OFFLINE", 1, 2);
}

// ─── Native Buffer Pixel Helpers for Multi-Panel Color Modes ─────────────────
static void set_native_pixel(uint8_t* buf, int w, int h, int colorMode, int x, int y, uint8_t colorVal) {
  if (!buf || x < 0 || x >= w || y < 0 || y >= h) return;
  if (colorMode == COLOR_MODE_1BPP_MONO) {
    size_t byte_idx = ((size_t)y * (w / 8)) + (x / 8);
    uint8_t bit_mask = 0x80 >> (x & 7);
    if (colorVal) {
      buf[byte_idx] |= bit_mask;
    } else {
      buf[byte_idx] &= ~bit_mask;
    }
  } else if (colorMode == COLOR_MODE_SPECTRA6 || colorMode == COLOR_MODE_GRAY16) {
    size_t stride = (w + 1) / 2;
    size_t byte_idx = (size_t)y * stride + (x >> 1);
    if (x & 1) {
      buf[byte_idx] = (buf[byte_idx] & 0xF0) | (colorVal & 0x0F);
    } else {
      buf[byte_idx] = (buf[byte_idx] & 0x0F) | ((colorVal & 0x0F) << 4);
    }
  } else if (colorMode == COLOR_MODE_GRAY4) {
    size_t stride = (w + 3) / 4;
    size_t byte_idx = (size_t)y * stride + (x >> 2);
    uint8_t shift = 6 - 2 * (x & 3);
    buf[byte_idx] = (buf[byte_idx] & ~(0x03 << shift)) | ((colorVal & 0x03) << shift);
  }
}

static void fill_native_rect(uint8_t* buf, int w, int h, int colorMode, int rx, int ry, int rw, int rh, uint8_t colorVal) {
  for (int j = 0; j < rh; j++) {
    for (int i = 0; i < rw; i++) {
      set_native_pixel(buf, w, h, colorMode, rx + i, ry + j, colorVal);
    }
  }
}

static void draw_native_rect(uint8_t* buf, int w, int h, int colorMode, int rx, int ry, int rw, int rh, uint8_t colorVal) {
  for (int i = 0; i < rw; i++) {
    set_native_pixel(buf, w, h, colorMode, rx + i, ry, colorVal);
    set_native_pixel(buf, w, h, colorMode, rx + i, ry + rh - 1, colorVal);
  }
  for (int j = 0; j < rh; j++) {
    set_native_pixel(buf, w, h, colorMode, rx, ry + j, colorVal);
    set_native_pixel(buf, w, h, colorMode, rx + rw - 1, ry + j, colorVal);
  }
}

// ─── Low Battery Badge Icon Overlay (< 10%) ──────────────────────────────────
void gfx_draw_low_battery_icon_native(uint8_t* buffer, int width, int height, int colorMode, int x, int y, int batteryPct) {
  if (!buffer) return;
  if (width <= 0) width = s_epd_width;
  if (height <= 0) height = s_epd_height;

  int boxW = 44;
  int boxH = 22;

  if (x < 0) x = width - boxW - 14;
  if (y < 0) y = 14;

  uint8_t blackVal = 0x00;
  uint8_t whiteVal = (colorMode == COLOR_MODE_SPECTRA6) ? 0x01 :
                     ((colorMode == COLOR_MODE_GRAY16) ? 0x0F :
                     ((colorMode == COLOR_MODE_GRAY4) ? 0x03 : 1));
  uint8_t alertVal = (colorMode == COLOR_MODE_SPECTRA6) ? 0x03 : whiteVal; // Red on Spectra 6, else White

  // 1. High-contrast solid black background box
  fill_native_rect(buffer, width, height, colorMode, x, y, boxW, boxH, blackVal);

  // 2. 1px white border around the badge box
  draw_native_rect(buffer, width, height, colorMode, x, y, boxW, boxH, whiteVal);

  // 3. Battery body shell (white outline)
  int bx = x + 5;
  int by = y + 4;
  int bw = 28;
  int bh = 14;
  draw_native_rect(buffer, width, height, colorMode, bx, by, bw, bh, whiteVal);

  // 4. Positive terminal nipple on the right (white filled)
  fill_native_rect(buffer, width, height, colorMode, bx + bw, by + 3, 3, 8, whiteVal);

  // 5. Critical level sliver on the left inside battery shell (3px wide)
  fill_native_rect(buffer, width, height, colorMode, bx + 2, by + 2, 3, bh - 4, alertVal);

  // 6. Exclamation mark in the center of the battery shell
  int exX = bx + 14;
  fill_native_rect(buffer, width, height, colorMode, exX, by + 2, 2, 6, alertVal); // Stem
  fill_native_rect(buffer, width, height, colorMode, exX, by + 9, 2, 2, alertVal); // Dot
}

void gfx_draw_low_battery_icon(uint8_t* buffer, int x, int y, int batteryPct) {
  if (!buffer) return;
  if (x < 0) x = s_epd_width - 58;
  if (y < 0) y = 14;
#ifdef DEVICE_COLOR_MODE
  gfx_draw_low_battery_icon_native(buffer, s_epd_width, s_epd_height, DEVICE_COLOR_MODE, x, y, batteryPct);
#else
  gfx_draw_low_battery_icon_native(buffer, s_epd_width, s_epd_height, COLOR_MODE_1BPP_MONO, x, y, batteryPct);
#endif
}

// ─── Unsupported Content (501 / Missing Browser) Screen ─────────────────────
void renderUnsupportedContentScreen(uint8_t* buffer, const char* reason, Language lang) {
  gfx_fill(buffer, 1); // White background

  // Header Bar
  gfx_draw_screentinker_logo(buffer, 30, 20, 2);
  gfx_draw_string(buffer, 85, 26, "ScreenTinker", 0, 2);
  gfx_draw_string(buffer, max(300, s_epd_width - 200), 26, (lang == LANG_DE) ? "HINWEIS" : "NOTICE", 0, 2);
  gfx_draw_hline(buffer, 30, 56, s_epd_width - 60, 0);

  // Black Banner
  gfx_fill_rect(buffer, 30, 68, s_epd_width - 60, 46, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 48, 81, "INHALT NICHT UNTERSTUETZT (501)", 1, 2);
  } else {
    gfx_draw_string(buffer, 48, 81, "UNSUPPORTED CONTENT TYPE (501)", 1, 2);
  }

  // Main Card
  gfx_draw_rect(buffer, 30, 122, s_epd_width - 60, 276, 0);

  // Guidance text
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 50, 140, "Dieses E-Paper Display benoetigt vorbereitete Bilddaten.", 0, 2);
    gfx_draw_string(buffer, 50, 165, "Web-Widgets (HTML/Kalender/Uhr) erfordern einen Server-Browser.", 0, 2);
  } else {
    gfx_draw_string(buffer, 50, 140, "This e-paper display requires rendered image bitmaps.", 0, 2);
    gfx_draw_string(buffer, 50, 165, "Web widgets or HTML require a headless browser on server.", 0, 2);
  }

  // Server message box
  gfx_draw_rect(buffer, 50, 195, s_epd_width - 100, 75, 0);
  gfx_draw_string(buffer, 65, 205, (lang == LANG_DE) ? "Server-Rueckmeldung:" : "Server Message:", 0, 1);
  const char* msg = (reason && strlen(reason) > 0) ? reason : "Widget rendering requires a browser (set CHROME_PATH).";
  
  char line1[64] = {0};
  char line2[64] = {0};
  int msgLen = strlen(msg);
  if (msgLen <= 40) {
    snprintf(line1, sizeof(line1), "%s", msg);
  } else {
    int split = 38;
    while (split > 20 && msg[split] != ' ') split--;
    snprintf(line1, sizeof(line1), "%.*s", split, msg);
    snprintf(line2, sizeof(line2), "%s", msg + split + (msg[split] == ' ' ? 1 : 0));
  }
  gfx_draw_string(buffer, 65, 225, line1, 0, 2);
  if (line2[0]) {
    gfx_draw_string(buffer, 65, 245, line2, 0, 2);
  }

  // Action steps
  gfx_draw_string(buffer, 50, 285, (lang == LANG_DE) ? "Loesungsschritte:" : "How to resolve:", 0, 2);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 50, 310, "1. Playlist im Dashboard auf Bilddateien (PNG/JPG) umstellen, ODER", 0, 2);
    gfx_draw_string(buffer, 50, 335, "2. Chromium auf dem ScreenTinker Server installieren (CHROME_PATH).", 0, 2);
  } else {
    gfx_draw_string(buffer, 50, 310, "1. In Dashboard: Use image slides (PNG/JPG) in this playlist, OR", 0, 2);
    gfx_draw_string(buffer, 50, 335, "2. On Server: Install Chromium and configure CHROME_PATH.", 0, 2);
  }

  // Footer bar
  gfx_fill_rect(buffer, 0, s_epd_height - 56, s_epd_width, 56, 0);
  gfx_draw_string(buffer, 120, s_epd_height - 38, (lang == LANG_DE) ? "Automatische Neupruefung beim naechsten Abrufzyklus" : "Display will automatically re-check on next sync interval", 1, 2);
}

// ─── 8. Over-The-Air (OTA) Firmware Update Screen ───────────────────────────
void renderOtaUpdateScreen(uint8_t* buffer, const char* statusMsg, Language lang) {
  gfx_fill(buffer, 1);

  // Top header banner (54px high)
  gfx_fill_rect(buffer, 0, 0, s_epd_width, 54, 0);
  gfx_draw_screentinker_logo(buffer, 20, 12, 1);
  gfx_draw_string(buffer, 56, 16, "ScreenTinker", 1, 3);

  const char* title = (lang == LANG_DE) ? "Firmware-Update" : "Firmware Update";
  gfx_draw_string(buffer, max(300, s_epd_width - 280), 16, title, 1, 3);

  // Prominent Headline Banner (46px high)
  gfx_fill_rect(buffer, 30, 68, s_epd_width - 60, 46, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 48, 81, "FIRMWARE-UPDATE LAEUFT", 1, 2);
  } else {
    gfx_draw_string(buffer, 48, 81, "FIRMWARE UPDATE IN PROGRESS", 1, 2);
  }

  // Main Card
  gfx_draw_rect(buffer, 30, 122, s_epd_width - 60, 276, 0);

  // Warning text
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 50, 150, "Das System aktualisiert die Geraete-Firmware (OTA).", 0, 2);
    gfx_draw_string(buffer, 50, 185, "BITTE DAS GERAET NICHT AUSSCHALTEN!", 0, 2);
  } else {
    gfx_draw_string(buffer, 50, 150, "The system is updating device firmware over the air.", 0, 2);
    gfx_draw_string(buffer, 50, 185, "DO NOT POWER OFF OR DISCONNECT POWER!", 0, 2);
  }

  // Status message box
  if (statusMsg && strlen(statusMsg) > 0) {
    gfx_draw_rect(buffer, 50, 230, s_epd_width - 100, 60, 0);
    gfx_draw_string(buffer, 65, 252, statusMsg, 0, 2);
  }

  // Footer bar
  gfx_fill_rect(buffer, 0, s_epd_height - 56, s_epd_width, 56, 0);
  if (lang == LANG_DE) {
    gfx_draw_string(buffer, 60, s_epd_height - 38, "Automatischer Neustart nach Abschluss des Downloads...", 1, 2);
  } else {
    gfx_draw_string(buffer, 60, s_epd_height - 38, "Device will automatically restart when complete...", 1, 2);
  }
}

void renderImuDebugScreen(uint8_t* buffer, int orient, int16_t ax, int16_t ay, int16_t az) {
  if (!buffer) return;
  gfx_fill(buffer, 1); // White background

  int w = gfx_get_width();
  int h = gfx_get_height();

  // Double border
  gfx_draw_rect(buffer, 10, 10, w - 20, h - 20, 0);
  gfx_draw_rect(buffer, 14, 14, w - 28, h - 28, 0);

  // Header
  gfx_draw_string(buffer, 40, 30, "SCREENTINKER IMU CALIBRATION", 0, 2);
  gfx_draw_hline(buffer, 40, 55, w - 80, 0);

  // Large Letter A, B, C, D in center
  char letter = 'A' + (orient >= 0 && orient <= 3 ? orient : 0);

  // Scale 22: 8 * 22 = 176 pixels wide and tall
  int char_w = 8 * 22;
  int char_x = (w - char_w) / 2;
  int char_y = 75;

  // Draw background box for letter
  gfx_draw_rect(buffer, char_x - 20, char_y - 10, char_w + 40, char_w + 20, 0);
  gfx_draw_rect(buffer, char_x - 18, char_y - 8, char_w + 36, char_w + 16, 0);
  gfx_draw_char(buffer, char_x, char_y, letter, 0, 22);

  // Caption below letter
  char letterCaption[48];
  snprintf(letterCaption, sizeof(letterCaption), "[ LETTER '%c' ]", letter);
  gfx_draw_string(buffer, (w - (strlen(letterCaption) * 8 * 3)) / 2, char_y + char_w + 25, letterCaption, 0, 3);

  // Orientation name
  const char* orientNames[] = {
    "ID 0: LANDSCAPE_0 (0 deg)",
    "ID 1: PORTRAIT_90 (90 deg)",
    "ID 2: LANDSCAPE_180 (180 deg)",
    "ID 3: PORTRAIT_270 (270 deg)"
  };
  const char* name = (orient >= 0 && orient <= 3) ? orientNames[orient] : "UNKNOWN";
  gfx_draw_string(buffer, 40, h - 90, name, 0, 2);

  // Accel info
  char accelInfo[96];
  snprintf(accelInfo, sizeof(accelInfo), "Raw Accel: ax=%d, ay=%d, az=%d", (int)ax, (int)ay, (int)az);
  gfx_draw_string(buffer, 40, h - 60, accelInfo, 0, 2);

  gfx_draw_string(buffer, 40, h - 35, "Rotate device 90 deg to inspect all 4 orientations (A, B, C, D)", 0, 1);
}

// ─── Display Buffer Rotation Algorithms ──────────────────────────────────────
void rotate_mono_180(const uint8_t *source, uint8_t *destination, uint16_t width, uint16_t height) {
  const size_t stride = width / 8U;
  for (uint16_t y = 0; y < height; ++y) {
    const size_t src_row = y * stride;
    const size_t dst_row = (height - 1U - y) * stride;
    for (size_t x = 0; x < stride; ++x) {
      uint8_t b = source[src_row + (stride - 1U - x)];
      b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
      b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
      b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
      destination[dst_row + x] = b;
    }
  }
}

void rotate_nibble_180(const uint8_t *source, uint8_t *destination, uint16_t width, uint16_t height) {
  if (!source || !destination) return;
  const size_t stride = (width + 1) / 2;
  memset(destination, 0, stride * height);
  for (uint16_t y = 0; y < height; ++y) {
    for (uint16_t x = 0; x < width; ++x) {
      uint8_t src_byte = source[y * stride + (x >> 1)];
      uint8_t nibble = (x & 1) ? (src_byte & 0x0F) : (src_byte >> 4);
      uint16_t dx = width - 1 - x;
      uint16_t dy = height - 1 - y;
      if (dx & 1) {
        destination[dy * stride + (dx >> 1)] |= (nibble & 0x0F);
      } else {
        destination[dy * stride + (dx >> 1)] |= (nibble << 4);
      }
    }
  }
}

void rotate_mono_portrait(const uint8_t *src, uint8_t *dst, uint16_t src_w, uint16_t src_h, bool flip) {
  if (!src || !dst) return;
  const size_t src_stride = src_w / 8U;
  const size_t dst_stride = src_h / 8U;
  memset(dst, 0xFF, dst_stride * src_w);

  for (uint16_t sy = 0; sy < src_h; ++sy) {
    const size_t src_row = sy * src_stride;
    for (uint16_t sx = 0; sx < src_w; ++sx) {
      uint8_t byte = src[src_row + (sx >> 3)];
      uint8_t bit = (byte >> (7 - (sx & 7))) & 1;
      if (!bit) {
        uint16_t dx, dy;
        if (!flip) {
          dx = (src_h - 1U) - sy;
          dy = sx;
        } else {
          dx = sy;
          dy = (src_w - 1U) - sx;
        }
        dst[dy * dst_stride + (dx >> 3)] &= ~(1 << (7 - (dx & 7)));
      }
    }
  }
}

void rotate_nibble_portrait(const uint8_t *src, uint8_t *dst, uint16_t src_w, uint16_t src_h, bool flip) {
  if (!src || !dst) return;
  const size_t src_stride = (src_w + 1) / 2;
  const size_t dst_stride = (src_h + 1) / 2;
  memset(dst, 0, dst_stride * src_w);

  for (uint16_t sy = 0; sy < src_h; ++sy) {
    const size_t src_row = sy * src_stride;
    for (uint16_t sx = 0; sx < src_w; ++sx) {
      uint8_t src_byte = src[src_row + (sx >> 1)];
      uint8_t nibble = (sx & 1) ? (src_byte & 0x0F) : (src_byte >> 4);
      uint16_t dx, dy;
      if (!flip) {
        dx = (src_h - 1U) - sy;
        dy = sx;
      } else {
        dx = sy;
        dy = (src_w - 1U) - sx;
      }
      if (dx & 1) {
        dst[dy * dst_stride + (dx >> 1)] |= (nibble & 0x0F);
      } else {
        dst[dy * dst_stride + (dx >> 1)] |= (nibble << 4);
      }
    }
  }
}

void rotate_buffer_180(const uint8_t *source, uint8_t *destination, uint16_t width, uint16_t height) {
#if (DEVICE_COLOR_MODE == COLOR_MODE_SPECTRA6) || (DEVICE_COLOR_MODE == COLOR_MODE_GRAY16)
  rotate_nibble_180(source, destination, width, height);
#else
  rotate_mono_180(source, destination, width, height);
#endif
}







