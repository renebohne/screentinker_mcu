#include "ota_updater.h"
#include "diag_logger.h"
#include "config.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>

// Keep confirm control in application code so setup() observes PENDING_VERIFY
extern "C" bool verifyRollbackLater() {
  return true;
}
#else
#include <string>
#include <stdlib.h>
#include <stdint.h>

// Standalone SHA-256 implementation for host / simulator
class HostSha256 {
private:
  uint32_t state[8];
  uint64_t count;
  uint8_t buffer[64];

  static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
  static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
  static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
  static uint32_t sig0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
  static uint32_t sig1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
  static uint32_t gam0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
  static uint32_t gam1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

  void transform(const uint8_t data[64]) {
    static const uint32_t K[64] = {
      0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
      0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
      0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
      0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
      0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
      0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
      0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
      0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t W[64];
    for (int i = 0; i < 16; ++i) {
      W[i] = ((uint32_t)data[i*4] << 24) | ((uint32_t)data[i*4+1] << 16) | ((uint32_t)data[i*4+2] << 8) | ((uint32_t)data[i*4+3]);
    }
    for (int i = 16; i < 64; ++i) {
      W[i] = gam1(W[i-2]) + W[i-7] + gam0(W[i-15]) + W[i-16];
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; ++i) {
      uint32_t T1 = h + sig1(e) + ch(e, f, g) + K[i] + W[i];
      uint32_t T2 = sig0(a) + maj(a, b, c);
      h = g; g = f; f = e; e = d + T1;
      d = c; c = b; b = a; a = T1 + T2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
  }

public:
  HostSha256() { reset(); }
  void reset() {
    state[0] = 0x6a09e667; state[1] = 0xbb67ae85; state[2] = 0x3c6ef372; state[3] = 0xa54ff53a;
    state[4] = 0x510e527f; state[5] = 0x9b05688c; state[6] = 0x1f83d9ab; state[7] = 0x5be0cd19;
    count = 0;
  }
  void update(const uint8_t* data, size_t len) {
    size_t idx = (size_t)(count & 63);
    count += len;
    size_t partLen = 64 - idx;
    size_t i = 0;
    if (len >= partLen) {
      memcpy(&buffer[idx], data, partLen);
      transform(buffer);
      for (i = partLen; i + 63 < len; i += 64) {
        transform(&data[i]);
      }
      idx = 0;
    }
    if (i < len) {
      memcpy(&buffer[idx], &data[i], len - i);
    }
  }
  std::string finish() {
    uint8_t finalCount[8];
    for (int i = 0; i < 8; ++i) {
      finalCount[i] = (uint8_t)((count * 8) >> ((7 - i) * 8));
    }
    uint8_t pad = 0x80;
    update(&pad, 1);
    uint8_t zero = 0;
    while ((count & 63) != 56) {
      update(&zero, 1);
    }
    update(finalCount, 8);
    char hex[65];
    for (int i = 0; i < 8; ++i) {
      snprintf(&hex[i * 8], 9, "%08x", state[i]);
    }
    hex[64] = '\0';
    return std::string(hex);
  }
};
#endif

static OtaState s_state = {
  .running_partition = "app0",
  .next_partition = "app1",
  .is_rollback_possible = false,
  .is_pending_verification = false,
  .status = OTA_STATUS_IDLE,
  .progress_percent = 0,
  .error_msg = {0}
};

static bool is_valid_sha256_hex(const char* s) {
  if (!s || strlen(s) != 64) return false;
  for (int i = 0; i < 64; ++i) {
    char c = s[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
      return false;
    }
  }
  return true;
}

void ota_init(void) {
  s_state.status = OTA_STATUS_IDLE;
  s_state.progress_percent = 0;
  s_state.error_msg[0] = '\0';

#ifdef ARDUINO
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);

  if (running) {
    s_state.running_partition = running->label;
  }
  if (next) {
    s_state.next_partition = next->label;
  }

  esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
  if (running && esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    s_state.is_pending_verification = (ota_state == ESP_OTA_IMG_PENDING_VERIFY);
    s_state.is_rollback_possible = (ota_state == ESP_OTA_IMG_PENDING_VERIFY || ota_state == ESP_OTA_IMG_VALID);
    diag_log_printf(DIAG_LOG_INFO, "OTA", "Running partition '%s', next '%s', state=%d",
                    s_state.running_partition, s_state.next_partition, (int)ota_state);

    if (s_state.is_pending_verification) {
      diag_log_printf(DIAG_LOG_WARN, "OTA", "Firmware is PENDING VERIFICATION. Waiting for successful sync.");
      Serial.println("[OTA] Current firmware is PENDING VERIFICATION. Waiting for successful sync before confirming.");
    }
  } else {
    diag_log_printf(DIAG_LOG_INFO, "OTA", "Running factory/initial partition '%s', next '%s'",
                    s_state.running_partition, s_state.next_partition);
  }
#else
  s_state.is_pending_verification = false;
  diag_log_printf(DIAG_LOG_INFO, "OTA", "OTA initialized (simulator/host mode)");
#endif
}

bool ota_confirm_running_partition(void) {
#ifdef ARDUINO
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
  if (running && esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
      if (err == ESP_OK) {
        s_state.is_pending_verification = false;
        diag_log_printf(DIAG_LOG_INFO, "OTA", "Firmware confirmed as VALID! Rollback cancelled.");
        Serial.println("[OTA] Firmware successfully confirmed as VALID! Rollback cancelled.");
        return true;
      } else {
        diag_log_printf(DIAG_LOG_ERROR, "OTA", "Failed to cancel rollback: %s", esp_err_to_name(err));
        return false;
      }
    }
  }
  return false;
#else
  if (!s_state.is_pending_verification) {
    return false;
  }
  s_state.is_pending_verification = false;
  diag_log_printf(DIAG_LOG_INFO, "OTA", "Simulated firmware confirmed as VALID.");
  return true;
#endif
}

