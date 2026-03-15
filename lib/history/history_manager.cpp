#include "history_manager.h"

namespace airmon {

namespace {

const RingBuffer<float, 1440>& minuteBuffer(const HistorySeries& series) {
  return series.minute24h;
}

const RingBuffer<float, 672>& quarterBuffer(const HistorySeries& series) {
  return series.quarter7d;
}

}  // namespace

void HistoryManager::addMinuteSample(const SensorData& sample) {
  const size_t co2Index = metricIndex(MetricId::CO2);
  const size_t pm1Index = metricIndex(MetricId::PM1);
  const size_t pm25Index = metricIndex(MetricId::PM25);
  const size_t pm4Index = metricIndex(MetricId::PM4);
  const size_t pm10Index = metricIndex(MetricId::PM10);
  const size_t tempIndex = metricIndex(MetricId::TEMP);
  const size_t humIndex = metricIndex(MetricId::HUM);
  const size_t vocIndex = metricIndex(MetricId::VOC);
  const size_t noxIndex = metricIndex(MetricId::NOX);

  appendSample(series_[co2Index], sample.co2);
  appendSample(series_[pm1Index], sample.pm1);
  appendSample(series_[pm25Index], sample.pm25);
  appendSample(series_[pm4Index], sample.pm4);
  appendSample(series_[pm10Index], sample.pm10);
  appendSample(series_[tempIndex], sample.temp);
  appendSample(series_[humIndex], sample.hum);
  appendSample(series_[vocIndex], sample.voc);
  appendSample(series_[noxIndex], sample.nox);

  ++minuteRevisions_[co2Index];
  ++minuteRevisions_[pm1Index];
  ++minuteRevisions_[pm25Index];
  ++minuteRevisions_[pm4Index];
  ++minuteRevisions_[pm10Index];
  ++minuteRevisions_[tempIndex];
  ++minuteRevisions_[humIndex];
  ++minuteRevisions_[vocIndex];
  ++minuteRevisions_[noxIndex];

  if (series_[co2Index].aggregateCount == 0U) ++quarterRevisions_[co2Index];
  if (series_[pm1Index].aggregateCount == 0U) ++quarterRevisions_[pm1Index];
  if (series_[pm25Index].aggregateCount == 0U) ++quarterRevisions_[pm25Index];
  if (series_[pm4Index].aggregateCount == 0U) ++quarterRevisions_[pm4Index];
  if (series_[pm10Index].aggregateCount == 0U) ++quarterRevisions_[pm10Index];
  if (series_[tempIndex].aggregateCount == 0U) ++quarterRevisions_[tempIndex];
  if (series_[humIndex].aggregateCount == 0U) ++quarterRevisions_[humIndex];
  if (series_[vocIndex].aggregateCount == 0U) ++quarterRevisions_[vocIndex];
  if (series_[noxIndex].aggregateCount == 0U) ++quarterRevisions_[noxIndex];
}

size_t HistoryManager::pointCount(const MetricId metric, const HistoryRange range) const {
  const HistorySeries& series = series_[metricIndex(metric)];
  return range == HistoryRange::Range24H ? minuteBuffer(series).count() : quarterBuffer(series).count();
}

bool HistoryManager::pointAt(const MetricId metric, const HistoryRange range, const size_t index, float& out) const {
  const HistorySeries& series = series_[metricIndex(metric)];
  return range == HistoryRange::Range24H ? minuteBuffer(series).getOldest(index, out)
                                         : quarterBuffer(series).getOldest(index, out);
}

bool HistoryManager::latest(const MetricId metric, const HistoryRange range, float& out) const {
  const HistorySeries& series = series_[metricIndex(metric)];
  return range == HistoryRange::Range24H ? minuteBuffer(series).latest(out) : quarterBuffer(series).latest(out);
}

uint32_t HistoryManager::revision(const MetricId metric, const HistoryRange range) const {
  const size_t index = metricIndex(metric);
  return range == HistoryRange::Range24H ? minuteRevisions_[index] : quarterRevisions_[index];
}

size_t HistoryManager::metricIndex(const MetricId metric) { return static_cast<size_t>(metric); }

void HistoryManager::appendSample(HistorySeries& series, const float value) {
  series.minute24h.push(value);
  series.aggregateSum += value;
  ++series.aggregateCount;

  if (series.aggregateCount < kAggregateWindow) {
    return;
  }

  series.quarter7d.push(series.aggregateSum / static_cast<float>(series.aggregateCount));
  series.aggregateSum = 0.0f;
  series.aggregateCount = 0;
}

}  // namespace airmon
