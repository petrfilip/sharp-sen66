#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "config.h"

namespace sharp {

struct WebUiDataSnapshot {
  String wifiStatus = "disconnected";
  String mqttStatus = "disconnected";
  bool valid = false;
  bool displayTemporary = false;
  unsigned long uptimeSeconds = 0;

  String tmepUrl;
  String tmepStatus;
  String wifiMode;
  String displayMode;
  String currentView;
  String currentMetric;
  String currentRange;

  String apSsid;
  String apIp;
  String currentSsid;
  String currentIp;
  long rssi = 0;

  float temperature = 0.0f;
  float humidity = 0.0f;
  float pm1 = 0.0f;
  float pm25 = 0.0f;
  float pm4 = 0.0f;
  float pm10 = 0.0f;
  float voc = 0.0f;
  float nox = 0.0f;
  uint16_t co2 = 0;
};

struct WebUiActionResult {
  bool ok = false;
  int statusCode = 500;
  String message;
  String wifiMode;
  bool restartRequired = false;
};

struct WebUiDisplayConfig {
  uint8_t displayMode = 0;
  uint8_t displayScreen = 0;
  uint8_t displayGraphMetric = 0;
  uint8_t displayGraphRange = 0;
  bool resetToSaved = false;
};

class WebUi {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual WebUiDataSnapshot buildWebUiData() const = 0;
    virtual const AppConfig& webUiConfig() const = 0;
    virtual WebUiActionResult applyWebUiConfig(const AppConfig& updatedConfig) = 0;
    virtual WebUiActionResult applyWebUiDisplayConfig(const WebUiDisplayConfig& displayConfig) = 0;
    virtual WebUiActionResult reconnectWebUiWifi(const String& ssid, const String& password) = 0;
    virtual WebUiActionResult saveWebUiWifi(const String& ssid, const String& password) = 0;
    virtual WebUiActionResult forgetWebUiWifi() = 0;
    virtual WebUiActionResult sendWebUiTmep() = 0;
    virtual bool isWebUiCaptiveMode() const = 0;
    virtual String webUiCaptiveIp() const = 0;
  };

  WebUi(WebServer& server, Delegate& delegate);

  void begin();

 private:
  void registerRoutes();
  void maybeRestart(const WebUiActionResult& result) const;
  void handleRoot();
  void handleApiData();
  void handleApiConfigGet();
  void handleApiConfigPost();
  void handleApiDisplayRuntimePost();
  void handleApiWifiReconnect();
  void handleApiWifiSave();
  void handleApiWifiForget();
  void handleApiTmepSend();
  void handleCaptiveRedirect();

  WebServer& server_;
  Delegate& delegate_;
};

}  // namespace sharp
