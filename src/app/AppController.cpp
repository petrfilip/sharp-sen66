#include "AppController.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_SharpMem.h>
#include <SensirionI2cSen66.h>
#include <esp_wifi.h>

#include "DashboardRenderer.h"
#include "DisplayFrameSignature.h"
#include "MqttSupport.h"
#include "SensorService.h"
#include "TmepClient.h"
#include "WebUi.h"
#include "WifiProvisioning.h"
#include "button.h"
#include "config.h"
#include "graph_renderer.h"
#include "history_manager.h"
#include "metrics.h"
#include "time_manager.h"

namespace sharp {

namespace {

constexpr uint8_t kPinSpiClk = 6;
constexpr uint8_t kPinSpiMosi = 7;
constexpr uint8_t kPinSpiCs = 3;
constexpr uint8_t kPinSda = 10;
constexpr uint8_t kPinScl = 8;
constexpr uint8_t kPinButton = 4;
constexpr uint16_t kDisplayWidth = 400;
constexpr uint16_t kDisplayHeight = 240;
constexpr uint16_t kBlack = 0;
constexpr unsigned long kSensorReadInterval = 2000UL;
constexpr unsigned long kMqttReconnectInterval = 5000UL;
constexpr unsigned long kHistorySampleInterval = 60000UL;
constexpr unsigned long kWifiDebugOverlayDuration = 30000UL;

struct WifiDebugSnapshot {
  bool connected = false;
  bool targetFound = false;
  String ssid = "-";
  String ip = "-";
  String bssid = "-";
  String auth = "-";
  int32_t rssi = 0;
  int32_t channel = 0;
};

String wifiAuthModeLabel(const wifi_auth_mode_t authMode) {
  switch (authMode) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3";
    case WIFI_AUTH_WPA2_ENTERPRISE:
      return "WPA2-ENT";
    case WIFI_AUTH_WAPI_PSK:
      return "WAPI";
    case WIFI_AUTH_WPA3_ENT_192:
      return "WPA3-ENT";
    case WIFI_AUTH_MAX:
    default:
      return "?";
  }
}

String wifiBandLabel(const int32_t channel) {
  if (channel > 0) {
    return String("2.4 GHz CH ") + String(channel);
  }
  return "2.4 GHz";
}

String buildWifiDebugOverlayText(const WifiDebugSnapshot& snapshot, const String& actionStatus) {
  String text;
  text.reserve(256);
  text += "WiFi reconnect\n";
  text += "SSID: ";
  text += snapshot.ssid;
  text += "\nAction: ";
  text += actionStatus;
  text += "\nTarget: ";
  text += snapshot.targetFound ? "FOUND" : "NOT FOUND";
  text += "\nRSSI: ";
  if (snapshot.connected || snapshot.targetFound) {
    text += String(snapshot.rssi);
    text += " dBm";
  } else {
    text += "-";
  }
  text += "\nAuth: ";
  text += snapshot.auth;
  text += "\nBand: ";
  text += wifiBandLabel(snapshot.channel);
  text += "\nBSSID: ";
  text += snapshot.bssid;
  text += "\nIP: ";
  text += snapshot.ip;
  return text;
}

}  // namespace

class AppController::Impl : public WebUi::Delegate {
 public:
  Impl();

  void setup();
  void loop();

 private:
  friend class AppController;

  WebUiDataSnapshot buildWebUiData() const override;
  const AppConfig& webUiConfig() const override;
  WebUiActionResult applyWebUiConfig(const AppConfig& updatedConfig) override;
  WebUiActionResult applyWebUiDisplayConfig(const WebUiDisplayConfig& displayConfig) override;
  WebUiActionResult reconnectWebUiWifi(const String& ssid, const String& password) override;
  WebUiActionResult saveWebUiWifi(const String& ssid, const String& password) override;
  WebUiActionResult forgetWebUiWifi() override;
  WebUiActionResult sendWebUiTmep() override;
  bool isWebUiCaptiveMode() const override;
  String webUiCaptiveIp() const override;

