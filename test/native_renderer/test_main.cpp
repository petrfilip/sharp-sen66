#include <algorithm>
#include <string>

#include <unity.h>

#include "DashboardRenderer.h"
#include "DisplayFrameSignature.h"
#include "graph_renderer.h"
#include "history_manager.h"

namespace {

bool hasText(const Adafruit_SharpMem& display, const std::string& expected) {
  return std::any_of(display.textCalls.begin(), display.textCalls.end(), [&](const auto& call) {
    return call.text == expected;
  });
}

uint32_t countText(const Adafruit_SharpMem& display, const std::string& expected) {
  return static_cast<uint32_t>(std::count_if(display.textCalls.begin(), display.textCalls.end(), [&](const auto& call) {
    return call.text == expected;
  }));
}

bool hasLine(const Adafruit_SharpMem& display,
             const int16_t x1,
             const int16_t y1,
             const int16_t x2,
             const int16_t y2) {
  return std::any_of(display.lineCalls.begin(), display.lineCalls.end(), [&](const auto& call) {
    return call.x1 == x1 && call.y1 == y1 && call.x2 == x2 && call.y2 == y2;
  });
}

bool isPlotLine(const Adafruit_SharpMem::LineCall& call) {
  return call.x1 >= 57 && call.x1 <= 378 && call.x2 >= 57 && call.x2 <= 378 && call.y1 >= 35 && call.y1 <= 190 &&
         call.y2 >= 35 && call.y2 <= 190;
}

bool hasPlotLineBeforeX(const Adafruit_SharpMem& display, const int16_t thresholdX) {
  return std::any_of(display.lineCalls.begin(), display.lineCalls.end(), [&](const auto& call) {
    return isPlotLine(call) && std::min(call.x1, call.x2) < thresholdX;
  });
}

bool hasPlotLineAtOrAfterX(const Adafruit_SharpMem& display, const int16_t thresholdX) {
  return std::any_of(display.lineCalls.begin(), display.lineCalls.end(), [&](const auto& call) {
    return isPlotLine(call) && std::max(call.x1, call.x2) >= thresholdX;
  });
}

bool isPlotPixel(const Adafruit_SharpMem::PixelCall& call) {
  return call.x >= 57 && call.x <= 378 && call.y >= 35 && call.y <= 190;
}

uint32_t countPlotStrokes(const Adafruit_SharpMem& display) {
  const auto plotLineCount = static_cast<uint32_t>(std::count_if(display.lineCalls.begin(), display.lineCalls.end(), [&](const auto& call) {
    return isPlotLine(call);
  }));
  const auto plotPixelCount =
      static_cast<uint32_t>(std::count_if(display.pixelCalls.begin(), display.pixelCalls.end(), [&](const auto& call) {
        return isPlotPixel(call);
      }));
  return plotLineCount + plotPixelCount;
}

const Adafruit_SharpMem::RectCall* findFilledRect(const Adafruit_SharpMem& display,
                                                  const int16_t x,
                                                  const int16_t y) {
  const auto it = std::find_if(display.fillRectCalls.begin(), display.fillRectCalls.end(), [&](const auto& call) {
    return call.x == x && call.y == y;
  });
  return it != display.fillRectCalls.end() ? &(*it) : nullptr;
}

void addHistorySamples(airmon::HistoryManager& history, const std::initializer_list<float> values) {
  for (const float value : values) {
    airmon::SensorData sample;
    sample.co2 = value;
    sample.pm25 = value;
    sample.temp = value;
    sample.hum = value;
    sample.voc = value;
    sample.nox = value;
    history.addMinuteSample(sample);
  }
}

void addHistoryRampSamples(airmon::HistoryManager& history, const size_t count, const float startValue, const float step) {
  for (size_t index = 0; index < count; ++index) {
    const float value = startValue + (static_cast<float>(index) * step);
    airmon::SensorData sample;
    sample.co2 = value;
    sample.pm25 = value;
    sample.temp = value;
    sample.hum = value;
    sample.voc = value;
    sample.nox = value;
    history.addMinuteSample(sample);
  }
}

}  // namespace

void setUp() {}

void tearDown() {}

