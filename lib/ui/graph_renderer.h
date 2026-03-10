#pragma once

#include <Adafruit_SharpMem.h>

#include "history_manager.h"
#include "metrics.h"

namespace airmon {

class GraphRenderer {
 public:
  explicit GraphRenderer(Adafruit_SharpMem& display);

  void render(const HistoryManager& history,
              MetricId metric,
              HistoryRange range,
              float currentValue,
              bool currentValueValid,
              bool forceFullRedraw = false);

 private:
  enum class TrendDirection : uint8_t {
    Unknown = 0,
    Flat = 1,
    Up = 2,
    Down = 3,
  };

  void drawCenteredText(const char* text, int16_t x, int16_t y, uint8_t textSize) const;
  void drawRightAlignedText(const char* text, int16_t x, int16_t y, uint8_t textSize) const;
  void drawStaticHeader(MetricId metric, HistoryRange range) const;
  void drawCurrentValue(const HistoryManager& history,
                        MetricId metric,
                        HistoryRange range,
                        float currentValue,
                        bool currentValueValid) const;
  void drawTrendIndicator(int16_t centerX, int16_t centerY, TrendDirection direction) const;
  void drawXAxisLabels(HistoryRange range, int16_t graphX, int16_t graphY, int16_t graphWidth, int16_t graphHeight) const;
  void drawYAxisLabels(MetricId metric,
                       float minScaled,
                       float maxScaled,
                       int16_t graphX,
                       int16_t graphY,
                       int16_t graphHeight) const;
  void clearCurrentValueRegion() const;
  void clearPlotRegion() const;
  TrendDirection calculateTrend(const HistoryManager& history,
                                MetricId metric,
                                HistoryRange range,
                                float currentValue,
                                bool currentValueValid) const;
  float trendThreshold(MetricId metric) const;
  void formatAxisValue(MetricId metric, float value, char* buffer, size_t bufferSize) const;
  void formatMetricValue(MetricId metric, float value, char* buffer, size_t bufferSize) const;

  Adafruit_SharpMem& display_;
  bool layoutInitialized_ = false;
  MetricId lastMetric_ = MetricId::COUNT;
  HistoryRange lastRange_ = HistoryRange::Range24H;
  uint32_t lastHistoryRevision_ = 0;
  size_t lastPointCount_ = 0;
};

}  // namespace airmon
