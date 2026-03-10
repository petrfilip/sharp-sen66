#pragma once

#include <stddef.h>
#include <time.h>

namespace airmon {

class TimeManager {
 public:
  void initTime();
  bool timeReady() const;
  bool getLocalTimeSafe(struct tm& timeInfo) const;
  bool formatDateTime(char* buffer, size_t bufferSize) const;

 private:
  bool initialized_ = false;
};

}  // namespace airmon