void test_graph_renderer_shows_placeholder_for_missing_history() {
  Adafruit_SharpMem display;
  airmon::HistoryManager history;
  airmon::GraphRenderer renderer(display);

  renderer.render(history, airmon::MetricId::PM25, airmon::HistoryRange::Range24H, 0.0f, false);

  TEST_ASSERT_EQUAL(1, display.clearDisplayCalls);
  TEST_ASSERT_EQUAL(1, display.refreshCalls);
  TEST_ASSERT_TRUE(hasText(display, "PM2.5"));
  TEST_ASSERT_TRUE(hasText(display, "Rozsah: 24h"));
  TEST_ASSERT_TRUE(hasText(display, "-- ug/m3"));
  TEST_ASSERT_TRUE(hasText(display, "Nedostatek dat"));
  TEST_ASSERT_TRUE(hasText(display, "-24h"));
  TEST_ASSERT_TRUE(hasText(display, "-12h"));
  TEST_ASSERT_TRUE(hasText(display, "ted"));
  TEST_ASSERT_EQUAL_UINT32(1U, static_cast<uint32_t>(display.rectCalls.size()));
  TEST_ASSERT_EQUAL_UINT32(7U, static_cast<uint32_t>(display.lineCalls.size()));

  const auto& graphBorder = display.rectCalls.front();
  TEST_ASSERT_EQUAL(56, graphBorder.x);
  TEST_ASSERT_EQUAL(34, graphBorder.y);
  TEST_ASSERT_EQUAL(324, graphBorder.w);
  TEST_ASSERT_EQUAL(158, graphBorder.h);
}

void test_graph_renderer_draws_series_axes_and_trend_arrow() {
  Adafruit_SharpMem display;
  airmon::HistoryManager history;
  addHistorySamples(history, {500.0f, 900.0f, 700.0f, 1100.0f});
  airmon::GraphRenderer renderer(display);

  renderer.render(history, airmon::MetricId::CO2, airmon::HistoryRange::Range24H, 1200.0f, true);

  TEST_ASSERT_EQUAL(1, display.refreshCalls);
  TEST_ASSERT_TRUE(hasText(display, "CO2"));
  TEST_ASSERT_TRUE(hasText(display, "1200 ppm"));
  TEST_ASSERT_TRUE(hasText(display, "1160"));
  TEST_ASSERT_TRUE(hasText(display, "980"));
  TEST_ASSERT_TRUE(hasText(display, "800"));
  TEST_ASSERT_TRUE(hasText(display, "620"));
  TEST_ASSERT_TRUE(hasText(display, "440"));
  TEST_ASSERT_TRUE(hasText(display, "-24h"));
  TEST_ASSERT_TRUE(hasText(display, "-12h"));
  TEST_ASSERT_TRUE(hasText(display, "ted"));
  TEST_ASSERT_TRUE(hasLine(display, 272, 23, 272, 13));
  TEST_ASSERT_TRUE(hasLine(display, 272, 13, 269, 17));
  TEST_ASSERT_TRUE(hasLine(display, 272, 13, 275, 17));
  TEST_ASSERT_GREATER_THAN_UINT32(0U, countPlotStrokes(display));
}

void test_graph_renderer_updates_live_value_without_full_redraw() {
  Adafruit_SharpMem display;
  airmon::HistoryManager history;
  addHistorySamples(history, {500.0f, 900.0f, 700.0f, 1100.0f});
  airmon::GraphRenderer renderer(display);

  renderer.render(history, airmon::MetricId::CO2, airmon::HistoryRange::Range24H, 1200.0f, true);

  const int clearDisplayCallsAfterFirstRender = display.clearDisplayCalls;
  const uint32_t rectCallsAfterFirstRender = static_cast<uint32_t>(display.rectCalls.size());
  const uint32_t lineCallsAfterFirstRender = static_cast<uint32_t>(display.lineCalls.size());

  renderer.render(history, airmon::MetricId::CO2, airmon::HistoryRange::Range24H, 1210.0f, true);

  TEST_ASSERT_EQUAL(clearDisplayCallsAfterFirstRender, display.clearDisplayCalls);
  TEST_ASSERT_EQUAL(2, display.refreshCalls);
  TEST_ASSERT_TRUE(hasText(display, "1210 ppm"));
  TEST_ASSERT_EQUAL_UINT32(rectCallsAfterFirstRender, static_cast<uint32_t>(display.rectCalls.size()));
  TEST_ASSERT_EQUAL_UINT32(lineCallsAfterFirstRender + 3U, static_cast<uint32_t>(display.lineCalls.size()));

  const auto* valueClear = findFilledRect(display, 210, 4);
  TEST_ASSERT_NOT_NULL(valueClear);
  TEST_ASSERT_EQUAL(180, valueClear->w);
  TEST_ASSERT_EQUAL(28, valueClear->h);
}

