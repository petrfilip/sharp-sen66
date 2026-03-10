#include "TmepClient.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

namespace sharp {

String TmepClient::buildRequestUrl(const AppConfig& config, const SensorSnapshot& sensorData) const {
  if (config.tmepDomain.length() == 0 || config.tmepParams.length() == 0 || !sensorData.valid) {
    return "";
  }
  return "http://" + config.tmepDomain + ".tmep.cz/?" + buildQueryParams(config, sensorData);
}

bool TmepClient::sendRequest(const AppConfig& config,
                             const SensorSnapshot& sensorData,
                             String& lastStatus,
                             const bool manualTrigger) const {
  if (config.tmepDomain.length() == 0 || config.tmepParams.length() == 0) {
    Serial.println("TMEP: domena nebo parametry nejsou nastaveny, request preskocen");
    lastStatus = "TMEP:SKIP";
    return false;
  }
  if (!sensorData.valid) {
    Serial.println("TMEP: nejsou validni data senzoru, request preskocen");
    lastStatus = "TMEP:SKIP";
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("TMEP: WiFi neni pripojena, request preskocen");
    lastStatus = "TMEP:SKIP";
    return false;
  }

  const String url = buildRequestUrl(config, sensorData);
  if (url.length() == 0) {
    lastStatus = "TMEP:SKIP";
    return false;
  }

  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(url)) {
    Serial.println("TMEP: Nelze inicializovat HTTP request");
    lastStatus = "TMEP:ERR";
    return false;
  }

  const int httpCode = http.GET();
  const String response = http.getString();
  http.end();

  if (httpCode > 0 && httpCode < 400) {
    Serial.printf("TMEP: %srequest OK, HTTP %d, URL: %s\n", manualTrigger ? "manual " : "", httpCode, url.c_str());
    lastStatus = "TMEP:OK";
    return true;
  }

  Serial.printf("TMEP: %srequest CHYBA, HTTP %d, URL: %s, body: %s\n",
                manualTrigger ? "manual " : "", httpCode, url.c_str(), response.c_str());
  lastStatus = "TMEP:ERR";
  return false;
}

String TmepClient::formatFloat1(const float value) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", value);
  return String(buf);
}

String TmepClient::formatFloat0(const float value) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f", value);
  return String(buf);
}

void TmepClient::replaceAllTokens(String& target, const String& token, const String& value) {
  target.replace("*" + token + "*", value);
  target.replace("{" + token + "}", value);
}

String TmepClient::buildQueryParams(const AppConfig& config, const SensorSnapshot& sensorData) const {
  String params = config.tmepParams;
  replaceAllTokens(params, "TEMP", formatFloat1(sensorData.temperature));
  replaceAllTokens(params, "HUM", formatFloat1(sensorData.humidity));
  replaceAllTokens(params, "PM1", formatFloat1(sensorData.pm1));
  replaceAllTokens(params, "PM2", formatFloat1(sensorData.pm25));
  replaceAllTokens(params, "PM4", formatFloat1(sensorData.pm4));
  replaceAllTokens(params, "PM10", formatFloat1(sensorData.pm10));
  replaceAllTokens(params, "VOC", formatFloat0(sensorData.voc));
  replaceAllTokens(params, "NOX", formatFloat0(sensorData.nox));
  replaceAllTokens(params, "CO2", String(sensorData.co2));
  return params;
}

}  // namespace sharp