  void applyDisplaySettings();
  void applyEffectiveViewState(unsigned long nowMs);
  void resetAutoCycleTimer(unsigned long nowMs);
  void maybeAdvanceAutoCycle(unsigned long nowMs);
  void handleViewShortPress(unsigned long nowMs);
  void handleViewLongPress(unsigned long nowMs);
  void maybeStoreHistorySample(unsigned long nowMs);
  airmon::SensorData buildHistorySample() const;
  float liveMetricValue(airmon::MetricId metric) const;
  void formatDashboardDateTime(char* buffer, size_t bufferSize) const;
  bool isMqttConnected() const;
  DashboardRenderInfo buildDashboardRenderInfo() const;
  String buildCurrentFrameSignature(const DashboardRenderInfo* dashboardInfo) const;
  WifiDebugSnapshot captureWifiDebugSnapshot();
  void showTemporaryDisplayMessage(const String& text, unsigned long durationMs, int textSize = 1,
                                   int x = 6, int y = 8);
  void processPendingWifiReconnect();
  void drawCustomTextScreen();
  void renderCurrentView();
  void handleMqttMessage(char* topic, byte* payload, unsigned int length);
  bool reconnectMqtt();
  airmon::DisplayMode configuredDisplayMode() const;
  airmon::DisplayMode effectiveDisplayMode() const;
  airmon::ViewId configuredManualView() const;
  airmon::ViewId effectiveManualView() const;
  airmon::MetricId configuredGraphMetric() const;
  airmon::MetricId effectiveGraphMetric() const;
  airmon::HistoryRange configuredGraphRange() const;
  airmon::HistoryRange effectiveGraphRange() const;

  static void mqttCallbackThunk(char* topic, byte* payload, unsigned int length);

  Adafruit_SharpMem display_;
  SensirionI2cSen66 sen66_;
  WiFiClient wifiClient_;
  PubSubClient mqtt_;
  WebServer webServer_;
  WebUi webUi_;
  WifiProvisioning wifiProvisioning_;
  airmon::HistoryManager historyManager_;
  airmon::Button viewButton_;
  airmon::TimeManager timeManager_;
  airmon::GraphRenderer graphRenderer_;
  DashboardRenderer dashboardRenderer_;
  SensorService sensorService_;
  TmepClient tmepClient_;
  AppConfig appConfig_;
  SensorSnapshot sensorData_;
  AppRuntimeState runtime_;
  bool pendingWifiReconnect_ = false;
};

namespace {

AppController* g_controller = nullptr;

}  // namespace

AppController::Impl::Impl()
    : display_(kPinSpiClk, kPinSpiMosi, kPinSpiCs, kDisplayWidth, kDisplayHeight),
      mqtt_(wifiClient_),
      webServer_(80),
      webUi_(webServer_, *this),
      graphRenderer_(display_),
      dashboardRenderer_(display_),
      sensorService_(Wire, sen66_, kPinSda, kPinScl) {}

void AppController::Impl::setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n========================================");
  Serial.println("  Sharp LCD + SEN66 + MQTT v2.0.0");
  Serial.println("========================================\n");

  const bool configLoaded = loadConfig(appConfig_);
  Serial.printf("CFG: load %s\n", configLoaded ? "OK" : "FAILED - defaults");
  Serial.printf("CFG: MQTT %s:%d, MQTT interval=%lu ms, TMEP interval=%lu ms\n",
                appConfig_.mqttServer.c_str(), appConfig_.mqttPort,
                appConfig_.mqttPublishInterval, appConfig_.tmepRequestInterval);
  Serial.printf("CFG: TMEP domena: %s\n",
                appConfig_.tmepDomain.length() ? appConfig_.tmepDomain.c_str() : "(nenastaveno)");
  Serial.printf("CFG: temperature offset=%.2f\n", appConfig_.temperatureOffset);

  applyEffectiveViewState(millis());
  runtime_.lastHistorySample = millis();

  Serial.println("Display: Inicializace...");
  display_.begin();
  applyDisplaySettings();
  display_.clearDisplay();
  display_.setTextColor(kBlack);
  dashboardRenderer_.renderSplash(appConfig_.wifiSsid, appConfig_.mqttServer);
  Serial.println("Display: OK!");

  wifiProvisioning_.begin(&appConfig_, 20000UL);
  runtime_.wasWifiConnected = wifiProvisioning_.getState() == WIFI_STA_CONNECTED;

  mqtt_.setServer(appConfig_.mqttServer.c_str(), appConfig_.mqttPort);
  mqtt_.setCallback(mqttCallbackThunk);
  mqtt_.setBufferSize(1024);

  if (runtime_.wasWifiConnected && appConfig_.mqttServer.length() > 0) {
    timeManager_.initTime();
    reconnectMqtt();
  }

  webUi_.begin();
  sensorService_.begin();
  runtime_.sen66Ready = sensorService_.isReady();
  viewButton_.begin(kPinButton);

  delay(2000);
  Serial.println("\n=== SETUP HOTOV ===\n");
}

