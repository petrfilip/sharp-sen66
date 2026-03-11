#include "TmepClient.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

namespace sharp {

namespace {

constexpr uint32_t kWorkerStackSize = 6144;
constexpr UBaseType_t kWorkerPriority = 1;
constexpr TickType_t kMutexTimeoutTicks = pdMS_TO_TICKS(250);

}  // namespace

void TmepClient::begin() {
  if (mutex_ == nullptr) {
    mutex_ = xSemaphoreCreateMutex();
  }

  if (mutex_ == nullptr) {
    lastStatus_ = "TMEP:ERR";
    Serial.println("TMEP: nepodarilo se vytvorit mutex");
    return;
  }

  if (workerTask_ != nullptr) {
    return;
  }

  if (xTaskCreate(workerTaskThunk, "tmep_http", kWorkerStackSize, this, kWorkerPriority, &workerTask_) != pdPASS) {
    workerTask_ = nullptr;
    setLastStatus("TMEP:ERR");
    Serial.println("TMEP: nepodarilo se spustit worker task");
  }
}

String TmepClient::buildRequestUrl(const AppConfig& config, const SensorSnapshot& sensorData) const {
  if (config.tmepDomain.length() == 0 || config.tmepParams.length() == 0 || !sensorData.valid) {
    return "";
  }
  return "http://" + config.tmepDomain + ".tmep.cz/?" + buildQueryParams(config, sensorData);
}

TmepClient::RequestResult TmepClient::queueRequest(const AppConfig& config,
                                                   const SensorSnapshot& sensorData,
                                                   const bool manualTrigger) {
  RequestResult result;

  if (config.tmepDomain.length() == 0 || config.tmepParams.length() == 0) {
    Serial.println("TMEP: domena nebo parametry nejsou nastaveny, request preskocen");
    setLastStatus("TMEP:SKIP");
    result.statusCode = 400;
    result.message = "TMEP domena nebo parametry nejsou nastaveny";
    return result;
  }
  if (!sensorData.valid) {
    Serial.println("TMEP: nejsou validni data senzoru, request preskocen");
    setLastStatus("TMEP:SKIP");
    result.statusCode = 400;
    result.message = "TMEP nema validni data senzoru";
    return result;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("TMEP: WiFi neni pripojena, request preskocen");
    setLastStatus("TMEP:SKIP");
    result.statusCode = 503;
    result.message = "TMEP request nelze odeslat bez Wi-Fi";
    return result;
  }

  const String url = buildRequestUrl(config, sensorData);
  if (url.length() == 0) {
    setLastStatus("TMEP:SKIP");
    result.statusCode = 400;
    result.message = "TMEP URL nelze sestavit";
    return result;
  }

  if (mutex_ == nullptr || workerTask_ == nullptr) {
    setLastStatus("TMEP:ERR");
    result.statusCode = 500;
    result.message = "TMEP worker neni inicializovan";
    return result;
  }

  if (xSemaphoreTake(mutex_, kMutexTimeoutTicks) != pdTRUE) {
    setLastStatus("TMEP:ERR");
    result.statusCode = 500;
    result.message = "TMEP worker mutex timeout";
    return result;
  }

  if (requestPending_ || requestInFlight_) {
    const String status = lastStatus_;
    xSemaphoreGive(mutex_);
    result.statusCode = 409;
    result.message = status == "TMEP:QUEUED" ? "TMEP request uz je ve fronte" : "TMEP request uz bezi";
    return result;
  }

  pendingRequest_.url = url;
  pendingRequest_.manualTrigger = manualTrigger;
  requestPending_ = true;
  lastStatus_ = "TMEP:QUEUED";
  TaskHandle_t workerTask = workerTask_;
  xSemaphoreGive(mutex_);

  xTaskNotifyGive(workerTask);
  result.accepted = true;
  result.statusCode = 202;
  result.message = "TMEP request byl zarazen do fronty";
  return result;
}

String TmepClient::lastStatus() const {
  if (mutex_ == nullptr) {
    return lastStatus_;
  }

  if (xSemaphoreTake(mutex_, kMutexTimeoutTicks) != pdTRUE) {
    return "TMEP:ERR";
  }

  const String status = lastStatus_;
  xSemaphoreGive(mutex_);
  return status;
}

void TmepClient::workerTaskThunk(void* context) {
  static_cast<TmepClient*>(context)->workerTask();
}

void TmepClient::workerTask() {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    PendingRequest request;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    if (!requestPending_) {
      xSemaphoreGive(mutex_);
      continue;
    }

    request = pendingRequest_;
    requestPending_ = false;
    requestInFlight_ = true;
    lastStatus_ = "TMEP:BUSY";
    xSemaphoreGive(mutex_);

    HTTPClient http;
    http.setTimeout(5000);
    int httpCode = 0;
    String response;
    bool ok = false;

    if (!http.begin(request.url)) {
      Serial.println("TMEP: Nelze inicializovat HTTP request");
    } else {
      httpCode = http.GET();
      response = http.getString();
      http.end();
      ok = httpCode > 0 && httpCode < 400;
    }

    if (ok) {
      Serial.printf("TMEP: %srequest OK, HTTP %d, URL: %s\n",
                    request.manualTrigger ? "manual " : "", httpCode, request.url.c_str());
    } else {
      Serial.printf("TMEP: %srequest CHYBA, HTTP %d, URL: %s, body: %s\n",
                    request.manualTrigger ? "manual " : "", httpCode, request.url.c_str(), response.c_str());
    }

    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
      requestInFlight_ = false;
      lastStatus_ = ok ? "TMEP:OK" : "TMEP:ERR";
      xSemaphoreGive(mutex_);
    }
  }
}

void TmepClient::setLastStatus(const String& status) {
  if (mutex_ == nullptr) {
    lastStatus_ = status;
    return;
  }

  if (xSemaphoreTake(mutex_, kMutexTimeoutTicks) != pdTRUE) {
    return;
  }

  lastStatus_ = status;
  xSemaphoreGive(mutex_);
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
