#pragma once

namespace Watchy_SyncTime {

// ntpServer defaults to NTP_SERVER from config.h
// but can be set to a more specific ntp server if known
extern const char* ntpServer;

constexpr const char *DEFAULT_TIMEZONE = "UTC0"; // posix format

typedef enum {
  ready,
  waiting,
  wifiFailed,
  timeout,
  success,
  numSyncStates
} SyncResult;

// sets RTC, now() and time() to UTC
// sets current timezone so that localtime works
extern SyncResult syncTime(const char* timezone = DEFAULT_TIMEZONE);
};  // namespace Watchy_SyncTime