void AppController::Impl::loop() {
  const unsigned long now = millis();

  wifiProvisioning_.process();
  webServer_.handleClient();
  if (pendingWifiReconnect_) {
    processPendingWifiReconnect();
  }

  const bool wifiConnected = wifiProvisioning_.getState() == WIFI_STA_CONNECTED;
  if (wifiConnected && !runtime_.wasWifiConnected) {
    timeManager_.initTime();
    if (appConfig_.mqttServer.length() > 0) {
      reconnectMqtt();
    }
  }
  runtime_.wasWifiConnected = wifiConnected;

  if (wifiConnected && appConfig_.mqttServer.length() > 0 && !mqtt_.connected()) {
    if (now - runtime_.lastMqttReconnect > kMqttReconnectInterval) {
      runtime_.lastMqttReconnect = now;
      reconnectMqtt();
    }
  }

  if (mqtt_.connected()) {
    mqtt_.loop();
  }

  if (now - runtime_.lastSensorRead > kSensorReadInterval) {
    runtime_.lastSensorRead = now;
    if (sensorService_.read(appConfig_, sensorData_)) {
      runtime_.lastValidSensorRead = now;
      if (runtime_.firstValidSensorAt == 0) {
        runtime_.firstValidSensorAt = now;
      }
    }
    runtime_.sen66Ready = sensorService_.isReady();
  }

  maybeStoreHistorySample(now);

  viewButton_.update(now);
  if (viewButton_.wasShortPressed()) {
    handleViewShortPress(now);
  }
  if (viewButton_.wasLongPressed()) {
    handleViewLongPress(now);
  }

  maybeAdvanceAutoCycle(now);

  if (now - runtime_.lastMqttPublish > appConfig_.mqttPublishInterval) {
    runtime_.lastMqttPublish = now;
    mqttsupport::publishSensorData(mqtt_, sensorData_, runtime_.firstValidSensorAt,
                                   appConfig_.mqttWarmupDelay, now);
  }

  if (now - runtime_.lastTmepRequest > appConfig_.tmepRequestInterval) {
    runtime_.lastTmepRequest = now;
    tmepClient_.sendRequest(appConfig_, sensorData_, runtime_.lastTmepStatus, false);
  }

  if (runtime_.displayOverride.active && now > runtime_.displayOverride.untilMs) {
    runtime_.displayOverride.active = false;
    resetAutoCycleTimer(now);
    Serial.println("Display: Override expired, zpet na aktualni view");
  }

  if (now - runtime_.lastDisplayRefresh > appConfig_.displayRefreshInterval) {
    runtime_.lastDisplayRefresh = now;
    renderCurrentView();
  }

  delay(10);
}

void AppController::Impl::applyDisplaySettings() {
  display_.setRotation(appConfig_.displayRotation % 4);
  if (appConfig_.displayInvertRequested) {
    Serial.println("Display: Inverze je pozadovana, HW inverze neni na Sharp LCD podporovana.");
  }
}

airmon::DisplayMode AppController::Impl::configuredDisplayMode() const {
  return appConfig_.displayMode == static_cast<uint8_t>(airmon::DisplayMode::AutoCycle)
             ? airmon::DisplayMode::AutoCycle
             : airmon::DisplayMode::Manual;
}

airmon::DisplayMode AppController::Impl::effectiveDisplayMode() const {
  if (runtime_.displayConfigOverride.active) {
    return runtime_.displayConfigOverride.mode;
  }
  return configuredDisplayMode();
}

airmon::ViewId AppController::Impl::configuredManualView() const {
  return appConfig_.displayScreen == static_cast<uint8_t>(airmon::ManualScreen::Graph)
             ? airmon::ViewId::Graph
             : airmon::ViewId::Dashboard;
}

airmon::ViewId AppController::Impl::effectiveManualView() const {
  if (runtime_.displayConfigOverride.active) {
    return runtime_.displayConfigOverride.manualView;
  }
  return configuredManualView();
}

airmon::MetricId AppController::Impl::configuredGraphMetric() const {
  return static_cast<airmon::MetricId>(appConfig_.displayGraphMetric);
}

airmon::MetricId AppController::Impl::effectiveGraphMetric() const {
  if (runtime_.displayConfigOverride.active) {
    return runtime_.displayConfigOverride.graphMetric;
  }
  return configuredGraphMetric();
}

airmon::HistoryRange AppController::Impl::configuredGraphRange() const {
  return appConfig_.displayGraphRange == static_cast<uint8_t>(airmon::HistoryRange::Range7D)
             ? airmon::HistoryRange::Range7D
             : airmon::HistoryRange::Range24H;
}

