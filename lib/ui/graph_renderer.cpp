#include "graph_renderer.h"

#include <array>
#include <math.h>
#include <stdio.h>

namespace airmon {

namespace {

constexpr uint16_t kBlack = 0;
constexpr uint16_t kWhite = 1;
constexpr int16_t kGraphX = 56;
constexpr int16_t kGraphY = 34;
constexpr int16_t kGraphWidth = 324;
constexpr int16_t kGraphHeight = 158;
constexpr int16_t kCurrentValueRight = 380;
constexpr int16_t kCurrentValueRegionX = 210;
constexpr int16_t kCurrentValueRegionY = 4;
constexpr int16_t kCurrentValueRegionWidth = 180;
constexpr int16_t kCurrentValueRegionHeight = 28;
constexpr int16_t kXAxisLabelY = 200;
constexpr uint8_t kXAxisTickCount = 7;
constexpr uint8_t kYAxisTickCount = 5;
constexpr size_t kMaxCurveControlPoints = 96;
constexpr size_t k24HRangeSlots = 1440U;
constexpr size_t k7DRangeSlots = 672U;
constexpr float kSlopeEpsilon = 0.0001f;

float absoluteValue(const float value) { return value < 0.0f ? -value : value; }

float sign(const float value) {
  if (value > 0.0f) {
    return 1.0f;
  }
  if (value < 0.0f) {
    return -1.0f;
  }
  return 0.0f;
}

size_t rangeSlotCount(const HistoryRange range) {
  return range == HistoryRange::Range24H ? k24HRangeSlots : k7DRangeSlots;
}

}  // namespace

GraphRenderer::GraphRenderer(Adafruit_SharpMem& display) : display_(display) {}

void GraphRenderer::render(const HistoryManager& history,
                           const MetricId metric,
                           const HistoryRange range,
                           const float currentValue,
                           const bool currentValueValid,
                           const bool forceFullRedraw) {
  const size_t pointCount = history.pointCount(metric, range);
  const uint32_t historyRevision = history.revision(metric, range);
  const bool redrawBase = forceFullRedraw || !layoutInitialized_ || lastMetric_ != metric || lastRange_ != range;
  const bool redrawPlot = redrawBase || lastHistoryRevision_ != historyRevision || lastPointCount_ != pointCount;

  display_.setTextColor(kBlack);
  if (redrawBase) {
    display_.clearDisplay();
    drawStaticHeader(metric, range);
  } else {
    clearCurrentValueRegion();
    if (redrawPlot) {
      clearPlotRegion();
    }
  }

  drawCurrentValue(history, metric, range, currentValue, currentValueValid);

  if (redrawPlot) {
    display_.drawRect(kGraphX, kGraphY, kGraphWidth, kGraphHeight, kBlack);
    drawXAxisLabels(range, kGraphX, kGraphY, kGraphWidth, kGraphHeight);
  }

  if (pointCount < 2) {
    if (redrawPlot) {
      display_.setTextSize(2);
      drawRightAlignedText("Nedostatek dat", 320, 98, 2);
    }
    layoutInitialized_ = true;
    lastMetric_ = metric;
    lastRange_ = range;
    lastHistoryRevision_ = historyRevision;
    lastPointCount_ = pointCount;
    display_.refresh();
    return;
  }

  if (redrawPlot) {
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float value = 0.0f;
    history.pointAt(metric, range, 0, value);
    minValue = value;
    maxValue = value;

    for (size_t index = 1; index < pointCount; ++index) {
      if (!history.pointAt(metric, range, index, value)) {
        continue;
      }
      if (value < minValue) {
        minValue = value;
      }
      if (value > maxValue) {
        maxValue = value;
      }
    }

    float minScaled = minValue;
    float maxScaled = maxValue;
    float span = maxScaled - minScaled;
    if (span < 0.001f) {
      const float padding = fmaxf(fabsf(minValue) * 0.1f, 1.0f);
      minScaled -= padding;
      maxScaled += padding;
    } else {
      const float padding = span * 0.1f;
      minScaled -= padding;
      maxScaled += padding;
    }

    drawYAxisLabels(metric, minScaled, maxScaled, kGraphX, kGraphY, kGraphHeight);

    const int16_t plotX = kGraphX + 1;
    const int16_t plotY = kGraphY + 1;
    const int16_t plotWidth = kGraphWidth - 2;
    const int16_t plotHeight = kGraphHeight - 2;

    const size_t controlCount = pointCount < kMaxCurveControlPoints ? pointCount : kMaxCurveControlPoints;
    std::array<float, kMaxCurveControlPoints> controlValues{};
    std::array<float, kMaxCurveControlPoints> tangents{};
    std::array<float, kMaxCurveControlPoints - 1U> deltas{};

    for (size_t controlIndex = 0; controlIndex < controlCount; ++controlIndex) {
      const size_t bucketStart = (controlIndex * pointCount) / controlCount;
      size_t bucketEnd = ((controlIndex + 1U) * pointCount) / controlCount;
      if (bucketEnd <= bucketStart) {
        bucketEnd = bucketStart + 1U;
      }

      float bucketSum = 0.0f;
      size_t bucketSamples = 0;
      for (size_t sampleIndex = bucketStart; sampleIndex < bucketEnd; ++sampleIndex) {
        if (!history.pointAt(metric, range, sampleIndex, value)) {
          continue;
        }
        bucketSum += value;
        ++bucketSamples;
      }

      if (bucketSamples == 0U) {
        controlValues[controlIndex] = (controlIndex == 0U) ? minValue : controlValues[controlIndex - 1U];
        continue;
      }

      controlValues[controlIndex] = bucketSum / static_cast<float>(bucketSamples);
    }

    if (controlCount == 2U) {
      const float delta = controlValues[1] - controlValues[0];
      tangents[0] = delta;
      tangents[1] = delta;
    } else {
      for (size_t index = 0; index < controlCount - 1U; ++index) {
        deltas[index] = controlValues[index + 1U] - controlValues[index];
      }

      float endpointSlope = ((3.0f * deltas[0]) - deltas[1]) * 0.5f;
      if (sign(endpointSlope) != sign(deltas[0])) {
        endpointSlope = 0.0f;
      } else if ((sign(deltas[0]) != sign(deltas[1])) &&
                 (absoluteValue(endpointSlope) > absoluteValue(3.0f * deltas[0]))) {
        endpointSlope = 3.0f * deltas[0];
      }
      tangents[0] = endpointSlope;

      for (size_t index = 1; index < controlCount - 1U; ++index) {
        const float previousDelta = deltas[index - 1U];
        const float nextDelta = deltas[index];
        if ((absoluteValue(previousDelta) < kSlopeEpsilon) || (absoluteValue(nextDelta) < kSlopeEpsilon) ||
            (sign(previousDelta) != sign(nextDelta))) {
          tangents[index] = 0.0f;
          continue;
        }
        tangents[index] = (2.0f * previousDelta * nextDelta) / (previousDelta + nextDelta);
      }

      endpointSlope = ((3.0f * deltas[controlCount - 2U]) - deltas[controlCount - 3U]) * 0.5f;
      if (sign(endpointSlope) != sign(deltas[controlCount - 2U])) {
        endpointSlope = 0.0f;
      } else if ((sign(deltas[controlCount - 2U]) != sign(deltas[controlCount - 3U])) &&
                 (absoluteValue(endpointSlope) > absoluteValue(3.0f * deltas[controlCount - 2U]))) {
        endpointSlope = 3.0f * deltas[controlCount - 2U];
      }
      tangents[controlCount - 1U] = endpointSlope;
    }

    int16_t previousX = plotX;
    int16_t previousY = plotY + plotHeight / 2;
    bool hasPrevious = false;
    uint16_t plottedPoints = 0U;
    const size_t totalSlots = rangeSlotCount(range);
    const float dataStartSlot = static_cast<float>(totalSlots - pointCount);

    for (int16_t pixelIndex = 0; pixelIndex < plotWidth; ++pixelIndex) {
      const float slotPosition =
          (plotWidth <= 1 || totalSlots <= 1U)
              ? static_cast<float>(totalSlots - 1U)
              : (static_cast<float>(pixelIndex) * static_cast<float>(totalSlots - 1U)) /
                    static_cast<float>(plotWidth - 1);

      if (slotPosition < dataStartSlot) {
        continue;
      }

      const float dataPosition = slotPosition - dataStartSlot;
      const float position = (pointCount <= 1U)
                                 ? 0.0f
                                 : (dataPosition * static_cast<float>(controlCount - 1U)) /
                                       static_cast<float>(pointCount - 1U);
      size_t segmentIndex = static_cast<size_t>(position);
      if (segmentIndex >= controlCount - 1U) {
        segmentIndex = controlCount - 2U;
      }
      const float t = position - static_cast<float>(segmentIndex);
      const float t2 = t * t;
      const float t3 = t2 * t;
      const float h00 = (2.0f * t3) - (3.0f * t2) + 1.0f;
      const float h10 = t3 - (2.0f * t2) + t;
      const float h01 = (-2.0f * t3) + (3.0f * t2);
      const float h11 = t3 - t2;

      value = (h00 * controlValues[segmentIndex]) + (h10 * tangents[segmentIndex]) +
              (h01 * controlValues[segmentIndex + 1U]) + (h11 * tangents[segmentIndex + 1U]);

      const float normalized = (value - minScaled) / (maxScaled - minScaled);
      int16_t y = plotY + plotHeight - 1 - static_cast<int16_t>(lroundf(normalized * static_cast<float>(plotHeight - 1)));
      if (y < plotY) {
        y = plotY;
      } else if (y >= plotY + plotHeight) {
        y = plotY + plotHeight - 1;
      }

      const int16_t x = plotX + pixelIndex;
      if (hasPrevious) {
        display_.drawLine(previousX, previousY, x, y, kBlack);
      }
      previousX = x;
      previousY = y;
      hasPrevious = true;
      ++plottedPoints;
    }

    if (plottedPoints == 1U) {
      display_.drawPixel(previousX, previousY, kBlack);
    }
  }

  display_.refresh();
  layoutInitialized_ = true;
  lastMetric_ = metric;
  lastRange_ = range;
  lastHistoryRevision_ = historyRevision;
  lastPointCount_ = pointCount;
}

void GraphRenderer::drawStaticHeader(const MetricId metric, const HistoryRange range) const {
  char buffer[32];
  display_.setTextSize(2);
  display_.setCursor(20, 6);
  display_.print(metricLabel(metric));

  display_.setTextSize(1);
  snprintf(buffer, sizeof(buffer), "Rozsah: %s", rangeLabel(range));
  display_.setCursor(20, 22);
  display_.print(buffer);
}

void GraphRenderer::drawCurrentValue(const HistoryManager& history,
                                     const MetricId metric,
                                     const HistoryRange range,
                                     const float currentValue,
                                     const bool currentValueValid) const {
  char buffer[96];
  if (currentValueValid) {
    formatMetricValue(metric, currentValue, buffer, sizeof(buffer));
  } else {
    snprintf(buffer, sizeof(buffer), "-- %s", metricUnit(metric));
  }

  display_.setTextSize(2);
  int16_t currentX1 = 0;
  int16_t currentY1 = 0;
  uint16_t currentWidth = 0;
  uint16_t currentHeight = 0;
  display_.getTextBounds(buffer, 0, 0, &currentX1, &currentY1, &currentWidth, &currentHeight);
  const int16_t currentValueLeft = kCurrentValueRight - static_cast<int16_t>(currentWidth);
  drawRightAlignedText(buffer, kCurrentValueRight, 10, 2);
  drawTrendIndicator(currentValueLeft - 12, 18, calculateTrend(history, metric, range, currentValue, currentValueValid));
}

void GraphRenderer::clearCurrentValueRegion() const {
  display_.fillRect(kCurrentValueRegionX, kCurrentValueRegionY, kCurrentValueRegionWidth, kCurrentValueRegionHeight,
                    kWhite);
}

void GraphRenderer::clearPlotRegion() const {
  display_.fillRect(0, kGraphY - 1, kGraphX + kGraphWidth + 10, kXAxisLabelY - kGraphY + 20, kWhite);
}

void GraphRenderer::drawCenteredText(const char* text,
                                     const int16_t x,
                                     const int16_t y,
                                     const uint8_t textSize) const {
  display_.setTextSize(textSize);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  display_.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  display_.setCursor(x - static_cast<int16_t>(width / 2U), y);
  display_.print(text);
}

void GraphRenderer::drawRightAlignedText(const char* text,
                                         const int16_t x,
                                         const int16_t y,
                                         const uint8_t textSize) const {
  display_.setTextSize(textSize);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  display_.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  display_.setCursor(x - static_cast<int16_t>(width), y);
  display_.print(text);
}

void GraphRenderer::drawTrendIndicator(const int16_t centerX,
                                       const int16_t centerY,
                                       const TrendDirection direction) const {
  switch (direction) {
    case TrendDirection::Up:
      display_.drawLine(centerX, centerY + 5, centerX, centerY - 5, kBlack);
      display_.drawLine(centerX, centerY - 5, centerX - 3, centerY - 1, kBlack);
      display_.drawLine(centerX, centerY - 5, centerX + 3, centerY - 1, kBlack);
      break;
    case TrendDirection::Down:
      display_.drawLine(centerX, centerY - 5, centerX, centerY + 5, kBlack);
      display_.drawLine(centerX, centerY + 5, centerX - 3, centerY + 1, kBlack);
      display_.drawLine(centerX, centerY + 5, centerX + 3, centerY + 1, kBlack);
      break;
    case TrendDirection::Flat:
      display_.drawLine(centerX - 5, centerY, centerX + 4, centerY, kBlack);
      display_.drawLine(centerX + 4, centerY, centerX + 1, centerY - 3, kBlack);
      display_.drawLine(centerX + 4, centerY, centerX + 1, centerY + 3, kBlack);
      break;
    case TrendDirection::Unknown:
      break;
  }
}

void GraphRenderer::drawXAxisLabels(const HistoryRange range,
                                    const int16_t graphX,
                                    const int16_t graphY,
                                    const int16_t graphWidth,
                                    const int16_t graphHeight) const {
  const int16_t tickY = graphY + graphHeight - 1;
  const int16_t rightX = graphX + graphWidth - 1;

  for (uint8_t tickIndex = 0; tickIndex < kXAxisTickCount; ++tickIndex) {
    const float ratio =
        (kXAxisTickCount <= 1) ? 0.0f : static_cast<float>(tickIndex) / static_cast<float>(kXAxisTickCount - 1U);
    const int16_t tickX =
        graphX + static_cast<int16_t>(lroundf(ratio * static_cast<float>(graphWidth - 1)));
    const int16_t tickLength = (tickIndex % 3U == 0U || tickIndex == (kXAxisTickCount - 1U)) ? 5 : 3;
    display_.drawLine(tickX, tickY, tickX, tickY + tickLength, kBlack);
  }

  const char* leftLabel = range == HistoryRange::Range24H ? "-24h" : "-7d";
  const char* centerLabel = range == HistoryRange::Range24H ? "-12h" : "-3.5d";
  const int16_t centerX = graphX + graphWidth / 2;

  display_.setTextSize(1);
  display_.setCursor(graphX - 2, kXAxisLabelY);
  display_.print(leftLabel);
  drawCenteredText(centerLabel, centerX, kXAxisLabelY, 1);
  drawRightAlignedText("ted", graphX + graphWidth, kXAxisLabelY, 1);
}

void GraphRenderer::drawYAxisLabels(const MetricId metric,
                                    const float minScaled,
                                    const float maxScaled,
                                    const int16_t graphX,
                                    const int16_t graphY,
                                    const int16_t graphHeight) const {
  char buffer[24];
  for (uint8_t tickIndex = 0; tickIndex < kYAxisTickCount; ++tickIndex) {
    const float ratio =
        (kYAxisTickCount <= 1) ? 0.0f : static_cast<float>(tickIndex) / static_cast<float>(kYAxisTickCount - 1U);
    const float value = maxScaled - (ratio * (maxScaled - minScaled));
    const int16_t tickY =
        graphY + static_cast<int16_t>(lroundf(ratio * static_cast<float>(graphHeight - 1)));
    const int16_t textY = tickIndex == 0 ? graphY : static_cast<int16_t>(tickY - 4);

    formatAxisValue(metric, value, buffer, sizeof(buffer));
    drawRightAlignedText(buffer, graphX - 6, textY, 1);
    display_.drawLine(graphX - 4, tickY, graphX, tickY, kBlack);
  }
}

GraphRenderer::TrendDirection GraphRenderer::calculateTrend(const HistoryManager& history,
                                                            const MetricId metric,
                                                            const HistoryRange range,
                                                            const float currentValue,
                                                            const bool currentValueValid) const {
  const size_t pointCount = history.pointCount(metric, range);
  if (currentValueValid) {
    float latestValue = 0.0f;
    if (!history.latest(metric, range, latestValue)) {
      return TrendDirection::Unknown;
    }

    const float delta = currentValue - latestValue;
    const float threshold = trendThreshold(metric);
    if (delta > threshold) {
      return TrendDirection::Up;
    }
    if (delta < -threshold) {
      return TrendDirection::Down;
    }
    return TrendDirection::Flat;
  }

  if (pointCount < 2) {
    return TrendDirection::Unknown;
  }

  float previousValue = 0.0f;
  float latestValue = 0.0f;
  if (!history.pointAt(metric, range, pointCount - 2U, previousValue) ||
      !history.pointAt(metric, range, pointCount - 1U, latestValue)) {
    return TrendDirection::Unknown;
  }

  const float delta = latestValue - previousValue;
  const float threshold = trendThreshold(metric);
  if (delta > threshold) {
    return TrendDirection::Up;
  }
  if (delta < -threshold) {
    return TrendDirection::Down;
  }
  return TrendDirection::Flat;
}

float GraphRenderer::trendThreshold(const MetricId metric) const {
  return metricTrendThreshold(metric);
}

void GraphRenderer::formatAxisValue(const MetricId metric,
                                    const float value,
                                    char* buffer,
                                    const size_t bufferSize) const {
  if (metric == MetricId::COUNT) {
    snprintf(buffer, bufferSize, "--");
    return;
  }

  snprintf(buffer, bufferSize, metricUsesSingleDecimal(metric) ? "%.1f" : "%.0f", value);
}

void GraphRenderer::formatMetricValue(const MetricId metric,
                                      const float value,
                                      char* buffer,
                                      const size_t bufferSize) const {
  if (metric == MetricId::COUNT) {
    snprintf(buffer, bufferSize, "--");
    return;
  }

  snprintf(buffer, bufferSize, metricUsesSingleDecimal(metric) ? "%.1f %s" : "%.0f %s", value, metricUnit(metric));
}

}  // namespace airmon
