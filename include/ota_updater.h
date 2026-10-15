#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  OTA_STATUS_IDLE = 0,
  OTA_STATUS_DOWNLOADING,
  OTA_STATUS_VERIFYING,
  OTA_STATUS_SUCCESS,
  OTA_STATUS_ERROR
} OtaStatus;

typedef struct {
  const char* running_partition;
  const char* next_partition;
  bool is_rollback_possible;
  bool is_pending_verification;
  OtaStatus status;
  int progress_percent;
  char error_msg[128];
} OtaState;

// Initialize OTA subsystem and check running partition & rollback status on boot
void ota_init(void);

// Validate and confirm running partition (cancels rollback)
// Called after successful server sync
bool ota_confirm_running_partition(void);

// Mark running partition invalid and immediately trigger rollback & restart
void ota_rollback_and_reboot(void);

// Perform full OTA update from HTTP/HTTPS URL with optional SHA-256 verification
bool ota_perform_update(const char* url, const char* expected_sha256);

// Query current OTA subsystem state
void ota_get_state(OtaState* out_state);

// Test double hook for verifying confirm semantics in simulator / native host
void ota_set_pending_verification_for_test(bool pending);

#ifdef __cplusplus
}
#endif