airmon::HistoryRange AppController::Impl::effectiveGraphRange() const {
  if (runtime_.displayConfigOverride.active) {
    return runtime_.displayConfigOverride.graphRange;
  }
  return configuredGraphRange();
}

void AppController::Impl::resetAutoCycleTimer(const unsigned long nowMs) {
  runtime_.lastAutoCycleAt = nowMs;
}

void AppController::Impl::applyEffectiveViewState(const unsigned long nowMs) {
  runtime_.currentView = effectiveManualView();
  runtime_.currentGraphMetric = effectiveGraphMetric();
  runtime_.currentGraphRange = effectiveGraphRange();
  resetAutoCycleTimer(nowMs);
}

void AppController::Impl::maybeAdvanceAutoCycle(const unsigned long nowMs) {
  if (runtime_.displayOverride.active || effectiveDisplayMode() != airmon::DisplayMode::AutoCycle) {
    return;
  }
  if ((nowMs - runtime_.lastAutoCycleAt) < appConfig_.displayCycleInterval) {
    return;
  }

  runtime_.lastAutoCycleAt = nowMs;
  if (runtime_.currentView == airmon::ViewId::Dashboard) {
    runtime_.currentView = airmon::ViewId::Graph;
    return;
  }

  const airmon::MetricId nextMetric = airmon::nextMetric(runtime_.currentGraphMetric);
  runtime_.currentGraphMetric = nextMetric;
  if (nextMetric == effectiveGraphMetric()) {
    runtime_.currentView = airmon::ViewId::Dashboard;
  }
}

void AppController::Impl::handleViewShortPress(const unsigned long nowMs) {
  if (runtime_.currentView == airmon::ViewId::Dashboard) {
    runtime_.currentView = airmon::ViewId::Graph;
  } else {
    runtime_.currentGraphMetric = airmon::nextMetric(runtime_.currentGraphMetric);
  }
  resetAutoCycleTimer(nowMs);
}

void AppController::Impl::handleViewLongPress(const unsigned long nowMs) {
  if (runtime_.currentView == airmon::ViewId::Dashboard) {
    runtime_.currentView = airmon::ViewId::Graph;
  } else {
    runtime_.currentGraphRange = runtime_.currentGraphRange == airmon::HistoryRange::Range24H
                                     ? airmon::HistoryRange::Range7D
                                     : airmon::HistoryRange::Range24H;
  }
  resetAutoCycleTimer(nowMs);
}

void AppController::Impl::maybeStoreHistorySample(const unsigned long nowMs) {
  while ((nowMs - runtime_.lastHistorySample) >= kHistorySampleInterval) {
    runtime_.lastHistorySample += kHistorySampleInterval;
    const bool freshSensorData =
        sensorData_.valid && runtime_.lastValidSensorRead != 0 &&
        (nowMs - runtime_.lastValidSensorRead) <= (kSensorReadInterval * 2UL);
    if (freshSensorData) {
      historyManager_.addMinuteSample(buildHistorySample());
    }
  }
}

airmon::SensorData AppController::Impl::buildHistorySample() const {
  airmon::SensorData sample;
  sample.co2 = static_cast<float>(sensorData_.co2);
  sample.pm25 = sensorData_.pm25;
  sample.temp = sensorData_.temperature;
  sample.hum = sensorData_.humidity;
  sample.voc = sensorData_.voc;
  sample.nox = sensorData_.nox;
  return sample;
}

float AppController::Impl::liveMetricValue(const airmon::MetricId metric) const {
  switch (metric) {
    case airmon::MetricId::CO2:
      return static_cast<float>(sensorData_.co2);
    case airmon::MetricId::PM25:
      return sensorData_.pm25;
    case airmon::MetricId::TEMP:
      return sensorData_.temperature;
    case airmon::MetricId::HUM:
      return sensorData_.humidity;
    case airmon::MetricId::VOC:
      return sensorData_.voc;
    case airmon::MetricId::NOX:
      return sensorData_.nox;
    case airmon::MetricId::COUNT:
      break;
  }
  return 0.0f;
}

void AppController::Impl::formatDashboardDateTime(char* buffer, const size_t bufferSize) const {
  if (!timeManager_.formatDateTime(buffer, bufferSize)) {
    snprintf(buffer, bufferSize, "NO TIME");
  }
}

bool AppController::Impl::isMqttConnected() const {
  return const_cast<PubSubClient&>(mqtt_).connected();
}

