#include "DashboardRenderer.h"

#include <Arduino.h>

namespace sharp {

namespace {

constexpr uint16_t kBlack = 0;
constexpr uint16_t kWhite = 1;
constexpr uint16_t kDisplayWidth = 400;

}  // namespace

DashboardRenderer::DashboardRenderer(Adafruit_SharpMem& display) : display_(display) {}

void DashboardRenderer::render(const SensorSnapshot& sensorData,
                               const DashboardRenderInfo& info,
                               const bool forceFullRedraw) {
  display_.setTextColor(kBlack);

  const bool redrawBase = forceFullRedraw || !layoutInitialized_ || lastLayoutWasValid_ != sensorData.valid;
  if (redrawBase) {
    display_.clearDisplay();
    if (sensorData.valid) {
      drawStaticValidLayout();
    } else {
      drawStaticWaitingLayout();
    }
  } else {
    clearHeaderRegion();
    if (sensorData.valid) {
      clearDynamicValidRegions();
    }
  }

  drawHeader(info);
  if (sensorData.valid) {
    drawDynamicValidData(sensorData);
  }

  display_.refresh();
  layoutInitialized_ = true;
  lastLayoutWasValid_ = sensorData.valid;
}

void DashboardRenderer::drawHeader(const DashboardRenderInfo& info) const {
  display_.setTextSize(1);

  display_.setCursor(5, 2);
  display_.print(info.wifiText);

  display_.setCursor(165, 2);
  display_.print(info.tmepText);

  display_.setCursor(245, 2);
  display_.print(info.mqttConnected ? "MQTT:OK" : "MQTT:---");

  display_.setCursor(325, 2);
  display_.print(info.sen66Ready ? "SEN66:OK" : "SEN66:---");

  display_.setCursor(5, 10);
  display_.print(info.uptimeText);

  drawRightAlignedText(info.dateTimeText.c_str(), 10, 1);
  drawDividerLine(20);
}

void DashboardRenderer::drawStaticWaitingLayout() const {
  drawCenteredText("Cekam na data", 80, 2);
  drawCenteredText("ze senzoru SEN66...", 110, 2);
}

void DashboardRenderer::drawStaticValidLayout() const {
  drawThermIcon(15, 28);
  drawDropIcon(220, 28);
  drawDividerLine(68);

  display_.setTextSize(1);
  display_.setCursor(15, 74);
  display_.print("PM1.0");
  display_.setCursor(115, 74);
  display_.print("PM2.5");
  display_.setCursor(215, 74);
  display_.print("PM4.0");
  display_.setCursor(315, 74);
  display_.print("PM10");

  display_.setCursor(15, 118);
  display_.print("ug/m3");
  display_.setCursor(115, 118);
  display_.print("ug/m3");
  display_.setCursor(215, 118);
  display_.print("ug/m3");
  display_.setCursor(315, 118);
  display_.print("ug/m3");

  drawDividerLine(132);

  display_.setCursor(15, 138);
  display_.print("VOC Index");
  display_.setCursor(155, 138);
  display_.print("NOx Index");
  display_.setCursor(295, 138);
  display_.print("CO2");

  display_.setCursor(350, 170);
  display_.print("ppm");

  drawDividerLine(185);

  display_.setCursor(15, 192);
  display_.print("Kvalita vzduchu:");

  display_.drawRect(270, 200, 122, 24, kBlack);
}

void DashboardRenderer::drawDynamicValidData(const SensorSnapshot& sensorData) const {
  char buf[64];

  snprintf(buf, sizeof(buf), "%.1f", sensorData.temperature);
  display_.setTextSize(4);
  display_.setCursor(35, 25);
  display_.print(buf);

  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  display_.getTextBounds(buf, 35, 25, &x1, &y1, &width, &height);
  display_.setTextSize(2);
  display_.setCursor(35 + static_cast<int>(width) + 5, 25);
  display_.print("o");
  display_.setCursor(35 + static_cast<int>(width) + 5, 40);
  display_.print("C");

  snprintf(buf, sizeof(buf), "%.1f", sensorData.humidity);
  display_.setTextSize(4);
  display_.setCursor(240, 25);
  display_.print(buf);
  display_.getTextBounds(buf, 240, 25, &x1, &y1, &width, &height);
  display_.setTextSize(2);
  display_.setCursor(240 + static_cast<int>(width) + 5, 30);
  display_.print("%");

  display_.setTextSize(3);
  snprintf(buf, sizeof(buf), "%.0f", sensorData.pm1);
  display_.setCursor(10, 90);
  display_.print(buf);

  snprintf(buf, sizeof(buf), "%.0f", sensorData.pm25);
  display_.setCursor(110, 90);
  display_.print(buf);

  snprintf(buf, sizeof(buf), "%.0f", sensorData.pm4);
  display_.setCursor(210, 90);
  display_.print(buf);

  snprintf(buf, sizeof(buf), "%.0f", sensorData.pm10);
  display_.setCursor(310, 90);
  display_.print(buf);

  display_.setTextSize(3);
  snprintf(buf, sizeof(buf), "%.0f", sensorData.voc);
  display_.setCursor(15, 152);
  display_.print(buf);

  snprintf(buf, sizeof(buf), "%.0f", sensorData.nox);
  display_.setCursor(155, 152);
  display_.print(buf);

  snprintf(buf, sizeof(buf), "%u", sensorData.co2);
  display_.setCursor(280, 152);
  display_.print(buf);

  display_.setTextSize(1);
  display_.setCursor(350, 170);
  display_.print("ppm");

  const char* quality = airQualityLabel(sensorData.pm25);
  display_.setTextSize(3);
  display_.setCursor(15, 208);
  display_.print(quality);

  const float barValue = min(sensorData.pm25 / 150.0f, 1.0f);
  const int barWidth = static_cast<int>(barValue * 120);
  display_.fillRect(271, 201, barWidth, 22, kBlack);
}

void DashboardRenderer::renderSplash(const String& wifiSsid, const String& mqttServer) {
  layoutInitialized_ = false;
  display_.clearDisplay();
  display_.setTextColor(kBlack);

  drawCenteredText("Sharp LCD + SEN66", 40, 3);
  drawCenteredText("MQTT Dashboard", 80, 2);
  drawDividerLine(110);

  display_.setTextSize(1);
  display_.setCursor(30, 125);
  display_.print("WiFi: ");
  display_.print(wifiSsid);

  display_.setCursor(30, 140);
  display_.print("MQTT: ");
  display_.print(mqttServer);

  display_.setCursor(30, 160);
  display_.print("Inicializace...");
  display_.refresh();
}

void DashboardRenderer::clearHeaderRegion() const {
  display_.fillRect(0, 0, kDisplayWidth, 20, kWhite);
}

void DashboardRenderer::clearDynamicValidRegions() const {
  display_.fillRect(35, 25, 155, 35, kWhite);
  display_.fillRect(240, 25, 145, 35, kWhite);

  display_.fillRect(10, 90, 80, 24, kWhite);
  display_.fillRect(110, 90, 80, 24, kWhite);
  display_.fillRect(210, 90, 80, 24, kWhite);
  display_.fillRect(310, 90, 80, 24, kWhite);

  display_.fillRect(15, 152, 90, 24, kWhite);
  display_.fillRect(155, 152, 90, 24, kWhite);
  display_.fillRect(280, 148, 115, 30, kWhite);

  display_.fillRect(15, 208, 240, 24, kWhite);
  display_.fillRect(271, 201, 120, 22, kWhite);
}

void DashboardRenderer::drawCenteredText(const char* text, const int y, const int textSize) const {
  display_.setTextSize(textSize);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  display_.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  display_.setCursor((kDisplayWidth - static_cast<int>(width)) / 2, y);
  display_.print(text);
}

void DashboardRenderer::drawRightAlignedText(const char* text, const int y, const int textSize) const {
  display_.setTextSize(textSize);
  int16_t x1 = 0;
  int16_t y1 = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  display_.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  display_.setCursor(kDisplayWidth - static_cast<int>(width) - 5, y);
  display_.print(text);
}

void DashboardRenderer::drawDividerLine(const int y) const {
  display_.drawLine(5, y, kDisplayWidth - 5, y, kBlack);
}

void DashboardRenderer::drawThermIcon(const int x, const int y) const {
  display_.drawCircle(x + 3, y + 12, 4, kBlack);
  display_.drawRect(x + 1, y, 5, 12, kBlack);
  display_.fillCircle(x + 3, y + 12, 3, kBlack);
}

void DashboardRenderer::drawDropIcon(const int x, const int y) const {
  display_.drawPixel(x + 3, y, kBlack);
  display_.drawLine(x + 2, y + 1, x + 4, y + 1, kBlack);
  display_.drawLine(x + 1, y + 2, x + 5, y + 2, kBlack);
  display_.drawLine(x, y + 3, x + 6, y + 3, kBlack);
  display_.drawLine(x, y + 4, x + 6, y + 4, kBlack);
  display_.drawLine(x, y + 5, x + 6, y + 5, kBlack);
  display_.drawLine(x + 1, y + 6, x + 5, y + 6, kBlack);
  display_.drawLine(x + 2, y + 7, x + 4, y + 7, kBlack);
}

}  // namespace sharp
