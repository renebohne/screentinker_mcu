#include "ota_updater.h"
#include "diag_logger.h"
#include "config.h"

#include <string.h>
#include <stdio.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>
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
#else
  s_state.is_pending_verification = false;
#endif
  return true;
}

void ota_rollback_and_reboot(void) {
#ifdef ARDUINO
  diag_log_printf(DIAG_LOG_ERROR, "OTA", "Triggering rollback to previous partition and restarting...");
  Serial.println("[OTA] Triggering rollback to previous partition and restarting...");
  esp_ota_mark_app_invalid_rollback_and_reboot();
#else
  diag_log_printf(DIAG_LOG_WARN, "OTA", "Simulated rollback triggered");
#endif
}

bool ota_perform_update(const char* url, const char* expected_sha256) {
  if (!url || !*url) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Invalid or empty update URL");
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
  bool check_sha = (expected_sha256 && strlen(expected_sha256) == 64);
  if (check_sha) {
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0); // 0 = SHA-256
  }

  int lastReport = -1;
  while (http.connected() && (written < (size_t)contentLength)) {
    size_t available = stream->available();
    if (available) {
      size_t toRead = available < sizeof(buff) ? available : sizeof(buff);
      size_t bytesRead = stream->readBytes(buff, toRead);
      if (bytesRead > 0) {
        if (check_sha) {
          mbedtls_sha256_update(&sha_ctx, buff, bytesRead);
        }
        size_t bytesWritten = Update.write(buff, bytesRead);
        if (bytesWritten != bytesRead) {
          s_state.status = OTA_STATUS_ERROR;
          snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Flash write failed at offset %u", (unsigned)written);
          diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
          Update.abort();
          if (check_sha) mbedtls_sha256_free(&sha_ctx);
          http.end();
          return false;
        }
        written += bytesWritten;
        s_state.progress_percent = (int)((written * 100) / contentLength);
        if (s_state.progress_percent != lastReport && s_state.progress_percent % 10 == 0) {
          lastReport = s_state.progress_percent;
          Serial.printf("[OTA] Progress: %d%% (%u / %d bytes)\n", s_state.progress_percent, (unsigned)written, contentLength);
        }
      }
    } else {
      delay(10);
    }
  }

  http.end();

  if (written != (size_t)contentLength) {
    s_state.status = OTA_STATUS_ERROR;
    snprintf(s_state.error_msg, sizeof(s_state.error_msg), "Incomplete transfer (%u / %d bytes)", (unsigned)written, contentLength);
    diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
    Update.abort();
    if (check_sha) mbedtls_sha256_free(&sha_ctx);
    return false;
  }

  if (check_sha) {
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
      snprintf(s_state.error_msg, sizeof(s_state.error_msg), "SHA-256 mismatch (got %s)", hexOutput);
      diag_log_printf(DIAG_LOG_ERROR, "OTA", "%s", s_state.error_msg);
      Update.abort();
      return false;
    }
    diag_log_printf(DIAG_LOG_INFO, "OTA", "SHA-256 verified successfully: %s", hexOutput);
  }

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
  diag_log_printf(DIAG_LOG_WARN, "OTA", "OTA successfully written! Rebooting into new firmware...");
  Serial.println("[OTA] Firmware update complete! Restarting in 1s...");
  delay(1000);
  ESP.restart();
  return true;

#else
  // Simulator / native host mock
  s_state.status = OTA_STATUS_SUCCESS;
  s_state.progress_percent = 100;
  diag_log_printf(DIAG_LOG_INFO, "OTA", "Simulated OTA download and flash complete: %s", url);
  return true;
#endif
}

void ota_get_state(OtaState* out_state) {
  if (out_state) {
    *out_state = s_state;
  }
}
