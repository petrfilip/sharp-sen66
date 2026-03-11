#pragma once

#include <Arduino.h>

#include <math.h>
#include <stdio.h>

#include "AppTypes.h"
#include "history_manager.h"

namespace sharp {
namespace displayframes {

namespace detail {

inline void appendSeparator(String& signature) { signature += "|"; }

inline void appendText(String& signature, const String& value) {
  signature += value;
  appendSeparator(signature);
}

inline void appendText(String& signature, const char* value) {
  signature += value != nullptr ? value : "";
  appendSeparator(signature);
}

inline void appendBool(String& signature, const bool value) {
  signature += value ? "1" : "0";
  appendSeparator(signature);
}

inline void appendUInt(String& signature, const uint32_t value) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%lu", static_cast<unsigned long>(value));
  signature += buffer;
  appendSeparator(signature);
}

inline void appendInt(String& signature, const int value) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%d", value);
  signature += buffer;
  appendSeparator(signature);
}

inline void appendFloat1(String& signature, const float value) {
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%.1f", value);
  signature += buffer;
  appendSeparator(signature);
}

inline void appendFloat0(String& signature, const float value) {
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%.0f", value);
  signature += buffer;
  appendSeparator(signature);
}

inline int dashboardBarWidth(const float pm25) {
  float ratio = pm25 / 150.0f;
  if (ratio < 0.0f) {
    ratio = 0.0f;
  } else if (ratio > 1.0f) {
    ratio = 1.0f;
  }
  return static_cast<int>(ratio * 120.0f);
}

inline void formatGraphMetricValue(const airmon::MetricId metric,
                                   const float value,
                                   const bool currentValueValid,
                                   char* buffer,
                                   const size_t bufferSize) {
  if (!currentValueValid) {
    snprintf(buffer, bufferSize, "-- %s", airmon::metricUnit(metric));
    return;
  }

  switch (metric) {
    case airmon::MetricId::CO2:
    case airmon::MetricId::VOC:
    case airmon::MetricId::NOX:
      snprintf(buffer, bufferSize, "%.0f %s", value, airmon::metricUnit(metric));
      break;
    case airmon::MetricId::PM25:
    case airmon::MetricId::TEMP:
    case airmon::MetricId::HUM:
      snprintf(buffer, bufferSize, "%.1f %s", value, airmon::metricUnit(metric));
      break;
    case airmon::MetricId::COUNT:
      snprintf(buffer, bufferSize, "--");
      break;
  }
}

inline float graphTrendThreshold(const airmon::MetricId metric) {
  switch (metric) {
    case airmon::MetricId::CO2:
      return 10.0f;
    case airmon::MetricId::PM25:
      return 1.0f;
    case airmon::MetricId::TEMP:
      return 0.3f;
    case airmon::MetricId::HUM:
      return 1.0f;
    case airmon::MetricId::VOC:
    case airmon::MetricId::NOX:
      return 3.0f;
    case airmon::MetricId::COUNT:
      return 0.0f;
  }
  return 0.0f;
}

inline uint8_t graphTrendDirection(const airmon::HistoryManager& history,
                                   const airmon::MetricId metric,
                                   const airmon::HistoryRange range,
                                   const float currentValue,
                                   const bool currentValueValid) {
  const size_t pointCount = history.pointCount(metric, range);
  if (currentValueValid) {
    float latestValue = 0.0f;
    if (!history.latest(metric, range, latestValue)) {
      return 0U;
    }

    const float delta = currentValue - latestValue;
    const float threshold = graphTrendThreshold(metric);
    if (delta > threshold) {
      return 1U;
    }
    if (delta < -threshold) {
      return 2U;
    }
    return 3U;
  }

  if (pointCount < 2U) {
    return 0U;
  }

  float previousValue = 0.0f;
  float latestValue = 0.0f;
  if (!history.pointAt(metric, range, pointCount - 2U, previousValue) ||
      !history.pointAt(metric, range, pointCount - 1U, latestValue)) {
    return 0U;
  }

  const float delta = latestValue - previousValue;
  const float threshold = graphTrendThreshold(metric);
  if (delta > threshold) {
    return 1U;
  }
  if (delta < -threshold) {
    return 2U;
  }
  return 3U;
}

}  // namespace detail

// Keep this aligned with renderer formatting so a redraw happens only when the visible frame changes.
inline String buildDashboardFrameSignature(const SensorSnapshot& sensorData,
                                           const String& wifiText,
                                           const String& tmepText,
                                           const bool mqttConnected,
                                           const bool sen66Ready,
                                           const String& uptimeText,
                                           const String& dateTimeText) {
  String signature = "dashboard|";
  detail::appendText(signature, wifiText);
  detail::appendText(signature, tmepText);
  detail::appendBool(signature, mqttConnected);
  detail::appendBool(signature, sen66Ready);
  detail::appendText(signature, uptimeText);
  detail::appendText(signature, dateTimeText);
  detail::appendBool(signature, sensorData.valid);

  if (!sensorData.valid) {
    return signature;
  }

  detail::appendFloat1(signature, sensorData.temperature);
  detail::appendFloat1(signature, sensorData.humidity);
  detail::appendFloat0(signature, sensorData.pm1);
  detail::appendFloat0(signature, sensorData.pm25);
  detail::appendFloat0(signature, sensorData.pm4);
  detail::appendFloat0(signature, sensorData.pm10);
  detail::appendFloat0(signature, sensorData.voc);
  detail::appendFloat0(signature, sensorData.nox);
  detail::appendUInt(signature, sensorData.co2);
  detail::appendText(signature, airQualityLabel(sensorData.pm25));
  detail::appendInt(signature, detail::dashboardBarWidth(sensorData.pm25));
  return signature;
}

inline String buildGraphFrameSignature(const airmon::HistoryManager& history,
                                       const airmon::MetricId metric,
                                       const airmon::HistoryRange range,
                                       const float currentValue,
                                       const bool currentValueValid) {
  String signature = "graph|";
  detail::appendInt(signature, static_cast<int>(metric));
  detail::appendInt(signature, static_cast<int>(range));
  detail::appendUInt(signature, history.revision(metric, range));
  detail::appendUInt(signature, static_cast<uint32_t>(history.pointCount(metric, range)));

  char buffer[32];
  detail::formatGraphMetricValue(metric, currentValue, currentValueValid, buffer, sizeof(buffer));
  detail::appendText(signature, buffer);
  detail::appendUInt(signature, detail::graphTrendDirection(history, metric, range, currentValue, currentValueValid));
  return signature;
}

inline String buildOverrideFrameSignature(const DisplayOverrideState& overrideState) {
  String signature = "override|";
  detail::appendInt(signature, static_cast<int>(overrideState.kind));
  if (overrideState.kind == DisplayOverrideKind::RawCanvas) {
    detail::appendUInt(signature, overrideState.canvasRevision);
    return signature;
  }
  detail::appendText(signature, overrideState.text);
  detail::appendInt(signature, overrideState.textSize);
  detail::appendInt(signature, overrideState.x);
  detail::appendInt(signature, overrideState.y);
  return signature;
}

}  // namespace displayframes
}  // namespace sharp