DashboardRenderInfo AppController::Impl::buildDashboardRenderInfo() const {
  DashboardRenderInfo info;
  if (WiFi.status() == WL_CONNECTED) {
    info.wifiText = "WiFi:" + WiFi.localIP().toString();
  } else {
    info.wifiText = "WiFi:---";
  }

  info.tmepText = runtime_.lastTmepStatus;
  info.mqttConnected = isMqttConnected();
  info.sen66Ready = runtime_.sen66Ready;

  const unsigned long uptimeSec = millis() / 1000UL;
  const unsigned long hrs = uptimeSec / 3600UL;
  const unsigned long mins = (uptimeSec % 3600UL) / 60UL;
  char uptimeBuf[32];
  snprintf(uptimeBuf, sizeof(uptimeBuf), "Uptime:%luh%02lum", hrs, mins);
  info.uptimeText = uptimeBuf;

  char dateTimeBuf[32];
  formatDashboardDateTime(dateTimeBuf, sizeof(dateTimeBuf));
  info.dateTimeText = dateTimeBuf;
  return info;
}

String AppController::Impl::buildCurrentFrameSignature(const DashboardRenderInfo* dashboardInfo) const {
  if (runtime_.displayOverride.active) {
    return displayframes::buildOverrideFrameSignature(runtime_.displayOverride);
  }

  if (runtime_.currentView == airmon::ViewId::Graph) {
    return displayframes::buildGraphFrameSignature(historyManager_, runtime_.currentGraphMetric,
                                                   runtime_.currentGraphRange, liveMetricValue(runtime_.currentGraphMetric),
                                                   sensorData_.valid);
  }

  DashboardRenderInfo info;
  const DashboardRenderInfo* infoToUse = dashboardInfo;
  if (infoToUse == nullptr) {
    info = buildDashboardRenderInfo();
    infoToUse = &info;
  }

  return displayframes::buildDashboardFrameSignature(sensorData_, infoToUse->wifiText, infoToUse->tmepText,
                                                     infoToUse->mqttConnected, infoToUse->sen66Ready,
                                                     infoToUse->uptimeText, infoToUse->dateTimeText);
}

WifiDebugSnapshot AppController::Impl::captureWifiDebugSnapshot() {
  WifiDebugSnapshot snapshot;
  snapshot.ssid = appConfig_.wifiSsid.length() ? appConfig_.wifiSsid : "-";

  if (WiFi.status() == WL_CONNECTED) {
    snapshot.connected = true;
    snapshot.targetFound = true;
    snapshot.ssid = WiFi.SSID();
    snapshot.ip = WiFi.localIP().toString();
    snapshot.bssid = WiFi.BSSIDstr();
    snapshot.rssi = WiFi.RSSI();
    snapshot.channel = WiFi.channel();

    wifi_ap_record_t apInfo = {};
    if (esp_wifi_sta_get_ap_info(&apInfo) == ESP_OK) {
      snapshot.auth = wifiAuthModeLabel(apInfo.authmode);
      snapshot.rssi = apInfo.rssi;
      snapshot.channel = apInfo.primary;
    }
    return snapshot;
  }

  if (!appConfig_.wifiSsid.length()) {
    return snapshot;
  }

  const wifi_mode_t currentMode = WiFi.getMode();
  if (currentMode != WIFI_STA && currentMode != WIFI_AP_STA) {
    WiFi.mode(WIFI_STA);
    delay(50);
  }

  const int16_t networkCount =
      WiFi.scanNetworks(false, true, false, 150, 0, appConfig_.wifiSsid.c_str(), nullptr);
  if (networkCount > 0) {
    snapshot.targetFound = true;
    snapshot.rssi = WiFi.RSSI(0);
    snapshot.channel = WiFi.channel(0);
    snapshot.bssid = WiFi.BSSIDstr(0);
    snapshot.auth = wifiAuthModeLabel(WiFi.encryptionType(0));
  }
  WiFi.scanDelete();
  return snapshot;
}

void AppController::Impl::showTemporaryDisplayMessage(const String& text,
                                                      const unsigned long durationMs,
                                                      const int textSize,
                                                      const int x,
                                                      const int y) {
  runtime_.displayOverride.text = text;
  runtime_.displayOverride.textSize = textSize;
  runtime_.displayOverride.x = x;
  runtime_.displayOverride.y = y;
  runtime_.displayOverride.active = true;
  runtime_.displayOverride.untilMs = millis() + durationMs;
  drawCustomTextScreen();
}

