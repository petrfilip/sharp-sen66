#include "button.h"

#include <Arduino.h>

namespace airmon {

Button::Button(const uint32_t debounceMs, const uint32_t longPressMs)
    : debounceMs_(debounceMs), longPressMs_(longPressMs) {}

void Button::begin(const uint8_t pin) {
  pin_ = pin;
  pinMode(pin_, INPUT_PULLUP);
  rawPressed_ = false;
  stablePressed_ = false;
  lastRawChangeAt_ = 0;
  pressedAt_ = 0;
  longReported_ = false;
  shortEventPending_ = false;
  longEventPending_ = false;
}

void Button::update(const uint32_t nowMs) {
  if (pin_ == 0xFF) {
    return;
  }

  const bool pressed = digitalRead(pin_) == LOW;
  updateRaw(pressed, nowMs);
}

void Button::updateRaw(const bool pressed, const uint32_t nowMs) {
  if (pressed != rawPressed_) {
    rawPressed_ = pressed;
    lastRawChangeAt_ = nowMs;
  }

  if ((nowMs - lastRawChangeAt_) >= debounceMs_ && stablePressed_ != rawPressed_) {
    stablePressed_ = rawPressed_;
    if (stablePressed_) {
      pressedAt_ = nowMs;
      longReported_ = false;
    } else if (!longReported_) {
      shortEventPending_ = true;
    }
  }

  if (stablePressed_ && !longReported_ && (nowMs - pressedAt_) >= longPressMs_) {
    longReported_ = true;
    longEventPending_ = true;
  }
}

bool Button::wasShortPressed() {
  const bool value = shortEventPending_;
  shortEventPending_ = false;
  return value;
}

bool Button::wasLongPressed() {
  const bool value = longEventPending_;
  longEventPending_ = false;
  return value;
}

}  // namespace airmon
