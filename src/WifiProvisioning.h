#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "config.h"

enum WifiModeState {
  WIFI_STA_CONNECTING = 0,
  WIFI_STA_CONNECTED,
  WIFI_AP_CAPTIVE,
};

class WifiProvisioning {
 public:
  void begin(AppConfig* config, unsigned long connectTimeoutMs = 20000UL);
  void process();

  bool reconnectWithoutSaving(String& statusMsg);
  bool saveCredentialsAndConnect(const String& ssid, const String& password, String& statusMsg);
  bool forgetCredentials();

  WifiModeState getState() const { return state_; }
  bool isCaptiveMode() const { return state_ == WIFI_AP_CAPTIVE; }
  String getStateText() const;
  String getApSsid() const { return apSsid_; }
  String getApIp() const { return apIp_.toString(); }
  unsigned long nextActionDelayMs(unsigned long nowMs) const;

 private:
  bool connectStaBlocking(unsigned long timeoutMs);
  void startStaConnect(bool forceDisconnect, unsigned long timeoutMs, bool fromCaptiveProbe = false);
  void startCaptiveMode(bool scheduleRetry);
  void stopCaptiveMode();
  void applyStaPowerSave() const;
  bool hasStoredCredentials() const;

  AppConfig* config_ = nullptr;
  DNSServer dnsServer_;
  WifiModeState state_ = WIFI_STA_CONNECTING;

  bool captiveRunning_ = false;
  IPAddress apIp_ = IPAddress(192, 168, 4, 1);
  String apSsid_;
  unsigned long connectTimeoutMs_ = 20000UL;

  unsigned long staConnectStartedAt_ = 0;
  unsigned long staConnectTimeoutMs_ = 0;
  unsigned long nextApRetryAt_ = 0;
  uint8_t reconnectBackoffIndex_ = 0;
  bool staProbeFromCaptive_ = false;
};
