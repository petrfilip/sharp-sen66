#pragma once

#include <stddef.h>
#include <stdint.h>

namespace airmon {

enum class ViewId : uint8_t {
  Dashboard = 0,
  Graph = 1,
};

enum class DisplayMode : uint8_t {
  Manual = 0,
  AutoCycle = 1,
};

enum class ManualScreen : uint8_t {
  Dashboard = 0,
  Graph = 1,
};

enum class MetricId : uint8_t {
  CO2 = 0,
  PM1 = 1,
  PM25 = 2,
  PM4 = 3,
  PM10 = 4,
  TEMP = 5,
  HUM = 6,
  VOC = 7,
  NOX = 8,
  COUNT = 9,
};

enum class HistoryRange : uint8_t {
  Range24H = 0,
  Range7D = 1,
};

struct SensorData {
  float co2 = 0.0f;
  float pm1 = 0.0f;
  float pm25 = 0.0f;
  float pm4 = 0.0f;
  float pm10 = 0.0f;
  float temp = 0.0f;
  float hum = 0.0f;
  float voc = 0.0f;
  float nox = 0.0f;
};

constexpr size_t kMetricCount = static_cast<size_t>(MetricId::COUNT);
constexpr uint8_t kMetricCountU8 = static_cast<uint8_t>(MetricId::COUNT);

inline bool isValidMetricId(const int value) {
  return value >= 0 && value < static_cast<int>(MetricId::COUNT);
}

inline bool isValidMetricIdValue(const uint8_t value) { return value < static_cast<uint8_t>(MetricId::COUNT); }

inline const char* metricLabel(const MetricId metric) {
  switch (metric) {
    case MetricId::CO2:
      return "CO2";
    case MetricId::PM1:
      return "PM1";
    case MetricId::PM25:
      return "PM2.5";
    case MetricId::PM4:
      return "PM4";
    case MetricId::PM10:
      return "PM10";
    case MetricId::TEMP:
      return "Teplota";
    case MetricId::HUM:
      return "Vlhkost";
    case MetricId::VOC:
      return "VOC";
    case MetricId::NOX:
      return "NOx";
    case MetricId::COUNT:
      break;
  }
  return "?";
}

inline const char* metricUnit(const MetricId metric) {
  switch (metric) {
    case MetricId::CO2:
      return "ppm";
    case MetricId::PM1:
    case MetricId::PM25:
    case MetricId::PM4:
    case MetricId::PM10:
      return "ug/m3";
    case MetricId::TEMP:
      return "C";
    case MetricId::HUM:
      return "%";
    case MetricId::VOC:
    case MetricId::NOX:
      return "idx";
    case MetricId::COUNT:
      break;
  }
  return "";
}

inline const char* rangeLabel(const HistoryRange range) {
  switch (range) {
    case HistoryRange::Range24H:
      return "24h";
    case HistoryRange::Range7D:
      return "7d";
  }
  return "";
}

inline const char* displayModeLabel(const DisplayMode mode) {
  switch (mode) {
    case DisplayMode::Manual:
      return "manual";
    case DisplayMode::AutoCycle:
      return "auto-cycle";
  }
  return "manual";
}

inline const char* viewLabel(const ViewId view) {
  switch (view) {
    case ViewId::Dashboard:
      return "dashboard";
    case ViewId::Graph:
      return "graph";
  }
  return "dashboard";
}

inline MetricId nextMetric(const MetricId metric) {
  const auto index = static_cast<uint8_t>(metric);
  const auto nextIndex = static_cast<uint8_t>((index + 1U) % static_cast<uint8_t>(MetricId::COUNT));
  return static_cast<MetricId>(nextIndex);
}

inline bool metricUsesSingleDecimal(const MetricId metric) {
  switch (metric) {
    case MetricId::PM1:
    case MetricId::PM25:
    case MetricId::PM4:
    case MetricId::PM10:
    case MetricId::TEMP:
    case MetricId::HUM:
      return true;
    case MetricId::CO2:
    case MetricId::VOC:
    case MetricId::NOX:
    case MetricId::COUNT:
      return false;
  }
  return false;
}

inline float metricTrendThreshold(const MetricId metric) {
  switch (metric) {
    case MetricId::CO2:
      return 10.0f;
    case MetricId::PM1:
    case MetricId::PM25:
    case MetricId::PM4:
    case MetricId::PM10:
      return 1.0f;
    case MetricId::TEMP:
      return 0.3f;
    case MetricId::HUM:
      return 1.0f;
    case MetricId::VOC:
    case MetricId::NOX:
      return 3.0f;
    case MetricId::COUNT:
      return 0.0f;
  }
  return 0.0f;
}

inline float metricValue(const SensorData& data, const MetricId metric) {
  switch (metric) {
    case MetricId::CO2:
      return data.co2;
    case MetricId::PM1:
      return data.pm1;
    case MetricId::PM25:
      return data.pm25;
    case MetricId::PM4:
      return data.pm4;
    case MetricId::PM10:
      return data.pm10;
    case MetricId::TEMP:
      return data.temp;
    case MetricId::HUM:
      return data.hum;
    case MetricId::VOC:
      return data.voc;
    case MetricId::NOX:
      return data.nox;
    case MetricId::COUNT:
      break;
  }
  return 0.0f;
}

}  // namespace airmon