void ota_set_pending_verification_for_test(bool pending) {
  s_state.is_pending_verification = pending;
}

void ota_rollback_and_reboot(void) {
#ifdef ARDUINO
  diag_log_printf(DIAG_LOG_ERROR, "OTA", "Triggering rollback to previous partition and restarting...");
  Serial.println("[OTA] Triggering rollback to previous partition and restarting...");
  esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
  if (err != ESP_OK) {
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "Rollback failed (%s), forcing restart...", esp_err_to_name(err));
    Serial.printf("[OTA] Rollback failed (%s), forcing restart...\n", esp_err_to_name(err));
    esp_restart();
  }
#else
  diag_log_printf(DIAG_LOG_WARN, "OTA", "Simulated rollback triggered");
  s_state.is_pending_verification = false;
#endif
}

bool ota_perform_update(const char* url, const char* expected_sha256) {
  if (!url || !*url) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Invalid or empty update URL");
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    return false;
  }

  if (!is_valid_sha256_hex(expected_sha256)) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Invalid or missing SHA-256 hash (expected 64 hex characters)");
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    return false;
  }

  s_state.status = OTA_STATUS_DOWNLOADING;
  s_state.progress_percent = 0;
  diag_log_printf(DIAG_LOG_INFO, "OTA", "Starting OTA download from %s", url);

