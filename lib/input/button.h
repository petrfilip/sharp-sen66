#pragma once

#include <stdint.h>

namespace airmon {

class Button {
 public:
  Button(uint32_t debounceMs = 40, uint32_t longPressMs = 700);

  void begin(uint8_t pin);
  void update(uint32_t nowMs);
  void updateRaw(bool pressed, uint32_t nowMs);

  bool wasShortPressed();
  bool wasLongPressed();

 private:
  uint8_t pin_ = 0xFF;
  uint32_t debounceMs_ = 40;
  uint32_t longPressMs_ = 700;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  uint32_t lastRawChangeAt_ = 0;
  uint32_t pressedAt_ = 0;
  bool longReported_ = false;
  bool shortEventPending_ = false;
  bool longEventPending_ = false;
};

}  // namespace airmon
