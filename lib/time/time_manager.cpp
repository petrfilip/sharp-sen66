#include "time_manager.h"

#include <Arduino.h>

namespace airmon {

namespace {

constexpr const char* kTimeZone = "CET-1CEST,M3.5.0/2,M10.5.0/3";
constexpr const char* kNtpServer1 = "pool.ntp.org";
constexpr const char* kNtpServer2 = "time.google.com";
constexpr const char* kNtpServer3 = "time.cloudflare.com";

}  // namespace

void TimeManager::initTime() {
  configTzTime(kTimeZone, kNtpServer1, kNtpServer2, kNtpServer3);
  initialized_ = true;
}

bool TimeManager::timeReady() const {
  struct tm timeInfo;
  return getLocalTimeSafe(timeInfo);
}

bool TimeManager::getLocalTimeSafe(struct tm& timeInfo) const {
  if (!initialized_) {
    return false;
  }

  if (!getLocalTime(&timeInfo, 100)) {
    return false;
  }

  return timeInfo.tm_year >= 120;
}

bool TimeManager::formatDateTime(char* buffer, const size_t bufferSize) const {
  if (bufferSize == 0) {
    return false;
  }

  struct tm timeInfo;
  if (!getLocalTimeSafe(timeInfo)) {
    snprintf(buffer, bufferSize, "NO TIME");
    return false;
  }

  strftime(buffer, bufferSize, "%d.%m.%Y %H:%M", &timeInfo);
  return true;
}

}  // namespace airmon