void test_graph_renderer_updates_plot_without_full_redraw_when_history_changes() {
  Adafruit_SharpMem display;
  airmon::HistoryManager history;
  addHistorySamples(history, {500.0f, 900.0f, 700.0f, 1100.0f});
  airmon::GraphRenderer renderer(display);

  renderer.render(history, airmon::MetricId::CO2, airmon::HistoryRange::Range24H, 1200.0f, true);

  const int clearDisplayCallsAfterFirstRender = display.clearDisplayCalls;
  const uint32_t rectCallsAfterFirstRender = static_cast<uint32_t>(display.rectCalls.size());
  const uint32_t plotStrokesAfterFirstRender = countPlotStrokes(display);

  airmon::SensorData sample;
  sample.co2 = 1300.0f;
  sample.pm25 = 1.0f;
  sample.temp = 20.0f;
  sample.hum = 50.0f;
  sample.voc = 1.0f;
  sample.nox = 1.0f;
  history.addMinuteSample(sample);

  renderer.render(history, airmon::MetricId::CO2, airmon::HistoryRange::Range24H, 1210.0f, true);

  TEST_ASSERT_EQUAL(clearDisplayCallsAfterFirstRender, display.clearDisplayCalls);
  TEST_ASSERT_EQUAL(2, display.refreshCalls);
  TEST_ASSERT_EQUAL_UINT32(rectCallsAfterFirstRender + 1U, static_cast<uint32_t>(display.rectCalls.size()));
  TEST_ASSERT_GREATER_THAN_UINT32(plotStrokesAfterFirstRender, countPlotStrokes(display));

  const auto* plotClear = findFilledRect(display, 0, 33);
  TEST_ASSERT_NOT_NULL(plotClear);
  TEST_ASSERT_EQUAL(390, plotClear->w);
  TEST_ASSERT_EQUAL(186, plotClear->h);
}

void test_graph_renderer_keeps_short_24h_history_near_present_edge() {
  Adafruit_SharpMem display;
  airmon::HistoryManager history;
  addHistoryRampSamples(history, 180U, 500.0f, 2.0f);
  airmon::GraphRenderer renderer(display);

  renderer.render(history, airmon::MetricId::CO2, airmon::HistoryRange::Range24H, 900.0f, true);

  TEST_ASSERT_TRUE(!hasPlotLineBeforeX(display, 330));
  TEST_ASSERT_TRUE(hasPlotLineAtOrAfterX(display, 360));
}

void test_graph_renderer_keeps_short_7d_history_near_present_edge() {
  Adafruit_SharpMem display;
  airmon::HistoryManager history;
  addHistoryRampSamples(history, 900U, 500.0f, 1.0f);
  airmon::GraphRenderer renderer(display);

  renderer.render(history, airmon::MetricId::CO2, airmon::HistoryRange::Range7D, 1400.0f, true);

  TEST_ASSERT_TRUE(!hasPlotLineBeforeX(display, 340));
  TEST_ASSERT_TRUE(hasPlotLineAtOrAfterX(display, 360));
}

void test_dashboard_renderer_shows_waiting_state_without_sensor_data() {
  Adafruit_SharpMem display;
  sharp::DashboardRenderer renderer(display);
  sharp::SensorSnapshot sensorData;
  sharp::DashboardRenderInfo info;
  info.wifiText = "WiFi:OK";
  info.tmepText = "TMEP:OK";
  info.mqttConnected = false;
  info.sen66Ready = false;
  info.uptimeText = "UP 00:01";
  info.dateTimeText = "2026-03-10 19:30";

  renderer.render(sensorData, info);

  TEST_ASSERT_EQUAL(1, display.refreshCalls);
  TEST_ASSERT_TRUE(hasText(display, "WiFi:OK"));
  TEST_ASSERT_TRUE(hasText(display, "TMEP:OK"));
  TEST_ASSERT_TRUE(hasText(display, "MQTT:---"));
  TEST_ASSERT_TRUE(hasText(display, "SEN66:---"));
  TEST_ASSERT_TRUE(hasText(display, "Cekam na data"));
  TEST_ASSERT_TRUE(hasText(display, "ze senzoru SEN66..."));
  TEST_ASSERT_EQUAL_UINT32(0U, static_cast<uint32_t>(display.fillRectCalls.size()));
}