void AppController::Impl::processPendingWifiReconnect() {
  pendingWifiReconnect_ = false;

  const WifiDebugSnapshot snapshot = captureWifiDebugSnapshot();

  mqtt_.disconnect();
  runtime_.lastMqttReconnect = 0;

  String statusMessage;
  const bool ok = wifiProvisioning_.reconnectWithoutSaving(statusMessage);
  showTemporaryDisplayMessage(buildWifiDebugOverlayText(snapshot, statusMessage), kWifiDebugOverlayDuration);
  Serial.printf("WIFI: Manual reconnect %s (%s)\n", ok ? "queued" : "failed", statusMessage.c_str());
}

void AppController::Impl::drawCustomTextScreen() {
  display_.clearDisplay();
  display_.setTextColor(kBlack);
  display_.setTextSize(runtime_.displayOverride.textSize);
  display_.setCursor(runtime_.displayOverride.x, runtime_.displayOverride.y);
  display_.println(runtime_.displayOverride.text);
  display_.refresh();
}

void AppController::Impl::renderCurrentView() {
  DashboardRenderInfo dashboardInfo;
  const DashboardRenderInfo* dashboardInfoPtr = nullptr;
  if (!runtime_.displayOverride.active && runtime_.currentView == airmon::ViewId::Dashboard) {
    dashboardInfo = buildDashboardRenderInfo();
    dashboardInfoPtr = &dashboardInfo;
  }

  const String frameSignature = buildCurrentFrameSignature(dashboardInfoPtr);
  if (frameSignature == runtime_.lastDisplaySignature) {
    // Sharp Memory LCD still needs a periodic refresh to keep VCOM inversion running.
    display_.refresh();
    return;
  }

  if (runtime_.displayOverride.active) {
    drawCustomTextScreen();
    runtime_.lastDisplaySignature = frameSignature;
    return;
  }

  if (runtime_.currentView == airmon::ViewId::Graph) {
    const bool forceFullGraphRedraw = !runtime_.lastDisplaySignature.startsWith("graph|");
    graphRenderer_.render(historyManager_, runtime_.currentGraphMetric, runtime_.currentGraphRange,
                          liveMetricValue(runtime_.currentGraphMetric), sensorData_.valid, forceFullGraphRedraw);
    runtime_.lastDisplaySignature = frameSignature;
    return;
  }

  const bool forceFullDashboardRedraw = !runtime_.lastDisplaySignature.startsWith("dashboard|");
  dashboardRenderer_.render(sensorData_, dashboardInfo, forceFullDashboardRedraw);
  runtime_.lastDisplaySignature = frameSignature;
}

WebUiDataSnapshot AppController::Impl::buildWebUiData() const {
  WebUiDataSnapshot data;
  data.wifiStatus = WiFi.status() == WL_CONNECTED ? "connected" : "disconnected";
  data.mqttStatus = isMqttConnected() ? "connected" : "disconnected";
  data.valid = sensorData_.valid;
  data.displayTemporary = runtime_.displayConfigOverride.active;
  data.uptimeSeconds = millis() / 1000UL;
  data.tmepUrl = tmepClient_.buildRequestUrl(appConfig_, sensorData_);
  data.tmepStatus = runtime_.lastTmepStatus;
  data.wifiMode = wifiProvisioning_.getStateText();
  data.displayMode = airmon::displayModeLabel(effectiveDisplayMode());
  data.currentView = airmon::viewLabel(runtime_.currentView);
  data.currentMetric = airmon::metricLabel(runtime_.currentGraphMetric);
  data.currentRange = airmon::rangeLabel(runtime_.currentGraphRange);
  data.apSsid = wifiProvisioning_.isCaptiveMode() ? wifiProvisioning_.getApSsid() : "";
  data.apIp = wifiProvisioning_.isCaptiveMode() ? wifiProvisioning_.getApIp() : "";
  data.currentSsid = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
  data.currentIp = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  data.rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;

  data.temperature = sensorData_.temperature;
  data.humidity = sensorData_.humidity;
  data.pm1 = sensorData_.pm1;
  data.pm25 = sensorData_.pm25;
  data.pm4 = sensorData_.pm4;
  data.pm10 = sensorData_.pm10;
  data.voc = sensorData_.voc;
  data.nox = sensorData_.nox;
  data.co2 = sensorData_.co2;
  return data;
}

const AppConfig& AppController::Impl::webUiConfig() const { return appConfig_; }

