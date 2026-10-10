// Standalone native harness that renders all ScreenTinker E-Paper screens to 1-bit BMP / PNG
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define EPD_WIDTH 800
#define EPD_HEIGHT 480
#define EPD_BUFFER_SIZE (EPD_WIDTH * EPD_HEIGHT / 8)
#define FIRMWARE_VERSION "1.2.0"
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

#include "../include/setup_screen.h"

// Write 1-bit BMP
static void save_bmp(const char* filename, const uint8_t* packed, int w, int h) {
  int rowBytes  = (w + 7) / 8;
  int rowStride = ((rowBytes + 3) / 4) * 4;
  int pixelData = rowStride * h;
  int fileSize  = 62 + pixelData;

  uint8_t header[62] = {0};
  // BM
  header[0] = 'B'; header[1] = 'M';
  *(uint32_t*)&header[2] = fileSize;
  *(uint32_t*)&header[10] = 62; // offset
  // DIB Header
  *(uint32_t*)&header[14] = 40;
  *(int32_t*)&header[18]  = w;
  *(int32_t*)&header[22]  = -h; // top-down
  *(uint16_t*)&header[26] = 1;
  *(uint16_t*)&header[28] = 1;
  *(uint32_t*)&header[30] = 0; // BI_RGB
  *(uint32_t*)&header[34] = pixelData;
  *(int32_t*)&header[38]  = 2835;
  *(int32_t*)&header[42]  = 2835;
  *(uint32_t*)&header[46] = 2; // colors
  *(uint32_t*)&header[50] = 2;
  // Color table (0: black, 1: white)
  header[54] = 0;   header[55] = 0;   header[56] = 0;   header[57] = 0;
  header[58] = 255; header[59] = 255; header[60] = 255; header[61] = 0;

  FILE* f = fopen(filename, "wb");
  if (!f) {
    fprintf(stderr, "Failed to open %s for writing\n", filename);
    return;
  }
  fwrite(header, 1, 62, f);

  uint8_t* rowBuf = (uint8_t*)calloc(1, rowStride);
  for (int y = 0; y < h; y++) {
    const uint8_t* src = packed + (y * rowBytes);
    memcpy(rowBuf, src, rowBytes);
    fwrite(rowBuf, 1, rowStride, f);
  }
  free(rowBuf);
  fclose(f);
  printf("Saved: %s\n", filename);
}

int main(int argc, char** argv) {
  uint8_t* buf = (uint8_t*)malloc(EPD_BUFFER_SIZE);
  if (!buf) return 1;

  const char* outDir = (argc > 1) ? argv[1] : "docs/screens";

  char path[512];

  // 1. Splash Screen
  renderSplashScreen(buf, "1.2.0", "Connecting to network...", LANG_EN);
  snprintf(path, sizeof(path), "%s/01_splash_en.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  renderSplashScreen(buf, "1.2.0", "Verbindung wird aufgebaut...", LANG_DE);
  snprintf(path, sizeof(path), "%s/01_splash_de.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  // 2. Pairing Code Screen
  renderPairingCodeScreen(buf, "822661", "http://192.168.100.222:3001", LANG_EN);
  snprintf(path, sizeof(path), "%s/02_pairing_code_en.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  renderPairingCodeScreen(buf, "822661", "http://192.168.100.222:3001", LANG_DE);
  snprintf(path, sizeof(path), "%s/02_pairing_code_de.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  // 3. Empty Playlist Screen
  renderEmptyPlaylistScreen(buf, "sticky120", "http://192.168.100.222:3001", LANG_EN);
  snprintf(path, sizeof(path), "%s/03_empty_playlist_en.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  renderEmptyPlaylistScreen(buf, "sticky120", "http://192.168.100.222:3001", LANG_DE);
  snprintf(path, sizeof(path), "%s/03_empty_playlist_de.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  // 4. No Wi-Fi Screen
  renderNoWifiScreen(buf, "smarthome", LANG_EN);
  snprintf(path, sizeof(path), "%s/04_no_wifi_en.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  renderNoWifiScreen(buf, "smarthome", LANG_DE);
  snprintf(path, sizeof(path), "%s/04_no_wifi_de.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  // 5. System Menu
  renderSystemMenu(buf, 0, LANG_EN, 0);
  snprintf(path, sizeof(path), "%s/05_system_menu_en.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  renderSystemMenu(buf, 0, LANG_DE, 0);
  snprintf(path, sizeof(path), "%s/05_system_menu_de.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  // 6. Status Info Screen
  renderStatusInfoScreen(buf, "192.168.20.180", "smarthome", -85, "http://192.168.100.222:3001", "06813352-374a-476b-bca9-986ebdb1ff2c", "1.2.0", LANG_EN);
  snprintf(path, sizeof(path), "%s/06_status_info_en.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  renderStatusInfoScreen(buf, "192.168.20.180", "smarthome", -85, "http://192.168.100.222:3001", "06813352-374a-476b-bca9-986ebdb1ff2c", "1.2.0", LANG_DE);
  snprintf(path, sizeof(path), "%s/06_status_info_de.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  // 7. Onboarding SoftAP Screen
  renderOnboardingScreen(buf, LANG_EN);
  snprintf(path, sizeof(path), "%s/07_onboarding_en.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  renderOnboardingScreen(buf, LANG_DE);
  snprintf(path, sizeof(path), "%s/07_onboarding_de.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  // 8. Power Off Screen
  renderPowerOffScreen(buf, LANG_EN);
  snprintf(path, sizeof(path), "%s/08_power_off_en.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  renderPowerOffScreen(buf, LANG_DE);
  snprintf(path, sizeof(path), "%s/08_power_off_de.bmp", outDir);
  save_bmp(path, buf, EPD_WIDTH, EPD_HEIGHT);

  free(buf);
  return 0;
}