void test_dashboard_renderer_draws_air_quality_bar_for_valid_data() {
  Adafruit_SharpMem display;
  sharp::DashboardRenderer renderer(display);

  sharp::SensorSnapshot sensorData;
  sensorData.pm1 = 10.0f;
  sensorData.pm25 = 75.0f;
  sensorData.pm4 = 12.0f;
  sensorData.pm10 = 18.0f;
  sensorData.temperature = 23.4f;
  sensorData.humidity = 45.6f;
  sensorData.voc = 101.0f;
  sensorData.nox = 77.0f;
  sensorData.co2 = 812;
  sensorData.valid = true;

  sharp::DashboardRenderInfo info;
  info.wifiText = "WiFi:192.168.1.10";
  info.tmepText = "TMEP:OK";
  info.mqttConnected = true;
  info.sen66Ready = true;
  info.uptimeText = "UP 10:20";
  info.dateTimeText = "2026-03-10 19:31";

  renderer.render(sensorData, info);

  TEST_ASSERT_EQUAL(1, display.refreshCalls);
  TEST_ASSERT_TRUE(hasText(display, "23.4"));
  TEST_ASSERT_TRUE(hasText(display, "45.6"));
  TEST_ASSERT_TRUE(hasText(display, "SPATNE"));
  TEST_ASSERT_TRUE(hasText(display, "MQTT:OK"));
  TEST_ASSERT_TRUE(hasText(display, "SEN66:OK"));

  const auto* barFill = findFilledRect(display, 271, 201);
  TEST_ASSERT_NOT_NULL(barFill);
  TEST_ASSERT_EQUAL(60, barFill->w);
  TEST_ASSERT_EQUAL(22, barFill->h);
}

void test_dashboard_renderer_updates_dynamic_regions_without_redrawing_static_labels() {
  Adafruit_SharpMem display;
  sharp::DashboardRenderer renderer(display);

  sharp::SensorSnapshot sensorData;
  sensorData.pm1 = 10.0f;
  sensorData.pm25 = 75.0f;
  sensorData.pm4 = 12.0f;
  sensorData.pm10 = 18.0f;
  sensorData.temperature = 23.4f;
  sensorData.humidity = 45.6f;
  sensorData.voc = 101.0f;
  sensorData.nox = 77.0f;
  sensorData.co2 = 812;
  sensorData.valid = true;

  sharp::DashboardRenderInfo info;
  info.wifiText = "WiFi:192.168.1.10";
  info.tmepText = "TMEP:OK";
  info.mqttConnected = true;
  info.sen66Ready = true;
  info.uptimeText = "UP 10:20";
  info.dateTimeText = "2026-03-10 19:31";

  renderer.render(sensorData, info);

  sensorData.temperature = 24.0f;
  sensorData.pm25 = 80.0f;
  info.uptimeText = "UP 10:21";
  info.dateTimeText = "2026-03-10 19:32";

  renderer.render(sensorData, info);

  TEST_ASSERT_EQUAL(1, display.clearDisplayCalls);
  TEST_ASSERT_EQUAL(2, display.refreshCalls);
  TEST_ASSERT_TRUE(hasText(display, "24.0"));
  TEST_ASSERT_EQUAL_UINT32(1U, countText(display, "PM2.5"));
  TEST_ASSERT_EQUAL_UINT32(1U, countText(display, "VOC Index"));
  TEST_ASSERT_EQUAL_UINT32(1U, countText(display, "Kvalita vzduchu:"));
  TEST_ASSERT_GREATER_THAN_UINT32(2U, static_cast<uint32_t>(display.fillRectCalls.size()));
}

