#pragma once

#include <Arduino.h>

#include "metrics.h"

namespace sharp {

struct SensorSnapshot {
  float pm1 = 0.0f;
  float pm25 = 0.0f;
  float pm4 = 0.0f;
  float pm10 = 0.0f;
  float temperature = 0.0f;
  float humidity = 0.0f;
  float voc = 0.0f;
  float nox = 0.0f;
  uint16_t co2 = 0;
  bool valid = false;
};

enum class DisplayOverrideKind : uint8_t {
  Text = 0,
  RawCanvas = 1,
};

struct DisplayOverrideState {
  bool active = false;
  DisplayOverrideKind kind = DisplayOverrideKind::Text;
  unsigned long untilMs = 0;
  String text = "";
  int textSize = 2;
  int x = 10;
  int y = 10;
  uint32_t canvasRevision = 0;
};

struct DisplayConfigOverrideState {
  bool active = false;
  airmon::DisplayMode mode = airmon::DisplayMode::Manual;
  airmon::ViewId manualView = airmon::ViewId::Dashboard;
  airmon::MetricId graphMetric = airmon::MetricId::CO2;
  airmon::HistoryRange graphRange = airmon::HistoryRange::Range24H;
};

struct AppRuntimeState {
  unsigned long lastSensorRead = 0;
  unsigned long lastMqttPublish = 0;
  unsigned long lastDisplayRefresh = 0;
  unsigned long lastMqttReconnect = 0;
  unsigned long lastTmepRequest = 0;
  unsigned long firstValidSensorAt = 0;
  unsigned long lastValidSensorRead = 0;
  unsigned long lastHistorySample = 0;
  unsigned long lastAutoCycleAt = 0;

  String lastTmepStatus = "TMEP:---";

  bool sen66Ready = false;
  bool wasWifiConnected = false;

  airmon::ViewId currentView = airmon::ViewId::Dashboard;
  airmon::MetricId currentGraphMetric = airmon::MetricId::CO2;
  airmon::HistoryRange currentGraphRange = airmon::HistoryRange::Range24H;
  DisplayConfigOverrideState displayConfigOverride;
  DisplayOverrideState displayOverride;
  String lastDisplaySignature = "";
};

inline const char* airQualityLabel(const float pm25) {
  if (pm25 < 12.0f) return "VYNIKAJICI";
  if (pm25 < 35.4f) return "DOBRE";
  if (pm25 < 55.4f) return "PRIJATELNE";
  if (pm25 < 150.4f) return "SPATNE";
  if (pm25 < 250.4f) return "VELMI SPATNE";
  return "NEBEZPECNE";
}

}  // namespace sharp