WebUiActionResult AppController::Impl::applyWebUiConfig(const AppConfig& updatedConfig) {
  if (!validateConfig(updatedConfig)) {
    WebUiActionResult result;
    result.statusCode = 400;
    result.message = "Neplatne hodnoty konfigurace";
    return result;
  }

  if (!saveConfig(updatedConfig)) {
    WebUiActionResult result;
    result.statusCode = 500;
    result.message = "Nepodarilo se ulozit konfiguraci";
    return result;
  }

  appConfig_ = updatedConfig;
  mqtt_.setServer(appConfig_.mqttServer.c_str(), appConfig_.mqttPort);
  runtime_.displayConfigOverride.active = false;
  applyDisplaySettings();
  applyEffectiveViewState(millis());

  WebUiActionResult result;
  result.ok = true;
  result.statusCode = 200;
  result.message = "Konfigurace ulozena, zarizeni se restartuje...";
  result.restartRequired = true;
  return result;
}

WebUiActionResult AppController::Impl::applyWebUiDisplayConfig(const WebUiDisplayConfig& displayConfig) {
  runtime_.displayConfigOverride.active = !displayConfig.resetToSaved;
  if (runtime_.displayConfigOverride.active) {
    runtime_.displayConfigOverride.mode =
        displayConfig.displayMode == static_cast<uint8_t>(airmon::DisplayMode::AutoCycle)
            ? airmon::DisplayMode::AutoCycle
            : airmon::DisplayMode::Manual;
    runtime_.displayConfigOverride.manualView =
        displayConfig.displayScreen == static_cast<uint8_t>(airmon::ManualScreen::Graph)
            ? airmon::ViewId::Graph
            : airmon::ViewId::Dashboard;
    runtime_.displayConfigOverride.graphMetric =
        static_cast<airmon::MetricId>(displayConfig.displayGraphMetric);
    runtime_.displayConfigOverride.graphRange =
        displayConfig.displayGraphRange == static_cast<uint8_t>(airmon::HistoryRange::Range7D)
            ? airmon::HistoryRange::Range7D
            : airmon::HistoryRange::Range24H;
  }

  runtime_.displayOverride.active = false;
  applyEffectiveViewState(millis());
  renderCurrentView();

  WebUiActionResult result;
  result.ok = true;
  result.statusCode = 200;
  result.message = displayConfig.resetToSaved
                       ? "Ulozene zobrazeni bylo obnoveno bez restartu"
                       : "Docasne zobrazeni aplikovano bez restartu";
  return result;
}

WebUiActionResult AppController::Impl::reconnectWebUiWifi(const String& ssid, const String& password) {
  WebUiActionResult result;

  const bool credentialsChanged = ssid != appConfig_.wifiSsid || password != appConfig_.wifiPassword;
  if (!credentialsChanged) {
    if (!appConfig_.wifiSsid.length()) {
      result.statusCode = 400;
      result.message = "SSID neni nastavene, nejdriv ho uloz";
      result.wifiMode = wifiProvisioning_.getStateText();
      return result;
    }

    pendingWifiReconnect_ = true;
    result.ok = true;
    result.statusCode = 200;
    result.message = "Wi-Fi reconnect naplanovan bez zapisu do flash";
    result.wifiMode = wifiProvisioning_.getStateText();
    return result;
  }

  if (!ssid.length()) {
    result.statusCode = 400;
    result.message = "SSID nesmi byt prazdne";
    result.wifiMode = wifiProvisioning_.getStateText();
    return result;
  }

  AppConfig updated = appConfig_;
  updated.wifiSsid = ssid;
  updated.wifiPassword = password;
  if (!validateConfig(updated)) {
    result.statusCode = 400;
    result.message = "Neplatne hodnoty Wi-Fi konfigurace";
    result.wifiMode = wifiProvisioning_.getStateText();
    return result;
  }

  if (!saveConfig(updated)) {
    result.statusCode = 500;
    result.message = "Nepodarilo se ulozit Wi-Fi konfiguraci";
    result.wifiMode = wifiProvisioning_.getStateText();
    return result;
  }

  pendingWifiReconnect_ = false;
  appConfig_ = updated;
  result.ok = true;
  result.statusCode = 200;
  result.message = "Wi-Fi udaje ulozeny, zarizeni se restartuje";
  result.wifiMode = wifiProvisioning_.getStateText();
  result.restartRequired = true;
  return result;
}

WebUiActionResult AppController::Impl::saveWebUiWifi(const String& ssid, const String& password) {
  String message;
  const bool ok = wifiProvisioning_.saveCredentialsAndConnect(ssid, password, message);
  WebUiActionResult result;
  result.ok = ok;
  result.statusCode = ok ? 200 : 400;
  result.message = message;
  result.wifiMode = wifiProvisioning_.getStateText();
  result.restartRequired = ok;
  return result;
}