void test_dashboard_frame_signature_ignores_raw_changes_that_do_not_change_visible_output() {
  sharp::SensorSnapshot baseline;
  baseline.pm1 = 10.2f;
  baseline.pm25 = 74.6f;
  baseline.pm4 = 12.1f;
  baseline.pm10 = 18.3f;
  baseline.temperature = 23.41f;
  baseline.humidity = 45.61f;
  baseline.voc = 101.2f;
  baseline.nox = 76.6f;
  baseline.co2 = 812;
  baseline.valid = true;

  sharp::SensorSnapshot sameVisible = baseline;
  sameVisible.pm1 = 10.4f;
  sameVisible.pm25 = 74.9f;
  sameVisible.pm4 = 12.4f;
  sameVisible.pm10 = 18.4f;
  sameVisible.temperature = 23.44f;
  sameVisible.humidity = 45.64f;
  sameVisible.voc = 101.4f;
  sameVisible.nox = 76.8f;

  const String baselineSignature = sharp::displayframes::buildDashboardFrameSignature(
      baseline, "WiFi:192.168.1.10", "TMEP:OK", true, true, "UP 10:20", "2026-03-10 19:31");
  const String sameVisibleSignature = sharp::displayframes::buildDashboardFrameSignature(
      sameVisible, "WiFi:192.168.1.10", "TMEP:OK", true, true, "UP 10:20", "2026-03-10 19:31");

  TEST_ASSERT_TRUE(baselineSignature == sameVisibleSignature);

  sameVisible.pm25 = 76.4f;
  const String changedSignature = sharp::displayframes::buildDashboardFrameSignature(
      sameVisible, "WiFi:192.168.1.10", "TMEP:OK", true, true, "UP 10:20", "2026-03-10 19:31");

  TEST_ASSERT_TRUE(baselineSignature != changedSignature);
}

void test_graph_frame_signature_ignores_raw_changes_when_visible_value_and_trend_match() {
  airmon::HistoryManager history;
  addHistorySamples(history, {20.0f, 20.0f, 20.0f, 20.0f});

  const String baselineSignature = sharp::displayframes::buildGraphFrameSignature(
      history, airmon::MetricId::TEMP, airmon::HistoryRange::Range24H, 20.11f, true);
  const String sameVisibleSignature = sharp::displayframes::buildGraphFrameSignature(
      history, airmon::MetricId::TEMP, airmon::HistoryRange::Range24H, 20.14f, true);

  TEST_ASSERT_TRUE(baselineSignature == sameVisibleSignature);

  const String changedSignature = sharp::displayframes::buildGraphFrameSignature(
      history, airmon::MetricId::TEMP, airmon::HistoryRange::Range24H, 20.51f, true);

  TEST_ASSERT_TRUE(baselineSignature != changedSignature);
}

void test_graph_frame_signature_changes_when_history_changes() {
  airmon::HistoryManager history;
  addHistorySamples(history, {500.0f, 600.0f, 700.0f});

  const String before = sharp::displayframes::buildGraphFrameSignature(
      history, airmon::MetricId::CO2, airmon::HistoryRange::Range24H, 710.0f, true);

  airmon::SensorData sample;
  sample.co2 = 720.0f;
  sample.pm25 = 1.0f;
  sample.temp = 20.0f;
  sample.hum = 50.0f;
  sample.voc = 1.0f;
  sample.nox = 1.0f;
  history.addMinuteSample(sample);

  const String after = sharp::displayframes::buildGraphFrameSignature(
      history, airmon::MetricId::CO2, airmon::HistoryRange::Range24H, 710.0f, true);

  TEST_ASSERT_TRUE(before != after);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_graph_renderer_shows_placeholder_for_missing_history);
  RUN_TEST(test_graph_renderer_draws_series_axes_and_trend_arrow);
  RUN_TEST(test_graph_renderer_updates_live_value_without_full_redraw);
  RUN_TEST(test_graph_renderer_updates_plot_without_full_redraw_when_history_changes);
  RUN_TEST(test_graph_renderer_keeps_short_24h_history_near_present_edge);
  RUN_TEST(test_graph_renderer_keeps_short_7d_history_near_present_edge);
  RUN_TEST(test_dashboard_renderer_shows_waiting_state_without_sensor_data);
  RUN_TEST(test_dashboard_renderer_draws_air_quality_bar_for_valid_data);
  RUN_TEST(test_dashboard_renderer_updates_dynamic_regions_without_redrawing_static_labels);
  RUN_TEST(test_dashboard_frame_signature_ignores_raw_changes_that_do_not_change_visible_output);
  RUN_TEST(test_graph_frame_signature_ignores_raw_changes_when_visible_value_and_trend_match);
  RUN_TEST(test_graph_frame_signature_changes_when_history_changes);
  return UNITY_END();
}
