#pragma once

#include <array>
#include <stddef.h>
#include <stdint.h>

#include "metrics.h"

namespace airmon {

template <typename T, size_t Capacity>
class RingBuffer {
 public:
  void push(const T& value) {
    if (count_ < Capacity) {
      data_[(start_ + count_) % Capacity] = value;
      ++count_;
      return;
    }

    data_[start_] = value;
    start_ = (start_ + 1U) % Capacity;
  }

  bool getOldest(const size_t index, T& out) const {
    if (index >= count_) {
      return false;
    }

    out = data_[(start_ + index) % Capacity];
    return true;
  }

  bool latest(T& out) const {
    if (count_ == 0) {
      return false;
    }

    out = data_[(start_ + count_ - 1U) % Capacity];
    return true;
  }

  size_t count() const { return count_; }

 private:
  std::array<T, Capacity> data_{};
  size_t start_ = 0;
  size_t count_ = 0;
};

struct HistorySeries {
  RingBuffer<float, 1440> minute24h;
  RingBuffer<float, 672> quarter7d;
  float aggregateSum = 0.0f;
  uint8_t aggregateCount = 0;
};

class HistoryManager {
 public:
  void addMinuteSample(const SensorData& sample);

  size_t pointCount(MetricId metric, HistoryRange range) const;
  bool pointAt(MetricId metric, HistoryRange range, size_t index, float& out) const;
  bool latest(MetricId metric, HistoryRange range, float& out) const;
  uint32_t revision(MetricId metric, HistoryRange range) const;

 private:
  static constexpr uint8_t kAggregateWindow = 15;

  static size_t metricIndex(MetricId metric);
  static void appendSample(HistorySeries& series, float value);

  std::array<HistorySeries, kMetricCount> series_{};
  std::array<uint32_t, kMetricCount> minuteRevisions_{};
  std::array<uint32_t, kMetricCount> quarterRevisions_{};
};

}  // namespace airmon
