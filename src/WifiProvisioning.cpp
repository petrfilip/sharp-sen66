#include "WifiProvisioning.h"

#include <WiFi.h>
#include <esp_wifi.h>

namespace {
constexpr byte DNS_PORT = 53;
constexpr const char* AP_PASSWORD = "";  // open AP for easier onboarding
constexpr unsigned long kApRetryIntervalMs = 30UL * 60UL * 1000UL;
constexpr unsigned long kApRetryProbeTimeoutMs = 15000UL;
constexpr unsigned long kNoPendingActionDelayMs = 60000UL;
constexpr unsigned long kReconnectBackoffScheduleMs[] = {1000UL, 3000UL, 7000UL, 15000UL, 29000UL};

bool timeReached(const unsigned long nowMs, const unsigned long targetMs) {
  return static_cast<long>(nowMs - targetMs) >= 0;
}

unsigned long remainingUntil(const unsigned long nowMs, const unsigned long targetMs) {
  return timeReached(nowMs, targetMs) ? 0UL : (targetMs - nowMs);
}

String buildApName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String("SharpDisplay-") + suffix;
}
}  // namespace

void WifiProvisioning::begin(AppConfig* config, unsigned long connectTimeoutMs) {
  config_ = config;
  connectTimeoutMs_ = connectTimeoutMs;

  if (!config_ || !hasStoredCredentials()) {
    Serial.println("WIFI PROV: Chybi ulozene SSID, start AP captive");
    startCaptiveMode(false);
    return;
  }

  if (!connectStaBlocking(connectTimeoutMs_)) {
    Serial.println("WIFI PROV: STA connect timeout, fallback do captive");
    startCaptiveMode(true);
  }
}

void WifiProvisioning::process() {
  const unsigned long now = millis();

  if (state_ == WIFI_AP_CAPTIVE && captiveRunning_) {
    dnsServer_.processNextRequest();
    if (hasStoredCredentials() && nextApRetryAt_ != 0 && timeReached(now, nextApRetryAt_)) {
      Serial.println("WIFI PROV: AP retry interval reached, zkousim STA probe");
      startStaConnect(true, kApRetryProbeTimeoutMs, true);
    }
    return;
  }

  if (!config_ || !hasStoredCredentials()) return;

  if (state_ == WIFI_STA_CONNECTED && WiFi.status() != WL_CONNECTED) {
    Serial.println("WIFI PROV: Spojeni ztraceno, zkousim reconnect");
    startStaConnect(false, connectTimeoutMs_, false);
    return;
  }

  if (state_ != WIFI_STA_CONNECTING) return;

  if (WiFi.status() == WL_CONNECTED) {
    state_ = WIFI_STA_CONNECTED;
    staConnectStartedAt_ = 0;
    staConnectTimeoutMs_ = 0;
    reconnectBackoffIndex_ = 0;
    nextApRetryAt_ = 0;
    const bool probeFromCaptive = staProbeFromCaptive_;
    staProbeFromCaptive_ = false;
    applyStaPowerSave();
    Serial.printf("WIFI PROV: %s, IP: %s\n",
                  probeFromCaptive ? "AP retry probe uspesny, prechazim do STA" : "Znovu pripojeno",
                  WiFi.localIP().toString().c_str());
    return;
  }

  const unsigned long elapsed = now - staConnectStartedAt_;
  if (reconnectBackoffIndex_ <
          static_cast<uint8_t>(sizeof(kReconnectBackoffScheduleMs) / sizeof(kReconnectBackoffScheduleMs[0])) &&
      elapsed >= kReconnectBackoffScheduleMs[reconnectBackoffIndex_]) {
    ++reconnectBackoffIndex_;
    WiFi.reconnect();
  }

  if (elapsed >= staConnectTimeoutMs_) {
    Serial.println(staProbeFromCaptive_ ? "WIFI PROV: AP retry probe timeout, vracim se do AP"
                                        : "WIFI PROV: Reconnect timeout, prepinam do captive");
    startCaptiveMode(true);
  }
}

bool WifiProvisioning::reconnectWithoutSaving(String& statusMsg) {
  if (!config_) {
    statusMsg = "Interni chyba: config neni inicializovan";
    return false;
  }

  if (!hasStoredCredentials()) {
    statusMsg = "Chybi ulozene Wi-Fi udaje";
    return false;
  }

  startStaConnect(true, connectTimeoutMs_, false);
  statusMsg = "Wi-Fi reconnect spusten bez zapisu do flash";
  return true;
}

bool WifiProvisioning::saveCredentialsAndConnect(const String& ssid, const String& password, String& statusMsg) {
  if (!config_) {
    statusMsg = "Interni chyba: config neni inicializovan";
    return false;
  }

  AppConfig updated = *config_;
  updated.wifiSsid = ssid;
  updated.wifiPassword = password;

  if (updated.wifiSsid.length() == 0) {
    statusMsg = "SSID nesmi byt prazdne";
    return false;
  }

  if (!saveConfig(updated)) {
    statusMsg = "Nepodarilo se ulozit WiFi konfiguraci";
    return false;
  }

  *config_ = updated;
  stopCaptiveMode();

  bool ok = connectStaBlocking(connectTimeoutMs_);
  if (ok) {
    statusMsg = "WiFi ulozena, pripojeno";
    return true;
  }

  startCaptiveMode(true);
  statusMsg = "WiFi ulozena, ale pripojeni selhalo - AP captive zustava aktivni";
  return false;
}

