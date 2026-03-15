#include "config.h"

#include <Preferences.h>
#include <climits>
#include <cmath>

#include "metrics.h"

namespace {
constexpr const char* NS = "appcfg";

String missingStringSentinel(const String& value) {
  return value == "\x1D" ? "\x1E" : "\x1D";
}

bool putStringChecked(Preferences& pref, const char* key, const String& value) {
  pref.putString(key, value);
  return pref.getString(key, missingStringSentinel(value)) == value;
}

bool putIntChecked(Preferences& pref, const char* key, const int value) {
  pref.putInt(key, value);
  const int sentinel = value == INT_MIN ? INT_MAX : INT_MIN;
  return pref.getInt(key, sentinel) == value;
}

bool putULongChecked(Preferences& pref, const char* key, const unsigned long value) {
  pref.putULong(key, value);
  const unsigned long sentinel = value == ULONG_MAX ? 0UL : ULONG_MAX;
  return pref.getULong(key, sentinel) == value;
}

bool putFloatChecked(Preferences& pref, const char* key, const float value) {
  pref.putFloat(key, value);
  const float stored = pref.getFloat(key, NAN);
  return isfinite(stored) && stored == value;
}

bool putBoolChecked(Preferences& pref, const char* key, const bool value) {
  pref.putBool(key, value);
  return pref.getBool(key, !value) == value;
}

bool putUCharChecked(Preferences& pref, const char* key, const uint8_t value) {
  pref.putUChar(key, value);
  const uint8_t sentinel = value == 0xFF ? 0xFE : 0xFF;
  return pref.getUChar(key, sentinel) == value;
}

void sanitize(AppConfig& cfg) {
  if (cfg.mqttPort < 1 || cfg.mqttPort > 65535) cfg.mqttPort = 1883;
  if (cfg.displayRotation > 3) cfg.displayRotation = 2;
  if (cfg.displayRefreshInterval < 500) cfg.displayRefreshInterval = 2000;
  if (cfg.displayCycleInterval < 2000) cfg.displayCycleInterval = 15000;
  if (cfg.mqttPublishInterval < 1000) cfg.mqttPublishInterval = 10000;
  if (cfg.tmepRequestInterval < 1000) cfg.tmepRequestInterval = 60000;
  if (cfg.mqttWarmupDelay < 1000) cfg.mqttWarmupDelay = 60000;
  if (!isfinite(cfg.temperatureOffset)) cfg.temperatureOffset = -2.0f;
  if (cfg.displayMode > 1) cfg.displayMode = 0;
  if (cfg.displayScreen > 1) cfg.displayScreen = 0;
  if (!airmon::isValidMetricIdValue(cfg.displayGraphMetric)) cfg.displayGraphMetric = 0;
  if (cfg.displayGraphRange > 1) cfg.displayGraphRange = 0;
}
}  // namespace

bool validateConfig(const AppConfig& cfg) {
  if (cfg.mqttPort < 1 || cfg.mqttPort > 65535) return false;
  if (!isfinite(cfg.temperatureOffset)) return false;
  if (cfg.displayRotation > 3) return false;
  if (cfg.displayRefreshInterval < 500) return false;
  if (cfg.displayCycleInterval < 2000) return false;
  if (cfg.mqttPublishInterval < 1000) return false;
  if (cfg.tmepRequestInterval < 1000) return false;
  if (cfg.mqttWarmupDelay < 1000) return false;
  if (cfg.displayMode > 1) return false;
  if (cfg.displayScreen > 1) return false;
  if (!airmon::isValidMetricIdValue(cfg.displayGraphMetric)) return false;
  if (cfg.displayGraphRange > 1) return false;
  return true;
}

