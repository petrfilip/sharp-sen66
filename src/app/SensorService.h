#pragma once

#include <Wire.h>
#include <SensirionI2cSen66.h>

#include "AppTypes.h"
#include "config.h"

namespace sharp {

class SensorService {
 public:
  SensorService(TwoWire& wire, SensirionI2cSen66& sensor, uint8_t sdaPin, uint8_t sclPin);

  void begin();
  bool isReady() const { return ready_; }
  bool read(const AppConfig& config, SensorSnapshot& outData);

 private:
  bool valuesLookValid(float pm1,
                       float pm25,
                       float pm4,
                       float pm10,
                       float hum,
                       float temp,
                       float voc,
                       float nox,
                       uint16_t co2) const;

  TwoWire& wire_;
  SensirionI2cSen66& sensor_;
  uint8_t sdaPin_ = 0;
  uint8_t sclPin_ = 0;
  bool ready_ = false;
};

}  // namespace sharp