#ifdef ARDUINO
  Serial.printf("[OTA] Starting OTA download from %s\n", url);
  HTTPClient http;
  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS * 3); // 30s timeout for firmware transfer
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "HTTP GET failed: %d", httpCode);
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    http.end();
    return false;
  }

  int contentLength = http.getSize();
  if (contentLength <= 0) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Invalid Content-Length: %d", contentLength);
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    http.end();
    return false;
  }

  bool canBegin = Update.begin(contentLength, U_FLASH);
  if (!canBegin) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Not enough flash space for %d bytes", contentLength);
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = 0;
  uint8_t buff[1024];

  mbedtls_sha256_context sha_ctx;
  mbedtls_sha256_init(&sha_ctx);
  mbedtls_sha256_starts(&sha_ctx, 0); // 0 = SHA-256

  int lastReport = -1;
  uint32_t lastByteTime = millis();
  const uint32_t STALL_TIMEOUT_MS = 10000;

  while (http.connected() && (written < (size_t)contentLength)) {
    size_t available = stream->available();
    if (available > 0) {
      lastByteTime = millis();
      size_t toRead = available < sizeof(buff) ? available : sizeof(buff);
      size_t bytesRead = stream->readBytes(buff, toRead);
      if (bytesRead > 0) {
        mbedtls_sha256_update(&sha_ctx, buff, bytesRead);
        size_t bytesWritten = Update.write(buff, bytesRead);
        if (bytesWritten != bytesRead) {
          s_state.status = OTA_STATUS_ERROR;
          snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Flash write failed at offset %u", (unsigned)written);
          diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
          Update.abort();
          mbedtls_sha256_free(&sha_ctx);
          http.end();
          return false;
        }
        written += bytesWritten;
        yield();
        s_state.progress_percent = (int)((written * 100) / contentLength);
        if (s_state.progress_percent != lastReport && s_state.progress_percent % 10 == 0) {
          lastReport = s_state.progress_percent;
          Serial.printf("[OTA] Progress: %d%% (%u / %d bytes)\n", s_state.progress_percent, (unsigned)written, contentLength);
        }
      }
    } else {
      if (millis() - lastByteTime > STALL_TIMEOUT_MS) {
        s_state.status = OTA_STATUS_ERROR;
        snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Download stalled: timeout waiting for data");
        diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
        Update.abort();
        mbedtls_sha256_free(&sha_ctx);
        http.end();
        return false;
      }
      delay(10);
      yield();
    }
  }

  http.end();

  if (written != (size_t)contentLength) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Incomplete transfer (%u / %d bytes)", (unsigned)written, contentLength);
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    Update.abort();
    mbedtls_sha256_free(&sha_ctx);
    return false;
  }

  s_state.status = OTA_STATUS_VERIFYING;
  unsigned char output[32];
  mbedtls_sha256_finish(&sha_ctx, output);
  mbedtls_sha256_free(&sha_ctx);
  char hexOutput[65];
  for (int i = 0; i < 32; i++) {
    snprintf(&hexOutput[i * 2], 3, "%02x", output[i]);
  }
  hexOutput[64] = '\0';
  if (strcasecmp(hexOutput, expected_sha256) != 0) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "SHA-256 mismatch (expected %s, got %s)", expected_sha256, hexOutput);
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    Update.abort();
    return false;
  }
  diag_log_printf(DIAG_LOG_INFO, "OTA", "SHA-256 verified successfully: %s", hexOutput);

  if (!Update.end(true)) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Update.end failed: code %u", Update.getError());
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    return false;
  }

  if (!Update.isFinished()) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Update was not marked finished");
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    return false;
  }

  s_state.status = OTA_STATUS_SUCCESS;
  s_state.progress_percent = 100;
  s_state.is_pending_verification = true;
  diag_log_printf(DIAG_LOG_WARN, "OTA", "OTA successfully written! Rebooting into new firmware...");
  Serial.println("[OTA] Firmware update complete! Restarting in 1s...");
  delay(1000);
  ESP.restart();
  return true;

#else
  // Simulator / native host mock: execute real curl download and verify SHA-256
  std::string safeUrl = url;
  std::string escapedUrl;
  for (char c : safeUrl) {
    if (c == '\'') escapedUrl += "'\\''";
    else escapedUrl += c;
  }
  std::string cmd = "curl -s -f -L --max-time 10 '" + escapedUrl + "'";
  FILE* fp = popen(cmd.c_str(), "r");
  if (!fp) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Failed to execute curl");
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    return false;
  }
  HostSha256 hasher;
  uint8_t chunk[4096];
  size_t totalBytes = 0;
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
    hasher.update(chunk, n);
    totalBytes += n;
  }
  int status = pclose(fp);
  if (status != 0 || totalBytes == 0) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Download failed or empty body (curl exit code %d, %zu bytes)", status, totalBytes);
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    return false;
  }

  std::string computedSha = hasher.finish();
  if (strcasecmp(computedSha.c_str(), expected_sha256) != 0) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "SHA-256 mismatch (expected %s, got %s)", expected_sha256, computedSha.c_str());
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    return false;
  }

  s_state.status = OTA_STATUS_SUCCESS;
  s_state.progress_percent = 100;
  s_state.is_pending_verification = true;
  diag_log_printf(DIAG_LOG_INFO, "OTA", "Simulated OTA fetch and flash complete (%zu bytes, SHA-256 verified: %s)", totalBytes, computedSha.c_str());
  return true;
#endif
}

void ota_get_state(OtaState* out_state) {
  if (out_state) {
    *out_state = s_state;
  }
}
