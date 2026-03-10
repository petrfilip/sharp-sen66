#pragma once

#include <PubSubClient.h>

#include "AppTypes.h"
#include "config.h"

namespace sharp::mqttsupport {

extern const char* const kTopicText;
extern const char* const kTopicClear;
extern const char* const kTopicCommand;
extern const char* const kTopicBrightness;
extern const char* const kTopicStatus;
extern const char* const kTopicSensor;
extern const char* const kTopicTemp;
extern const char* const kTopicHumidity;
extern const char* const kTopicPm1;
extern const char* const kTopicPm25;
extern const char* const kTopicPm4;
extern const char* const kTopicPm10;
extern const char* const kTopicVoc;
extern const char* const kTopicNox;
extern const char* const kTopicCo2;

void publishSensorData(PubSubClient& mqtt,
                       const SensorSnapshot& sensorData,
                       unsigned long firstValidSensorAt,
                       unsigned long mqttWarmupDelay,
                       unsigned long nowMs);

void publishHADiscovery(PubSubClient& mqtt);
bool reconnect(PubSubClient& mqtt, const AppConfig& config);

}  // namespace sharp::mqttsupport
