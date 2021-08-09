#include "SyncTime.h"

#include "Watchy.h"
#include "time.h"

namespace Watchy_SyncTime {

RTC_DATA_ATTR const char *ntpServer;

// RTC does not know about TZ
// so DST has to be in app code
// so RTC must store UTC
// RTC computes leap year so you want year to be correct
// according to how the RTC represents it

void timeSyncCallback(struct timeval *tv) {
  // consider using tv.tv_usec as well
  Watchy::RTC.set(tv->tv_sec);  // set RTC
  setTime(tv->tv_sec);          // set system time
  settimeofday(tv, nullptr);    // set posix
  sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}

SyncResult syncTime(const char* timezone) {
  if (!Watchy::connectWiFi()) {
    return Watchy_SyncTime::wifiFailed;  // failed
  }
  if (sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET) {
    // SNTP busy
    LOGI("%d", sntp_get_sync_status());
    return Watchy_SyncTime::waiting;
  }
  sntp_set_time_sync_notification_cb(timeSyncCallback);
  configTzTime(timezone, ntpServer);
  uint32_t timeout = millis() + 5000;  // 5 sec timeout
  SyncResult retVal = Watchy_SyncTime::timeout;
  while (millis() < timeout) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      retVal = Watchy_SyncTime::success;
      break;
    }
    delay(10);  // or yield or get event
  }
  WiFi.mode(WIFI_OFF);
  btStop();
  return retVal;
}
}  // namespace Watchy_SyncTime
