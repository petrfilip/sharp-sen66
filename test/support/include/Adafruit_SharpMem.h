#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Arduino.h"

class Adafruit_SharpMem {
 public:
  struct TextCall {
    int16_t x = 0;
    int16_t y = 0;
    uint8_t textSize = 1;
    std::string text;
  };

  struct LineCall {
    int16_t x1 = 0;
    int16_t y1 = 0;
    int16_t x2 = 0;
    int16_t y2 = 0;
    uint16_t color = 0;
  };

  struct RectCall {
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;
    uint16_t color = 0;
  };

  struct CircleCall {
    int16_t x = 0;
    int16_t y = 0;
    int16_t r = 0;
    uint16_t color = 0;
  };

  struct PixelCall {
    int16_t x = 0;
    int16_t y = 0;
    uint16_t color = 0;
  };

  void clearDisplay() { ++clearDisplayCalls; }

  void setTextColor(const uint16_t color) { textColor = color; }

  void setTextSize(const uint8_t size) { textSize_ = size; }

  void setCursor(const int16_t x, const int16_t y) {
    cursorX_ = x;
    cursorY_ = y;
  }

  size_t print(const char* text) {
    textCalls.push_back({cursorX_, cursorY_, textSize_, text != nullptr ? std::string(text) : std::string()});
    return text != nullptr ? std::strlen(text) : 0U;
  }

  size_t print(const String& text) { return print(text.c_str()); }

  void drawRect(const int16_t x, const int16_t y, const int16_t w, const int16_t h, const uint16_t color) {
    rectCalls.push_back({x, y, w, h, color});
  }

  void fillRect(const int16_t x, const int16_t y, const int16_t w, const int16_t h, const uint16_t color) {
    fillRectCalls.push_back({x, y, w, h, color});
  }

  void drawLine(const int16_t x1,
                const int16_t y1,
                const int16_t x2,
                const int16_t y2,
                const uint16_t color) {
    lineCalls.push_back({x1, y1, x2, y2, color});
  }

  void drawCircle(const int16_t x, const int16_t y, const int16_t r, const uint16_t color) {
    circleCalls.push_back({x, y, r, color});
  }

  void fillCircle(const int16_t x, const int16_t y, const int16_t r, const uint16_t color) {
    fillCircleCalls.push_back({x, y, r, color});
  }

  void drawPixel(const int16_t x, const int16_t y, const uint16_t color) {
    pixelCalls.push_back({x, y, color});
  }

  void refresh() { ++refreshCalls; }

  void getTextBounds(const char* text,
                     const int16_t x,
                     const int16_t y,
                     int16_t* x1,
                     int16_t* y1,
                     uint16_t* width,
                     uint16_t* height) const {
    const size_t length = text != nullptr ? std::strlen(text) : 0U;
    if (x1 != nullptr) {
      *x1 = x;
    }
    if (y1 != nullptr) {
      *y1 = y;
    }
    if (width != nullptr) {
      *width = static_cast<uint16_t>(length * 6U * static_cast<size_t>(textSize_));
    }
    if (height != nullptr) {
      *height = static_cast<uint16_t>(8U * static_cast<size_t>(textSize_));
    }
  }

  void getTextBounds(const String& text,
                     const int16_t x,
                     const int16_t y,
                     int16_t* x1,
                     int16_t* y1,
                     uint16_t* width,
                     uint16_t* height) const {
    getTextBounds(text.c_str(), x, y, x1, y1, width, height);
  }

  int clearDisplayCalls = 0;
  int refreshCalls = 0;
  uint16_t textColor = 0;
  std::vector<TextCall> textCalls;
  std::vector<LineCall> lineCalls;
  std::vector<RectCall> rectCalls;
  std::vector<RectCall> fillRectCalls;
  std::vector<CircleCall> circleCalls;
  std::vector<CircleCall> fillCircleCalls;
  std::vector<PixelCall> pixelCalls;

 private:
  uint8_t textSize_ = 1;
  int16_t cursorX_ = 0;
  int16_t cursorY_ = 0;
};
