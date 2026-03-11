#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "AppTypes.h"
#include "config.h"

namespace sharp {

class TmepClient {
 public:
  struct RequestResult {
    bool accepted = false;
    int statusCode = 500;
    String message;
  };

  void begin();
  String buildRequestUrl(const AppConfig& config, const SensorSnapshot& sensorData) const;
  RequestResult queueRequest(const AppConfig& config,
                             const SensorSnapshot& sensorData,
                             bool manualTrigger);
  String lastStatus() const;

 private:
  struct PendingRequest {
    String url;
    bool manualTrigger = false;
  };

  static void workerTaskThunk(void* context);
  void workerTask();
  void setLastStatus(const String& status);
  static String formatFloat1(float value);
  static String formatFloat0(float value);
  static void replaceAllTokens(String& target, const String& token, const String& value);
  String buildQueryParams(const AppConfig& config, const SensorSnapshot& sensorData) const;

  mutable SemaphoreHandle_t mutex_ = nullptr;
  TaskHandle_t workerTask_ = nullptr;
  PendingRequest pendingRequest_;
  bool requestPending_ = false;
  bool requestInFlight_ = false;
  String lastStatus_ = "TMEP:---";
};

}  // namespace sharp
