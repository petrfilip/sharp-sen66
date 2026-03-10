#include "MqttSupport.h"

#include <Arduino.h>
#include <ArduinoJson.h>

namespace sharp::mqttsupport {

const char* const kTopicText = "sharp/display/text";
const char* const kTopicClear = "sharp/display/clear";
const char* const kTopicCommand = "sharp/display/command";
const char* const kTopicBrightness = "sharp/display/brightness";
const char* const kTopicStatus = "sharp/status";
const char* const kTopicSensor = "sharp/sensor";
const char* const kTopicTemp = "sharp/sensor/temperature";
const char* const kTopicHumidity = "sharp/sensor/humidity";
const char* const kTopicPm1 = "sharp/sensor/pm1";
const char* const kTopicPm25 = "sharp/sensor/pm25";
const char* const kTopicPm4 = "sharp/sensor/pm4";
const char* const kTopicPm10 = "sharp/sensor/pm10";
const char* const kTopicVoc = "sharp/sensor/voc";
const char* const kTopicNox = "sharp/sensor/nox";
const char* const kTopicCo2 = "sharp/sensor/co2";

void publishSensorData(PubSubClient& mqtt,
                       const SensorSnapshot& sensorData,
                       const unsigned long firstValidSensorAt,
                       const unsigned long mqttWarmupDelay,
                       const unsigned long nowMs) {
  if (!mqtt.connected() || !sensorData.valid) return;
  if (firstValidSensorAt == 0 || (nowMs - firstValidSensorAt) < mqttWarmupDelay) {
    Serial.println("MQTT: warmup delay aktivni, publikace preskocena");
    return;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f", sensorData.temperature);
  mqtt.publish(kTopicTemp, buf, true);

  snprintf(buf, sizeof(buf), "%.1f", sensorData.humidity);
  mqtt.publish(kTopicHumidity, buf, true);

  snprintf(buf, sizeof(buf), "%.1f", sensorData.pm1);
  mqtt.publish(kTopicPm1, buf, true);

  snprintf(buf, sizeof(buf), "%.1f", sensorData.pm25);
  mqtt.publish(kTopicPm25, buf, true);

  snprintf(buf, sizeof(buf), "%.1f", sensorData.pm4);
  mqtt.publish(kTopicPm4, buf, true);

  snprintf(buf, sizeof(buf), "%.1f", sensorData.pm10);
  mqtt.publish(kTopicPm10, buf, true);

  snprintf(buf, sizeof(buf), "%.0f", sensorData.voc);
  mqtt.publish(kTopicVoc, buf, true);

  snprintf(buf, sizeof(buf), "%.0f", sensorData.nox);
  mqtt.publish(kTopicNox, buf, true);

  snprintf(buf, sizeof(buf), "%u", sensorData.co2);
  mqtt.publish(kTopicCo2, buf, true);

  JsonDocument doc;
  doc["temperature"] = round(sensorData.temperature * 10) / 10.0;
  doc["humidity"] = round(sensorData.humidity * 10) / 10.0;
  doc["pm1"] = round(sensorData.pm1 * 10) / 10.0;
  doc["pm25"] = round(sensorData.pm25 * 10) / 10.0;
  doc["pm4"] = round(sensorData.pm4 * 10) / 10.0;
  doc["pm10"] = round(sensorData.pm10 * 10) / 10.0;
  doc["voc"] = round(sensorData.voc);
  doc["nox"] = round(sensorData.nox);
  doc["co2"] = sensorData.co2;
  doc["quality"] = airQualityLabel(sensorData.pm25);
  doc["uptime"] = nowMs / 1000;

  char jsonBuf[512];
  serializeJson(doc, jsonBuf, sizeof(jsonBuf));
  mqtt.publish(kTopicSensor, jsonBuf, true);

  Serial.println("MQTT: Sensor data published");
  Serial.printf("MQTT: payload JSON: %s\n", jsonBuf);
}

void publishHADiscovery(PubSubClient& mqtt) {
  struct HASensor {
    const char* name;
    const char* uid;
    const char* topic;
    const char* unit;
    const char* devClass;
    const char* icon;
  };

  const HASensor sensors[] = {
      {"Teplota", "sen66_temp", kTopicTemp, "C", "temperature", "mdi:thermometer"},
      {"Vlhkost", "sen66_humidity", kTopicHumidity, "%", "humidity", "mdi:water-percent"},
      {"PM1.0", "sen66_pm1", kTopicPm1, "ug/m3", "pm1", "mdi:blur"},
      {"PM2.5", "sen66_pm25", kTopicPm25, "ug/m3", "pm25", "mdi:blur"},
      {"PM4.0", "sen66_pm4", kTopicPm4, "ug/m3", nullptr, "mdi:blur-radial"},
      {"PM10", "sen66_pm10", kTopicPm10, "ug/m3", "pm10", "mdi:blur-radial"},
      {"VOC Index", "sen66_voc", kTopicVoc, "", nullptr, "mdi:air-filter"},
      {"NOx Index", "sen66_nox", kTopicNox, "", nullptr, "mdi:molecule"},
      {"CO2", "sen66_co2", kTopicCo2, "ppm", "carbon_dioxide", "mdi:molecule-co2"},
  };

  for (const auto& sensor : sensors) {
    JsonDocument doc;
    doc["name"] = sensor.name;
    doc["unique_id"] = sensor.uid;
    doc["state_topic"] = sensor.topic;
    doc["unit_of_measurement"] = sensor.unit;
    if (sensor.devClass) doc["device_class"] = sensor.devClass;
    if (sensor.icon) doc["icon"] = sensor.icon;
    doc["availability_topic"] = kTopicStatus;
    doc["payload_available"] = "online";
    doc["payload_not_available"] = "offline";

    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = "sharp_sen66_esp32c3";
    device["name"] = "Sharp SEN66 Displej";
    device["model"] = "ESP32-C3 + SEN66 + Sharp LCD";
    device["manufacturer"] = "DIY";
    device["sw_version"] = "2.0.0";

    char topic[128];
    snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/config", sensor.uid);

    char payload[512];
    serializeJson(doc, payload, sizeof(payload));
    mqtt.publish(topic, payload, true);

    Serial.printf("HA Discovery: %s\n", sensor.name);
    delay(50);
  }

  Serial.println("HA Discovery: Hotovo!");
}

bool reconnect(PubSubClient& mqtt, const AppConfig& config) {
  Serial.print("MQTT: Pripojuji...");

  if (mqtt.connect(config.mqttClientId.c_str(), config.mqttUser.c_str(), config.mqttPassword.c_str(),
                   kTopicStatus, 0, true, "offline")) {
    Serial.println("OK!");
    mqtt.publish(kTopicStatus, "online", true);
    mqtt.subscribe(kTopicText);
    mqtt.subscribe(kTopicClear);
    mqtt.subscribe(kTopicCommand);
    mqtt.subscribe(kTopicBrightness);
    publishHADiscovery(mqtt);
    return true;
  }

  Serial.printf("CHYBA rc=%d\n", mqtt.state());
  return false;
}

}  // namespace sharp::mqttsupport