bool WifiProvisioning::forgetCredentials() {
  if (!config_) return false;

  AppConfig updated = *config_;
  updated.wifiSsid = "";
  updated.wifiPassword = "";
  if (!saveConfig(updated)) return false;

  *config_ = updated;
  startCaptiveMode(false);
  return true;
}

String WifiProvisioning::getStateText() const {
  switch (state_) {
    case WIFI_STA_CONNECTED:
      return "WIFI_STA_CONNECTED";
    case WIFI_AP_CAPTIVE:
      return "WIFI_AP_CAPTIVE";
    case WIFI_STA_CONNECTING:
    default:
      return "WIFI_STA_CONNECTING";
  }
}

unsigned long WifiProvisioning::nextActionDelayMs(const unsigned long nowMs) const {
  if (state_ == WIFI_AP_CAPTIVE) {
    if (nextApRetryAt_ == 0 || !hasStoredCredentials()) {
      return kNoPendingActionDelayMs;
    }
    return remainingUntil(nowMs, nextApRetryAt_);
  }

  if (state_ != WIFI_STA_CONNECTING || staConnectStartedAt_ == 0 || staConnectTimeoutMs_ == 0) {
    return kNoPendingActionDelayMs;
  }

  unsigned long delayMs = remainingUntil(nowMs, staConnectStartedAt_ + staConnectTimeoutMs_);
  if (reconnectBackoffIndex_ <
      static_cast<uint8_t>(sizeof(kReconnectBackoffScheduleMs) / sizeof(kReconnectBackoffScheduleMs[0]))) {
    delayMs =
        min(delayMs, remainingUntil(nowMs, staConnectStartedAt_ + kReconnectBackoffScheduleMs[reconnectBackoffIndex_]));
  }
  return delayMs;
}

void WifiProvisioning::startStaConnect(const bool forceDisconnect,
                                       const unsigned long timeoutMs,
                                       const bool fromCaptiveProbe) {
  if (!config_ || !hasStoredCredentials()) return;

  stopCaptiveMode();

  state_ = WIFI_STA_CONNECTING;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (forceDisconnect) {
    WiFi.disconnect(false, false);
    delay(50);
  }
  WiFi.begin(config_->wifiSsid.c_str(), config_->wifiPassword.c_str());
  staConnectStartedAt_ = millis();
  staConnectTimeoutMs_ = timeoutMs;
  reconnectBackoffIndex_ = 0;
  staProbeFromCaptive_ = fromCaptiveProbe;
  nextApRetryAt_ = 0;
  Serial.printf("WIFI PROV: %s SSID '%s'\n",
                fromCaptiveProbe ? "AP retry probe, zkousim" : "Pripojuji k",
                config_->wifiSsid.c_str());
}

bool WifiProvisioning::connectStaBlocking(unsigned long timeoutMs) {
  if (!config_ || !hasStoredCredentials()) return false;

  startStaConnect(true, timeoutMs, false);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && !timeReached(millis(), start + timeoutMs)) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    state_ = WIFI_STA_CONNECTED;
    staConnectStartedAt_ = 0;
    staConnectTimeoutMs_ = 0;
    reconnectBackoffIndex_ = 0;
    staProbeFromCaptive_ = false;
    nextApRetryAt_ = 0;
    applyStaPowerSave();
    Serial.printf("WIFI PROV: STA pripojeno, IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  return false;
}

void WifiProvisioning::startCaptiveMode(const bool scheduleRetry) {
  stopCaptiveMode();

  state_ = WIFI_AP_CAPTIVE;
  staConnectStartedAt_ = 0;
  staConnectTimeoutMs_ = 0;
  reconnectBackoffIndex_ = 0;
  staProbeFromCaptive_ = false;
  nextApRetryAt_ = scheduleRetry && hasStoredCredentials() ? millis() + kApRetryIntervalMs : 0;
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_AP);

  apSsid_ = buildApName();
  bool apOk = WiFi.softAP(apSsid_.c_str(), AP_PASSWORD);
  delay(50);
  apIp_ = WiFi.softAPIP();

  dnsServer_.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer_.start(DNS_PORT, "*", apIp_);
  captiveRunning_ = true;

  Serial.printf("WIFI PROV: AP %s (%s), open=%s, dns=%s\n",
                apSsid_.c_str(), apIp_.toString().c_str(),
                AP_PASSWORD[0] == '\0' ? "ano" : "ne",
                apOk ? "ok" : "fail");
  if (nextApRetryAt_ != 0) {
    Serial.printf("WIFI PROV: Dalsi STA probe za %lu min\n", kApRetryIntervalMs / 60000UL);
  }
}

void WifiProvisioning::stopCaptiveMode() {
  if (captiveRunning_) {
    dnsServer_.stop();
    captiveRunning_ = false;
  }

  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
    WiFi.softAPdisconnect(true);
  }
}

void WifiProvisioning::applyStaPowerSave() const {
  WiFi.setSleep(true);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
}

bool WifiProvisioning::hasStoredCredentials() const {
  return config_ && config_->wifiSsid.length() > 0;
}
