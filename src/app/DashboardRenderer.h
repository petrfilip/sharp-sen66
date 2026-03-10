#pragma once

#include <Adafruit_SharpMem.h>

#include "AppTypes.h"

namespace sharp {

struct DashboardRenderInfo {
  String wifiText;
  String tmepText;
  bool mqttConnected = false;
  bool sen66Ready = false;
  String uptimeText;
  String dateTimeText;
};

class DashboardRenderer {
 public:
  explicit DashboardRenderer(Adafruit_SharpMem& display);

  void render(const SensorSnapshot& sensorData, const DashboardRenderInfo& info, bool forceFullRedraw = false);
  void renderSplash(const String& wifiSsid, const String& mqttServer);

 private:
  void drawHeader(const DashboardRenderInfo& info) const;
  void drawStaticWaitingLayout() const;
  void drawStaticValidLayout() const;
  void drawDynamicValidData(const SensorSnapshot& sensorData) const;
  void clearHeaderRegion() const;
  void clearDynamicValidRegions() const;
  void drawCenteredText(const char* text, int y, int textSize) const;
  void drawRightAlignedText(const char* text, int y, int textSize) const;
  void drawDividerLine(int y) const;
  void drawThermIcon(int x, int y) const;
  void drawDropIcon(int x, int y) const;

  Adafruit_SharpMem& display_;
  bool layoutInitialized_ = false;
  bool lastLayoutWasValid_ = false;
};

}  // namespace sharp