bool loadConfig(AppConfig& config) {
  Preferences pref;
  if (!pref.begin(NS, true)) return false;

  config.wifiSsid = pref.getString("wifi_ssid", config.wifiSsid);
  config.wifiPassword = pref.getString("wifi_pass", config.wifiPassword);

  config.mqttServer = pref.getString("mqtt_server", config.mqttServer);
  config.mqttPort = pref.getInt("mqtt_port", config.mqttPort);
  config.mqttUser = pref.getString("mqtt_user", config.mqttUser);
  config.mqttPassword = pref.getString("mqtt_pass", config.mqttPassword);
  config.mqttClientId = pref.getString("mqtt_client", config.mqttClientId);

  config.tmepDomain = pref.getString("tmep_domain", config.tmepDomain);
  config.tmepParams = pref.getString("tmep_params", config.tmepParams);

  config.mqttPublishInterval = pref.getULong("mqtt_pub_ms", config.mqttPublishInterval);
  config.tmepRequestInterval = pref.getULong("tmep_req_ms", config.tmepRequestInterval);
  config.displayRefreshInterval = pref.getULong("disp_ref_ms", config.displayRefreshInterval);
  config.displayCycleInterval = pref.getULong("disp_cycle_ms", config.displayCycleInterval);
  config.mqttWarmupDelay = pref.getULong("mqtt_warmup", config.mqttWarmupDelay);

  config.tmepBaseUrl = pref.getString("tmep_base", config.tmepBaseUrl);
  config.temperatureOffset = pref.getFloat("temp_offset", config.temperatureOffset);

  config.displayRotation = pref.getUChar("disp_rot", config.displayRotation);
  config.displayInvertRequested = pref.getBool("disp_inv", config.displayInvertRequested);
  config.displayMode = pref.getUChar("disp_mode", config.displayMode);
  config.displayScreen = pref.getUChar("disp_screen", config.displayScreen);
  config.displayGraphMetric = pref.getUChar("disp_metric", config.displayGraphMetric);
  config.displayGraphRange = pref.getUChar("disp_range", config.displayGraphRange);

  pref.end();
  sanitize(config);
  return true;
}

bool saveConfig(const AppConfig& config) {
  if (!validateConfig(config)) return false;

  Preferences pref;
  if (!pref.begin(NS, false)) return false;

  bool ok = true;

  ok = putStringChecked(pref, "wifi_ssid", config.wifiSsid) && ok;
  ok = putStringChecked(pref, "wifi_pass", config.wifiPassword) && ok;

  ok = putStringChecked(pref, "mqtt_server", config.mqttServer) && ok;
  ok = putIntChecked(pref, "mqtt_port", config.mqttPort) && ok;
  ok = putStringChecked(pref, "mqtt_user", config.mqttUser) && ok;
  ok = putStringChecked(pref, "mqtt_pass", config.mqttPassword) && ok;
  ok = putStringChecked(pref, "mqtt_client", config.mqttClientId) && ok;

  ok = putStringChecked(pref, "tmep_domain", config.tmepDomain) && ok;
  ok = putStringChecked(pref, "tmep_params", config.tmepParams) && ok;

  ok = putULongChecked(pref, "mqtt_pub_ms", config.mqttPublishInterval) && ok;
  ok = putULongChecked(pref, "tmep_req_ms", config.tmepRequestInterval) && ok;
  ok = putULongChecked(pref, "disp_ref_ms", config.displayRefreshInterval) && ok;
  ok = putULongChecked(pref, "disp_cycle_ms", config.displayCycleInterval) && ok;
  ok = putULongChecked(pref, "mqtt_warmup", config.mqttWarmupDelay) && ok;

  ok = putStringChecked(pref, "tmep_base", config.tmepBaseUrl) && ok;
  ok = putFloatChecked(pref, "temp_offset", config.temperatureOffset) && ok;

  ok = putUCharChecked(pref, "disp_rot", config.displayRotation) && ok;
  ok = putBoolChecked(pref, "disp_inv", config.displayInvertRequested) && ok;
  ok = putUCharChecked(pref, "disp_mode", config.displayMode) && ok;
  ok = putUCharChecked(pref, "disp_screen", config.displayScreen) && ok;
  ok = putUCharChecked(pref, "disp_metric", config.displayGraphMetric) && ok;
  ok = putUCharChecked(pref, "disp_range", config.displayGraphRange) && ok;

  pref.end();
  return ok;
}
