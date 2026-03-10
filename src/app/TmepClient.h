#pragma once

#include "AppTypes.h"
#include "config.h"

namespace sharp {

class TmepClient {
 public:
  String buildRequestUrl(const AppConfig& config, const SensorSnapshot& sensorData) const;
  bool sendRequest(const AppConfig& config,
                   const SensorSnapshot& sensorData,
                   String& lastStatus,
                   bool manualTrigger) const;

 private:
  static String formatFloat1(float value);
  static String formatFloat0(float value);
  static void replaceAllTokens(String& target, const String& token, const String& value);
  String buildQueryParams(const AppConfig& config, const SensorSnapshot& sensorData) const;
};

}  // namespace sharp
