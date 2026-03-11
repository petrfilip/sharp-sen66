#include "SensorService.h"

#include <Arduino.h>

namespace sharp {

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

SensorService::SensorService(TwoWire& wire, SensirionI2cSen66& sensor, const uint8_t sdaPin, const uint8_t sclPin)
    : wire_(wire), sensor_(sensor), sdaPin_(sdaPin), sclPin_(sclPin) {}

void SensorService::begin() {
  Serial.println("SEN66: Inicializace I2C...");
  wire_.begin(sdaPin_, sclPin_);
  sensor_.begin(wire_, SEN66_I2C_ADDR_6B);

  int16_t error = sensor_.deviceReset();
  if (error != NO_ERROR) {
    char msg[64];
    errorToString(error, msg, sizeof(msg));
    Serial.printf("SEN66: deviceReset() CHYBA: %s\n", msg);
    ready_ = false;
    return;
  }

  delay(1200);

  int8_t serialNumber[32] = {0};
  error = sensor_.getSerialNumber(serialNumber, 32);
  if (error != NO_ERROR) {
    char msg[64];
    errorToString(error, msg, sizeof(msg));
    Serial.printf("SEN66: getSerialNumber() CHYBA: %s\n", msg);
  } else {
    Serial.printf("SEN66: S/N: %s\n", reinterpret_cast<const char*>(serialNumber));
  }

  error = sensor_.startContinuousMeasurement();
  if (error != NO_ERROR) {
    char msg[64];
    errorToString(error, msg, sizeof(msg));
    Serial.printf("SEN66: startContinuousMeasurement() CHYBA: %s\n", msg);
    ready_ = false;
    return;
  }

  ready_ = true;
  Serial.println("SEN66: OK, mereni spusteno!");
}

bool SensorService::read(const AppConfig& config, SensorSnapshot& outData) {
  if (!ready_) {
    return false;
  }

  float pm1 = 0.0f;
  float pm25 = 0.0f;
  float pm4 = 0.0f;
  float pm10 = 0.0f;
  float hum = 0.0f;
  float temp = 0.0f;
  float voc = 0.0f;
  float nox = 0.0f;
  uint16_t co2 = 0;

  const int16_t error = sensor_.readMeasuredValues(pm1, pm25, pm4, pm10, hum, temp, voc, nox, co2);
  if (error != NO_ERROR) {
    char msg[64];
    errorToString(error, msg, sizeof(msg));
    Serial.printf("SEN66: readMeasuredValues() CHYBA: %s\n", msg);
    return false;
  }

  if (!valuesLookValid(pm1, pm25, pm4, pm10, hum, temp, voc, nox, co2)) {
    Serial.println("SEN66: namerena neplatna data, preskakuji");
    return false;
  }

  outData.pm1 = pm1;
  outData.pm25 = pm25;
  outData.pm4 = pm4;
  outData.pm10 = pm10;
  const float adjustedTemperature = temp + config.temperatureOffset;
  if (!isfinite(adjustedTemperature)) {
    Serial.println("SEN66: upravena teplota neni konecna, preskakuji");
    return false;
  }
  outData.temperature = adjustedTemperature;
  outData.humidity = hum;
  outData.voc = voc;
  outData.nox = nox;
  outData.co2 = co2;
  outData.valid = true;

  Serial.printf("SEN66: T(raw)=%.1f T(adj)=%.1f H=%.1f PM2.5=%.1f VOC=%.0f NOx=%.0f CO2=%u\n",
                temp, outData.temperature, hum, pm25, voc, nox, co2);
  return true;
}

bool SensorService::valuesLookValid(const float pm1,
                                    const float pm25,
                                    const float pm4,
                                    const float pm10,
                                    const float hum,
                                    const float temp,
                                    const float voc,
                                    const float nox,
                                    const uint16_t co2) const {
  if (isnan(pm1) || isnan(pm25) || isnan(pm4) || isnan(pm10) || isnan(hum) || isnan(temp) ||
      isnan(voc) || isnan(nox)) {
    return false;
  }

  if (temp < -40.0f || temp > 85.0f) return false;
  if (hum < 0.0f || hum > 100.0f) return false;
  if (pm1 < 0.0f || pm1 > 1000.0f) return false;
  if (pm25 < 0.0f || pm25 > 1000.0f) return false;
  if (pm4 < 0.0f || pm4 > 1000.0f) return false;
  if (pm10 < 0.0f || pm10 > 1000.0f) return false;
  if (voc < 0.0f || voc > 500.0f) return false;
  if (nox < 0.0f || nox > 500.0f) return false;
  if (co2 < 350 || co2 > 10000) return false;
  return true;
}

}  // namespace sharp
