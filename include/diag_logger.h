#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DIAG_LOG_MAX_ENTRIES 100
#define DIAG_LOG_MAX_TAG_LEN 16
#define DIAG_LOG_MAX_MSG_LEN 128

enum DiagLogLevel {
  DIAG_LOG_DEBUG = 0,
  DIAG_LOG_INFO  = 1,
  DIAG_LOG_WARN  = 2,
  DIAG_LOG_ERROR = 3
};

struct DiagLogEntry {
  uint32_t uptime_ms;
  uint8_t  level;
  char     tag[DIAG_LOG_MAX_TAG_LEN];
  char     msg[DIAG_LOG_MAX_MSG_LEN];
};

#ifdef __cplusplus
extern "C" {
#endif

void diag_log_init(void);
void diag_log_write(DiagLogLevel level, const char* tag, const char* msg);
void diag_log_printf(DiagLogLevel level, const char* tag, const char* fmt, ...);

size_t diag_log_count(void);
void   diag_log_clear(void);
bool   diag_log_get(size_t index, DiagLogEntry* out);

void   diag_log_dump_stdout(void);
size_t diag_log_to_json(char* dst, size_t max_len);

#ifdef __cplusplus
}
#include <string>
std::string diag_log_to_json_string(const char* device_id, const char* version, int battery_pct, int rssi_dbm);
#endif