WebUiActionResult AppController::Impl::forgetWebUiWifi() {
  const bool ok = wifiProvisioning_.forgetCredentials();
  WebUiActionResult result;
  result.ok = ok;
  result.statusCode = ok ? 200 : 500;
  result.message =
      ok ? "Wi-Fi zapomenuta, restartuji do captive AP" : "Nepodarilo se zapomenout Wi-Fi";
  result.wifiMode = wifiProvisioning_.getStateText();
  result.restartRequired = ok;
  return result;
}

WebUiActionResult AppController::Impl::sendWebUiTmep() {
  const bool ok = tmepClient_.sendRequest(appConfig_, sensorData_, runtime_.lastTmepStatus, true);
  WebUiActionResult result;
  result.ok = ok;
  result.statusCode = ok ? 200 : 500;
  result.message = ok ? "TMEP request byl uspesne odeslan"
                      : "TMEP request se nepodarilo odeslat (zkontrolujte URL, WiFi a data)";
  return result;
}

bool AppController::Impl::isWebUiCaptiveMode() const { return wifiProvisioning_.isCaptiveMode(); }

String AppController::Impl::webUiCaptiveIp() const { return wifiProvisioning_.getApIp(); }

void AppController::Impl::mqttCallbackThunk(char* topic, byte* payload, unsigned int length) {
  if (g_controller != nullptr) {
    g_controller->handleMqttCallback(topic, payload, length);
  }
}

void AppController::Impl::handleMqttMessage(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; ++i) {
    message += static_cast<char>(payload[i]);
  }

  Serial.printf("MQTT RX [%s]: %s\n", topic, message.c_str());

  if (strcmp(topic, mqttsupport::kTopicText) == 0) {
    runtime_.displayOverride.text = message;
    runtime_.displayOverride.textSize = 2;
    runtime_.displayOverride.x = 10;
    runtime_.displayOverride.y = 10;
    runtime_.displayOverride.active = true;
    runtime_.displayOverride.untilMs = millis() + 30000UL;
    drawCustomTextScreen();
    return;
  }

  if (strcmp(topic, mqttsupport::kTopicClear) == 0) {
    runtime_.displayOverride.active = false;
    resetAutoCycleTimer(millis());
    renderCurrentView();
    Serial.println("Display cleared");
    return;
  }

  if (strcmp(topic, mqttsupport::kTopicCommand) != 0) {
    return;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, message);
  if (err) {
    Serial.printf("JSON parse error: %s\n", err.c_str());
    return;
  }

  if (!doc["text"].isNull()) {
    runtime_.displayOverride.text = doc["text"].as<String>();
    runtime_.displayOverride.x = doc["x"] | 10;
    runtime_.displayOverride.y = doc["y"] | 10;
    runtime_.displayOverride.textSize = doc["size"] | 2;
    const int duration = doc["duration"] | 30;
    runtime_.displayOverride.active = true;
    runtime_.displayOverride.untilMs = millis() + (duration * 1000UL);
    drawCustomTextScreen();
  }

  if (doc["line"].is<JsonObjectConst>()) {
    const JsonObject line = doc["line"];
    display_.drawLine(line["x1"] | 0, line["y1"] | 0, line["x2"] | 399, line["y2"] | 0, kBlack);
    display_.refresh();
  }

  if (doc["rect"].is<JsonObjectConst>()) {
    const JsonObject rect = doc["rect"];
    const int x = rect["x"] | 0;
    const int y = rect["y"] | 0;
    const int w = rect["w"] | 50;
    const int h = rect["h"] | 30;
    const bool fill = rect["fill"] | false;
    if (fill) {
      display_.fillRect(x, y, w, h, kBlack);
    } else {
      display_.drawRect(x, y, w, h, kBlack);
    }
    display_.refresh();
  }

  if (!doc["invert"].isNull()) {
    Serial.println("Invert command received");
  }

  if (!doc["dashboard"].isNull()) {
    runtime_.displayOverride.active = false;
    runtime_.currentView = airmon::ViewId::Dashboard;
    resetAutoCycleTimer(millis());
    renderCurrentView();
  }
}

bool AppController::Impl::reconnectMqtt() { return mqttsupport::reconnect(mqtt_, appConfig_); }

AppController::AppController() : impl_(new Impl()) { g_controller = this; }

AppController::~AppController() {
  if (g_controller == this) {
    g_controller = nullptr;
  }
  delete impl_;
}

void AppController::handleMqttCallback(char* topic, unsigned char* payload, unsigned int length) {
  if (impl_ != nullptr) {
    impl_->handleMqttMessage(topic, payload, length);
  }
}

void AppController::setup() { impl_->setup(); }

void AppController::loop() { impl_->loop(); }

}  // namespace sharp
