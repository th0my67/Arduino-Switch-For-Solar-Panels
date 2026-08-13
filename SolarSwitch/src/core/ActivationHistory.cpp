#include "ActivationHistory.h"

#include "TimeUtils.h"

namespace solar {

ActivationHistory::ActivationHistory() : lastOn_(false), lastEpoch_(0) {
  for (uint8_t i = 0; i < kBucketCount; i++) {
    buckets_[i].day = 0;
    buckets_[i].secondsOn = 0;
  }
}

void ActivationHistory::begin(uint32_t nowEpoch) {
  lastOn_ = false;
  lastEpoch_ = nowEpoch;
}

void ActivationHistory::onSwitchState(bool on, uint32_t nowEpoch) {
  if (lastOn_ && nowEpoch > lastEpoch_) accumulate(lastEpoch_, nowEpoch);
  lastOn_ = on;
  lastEpoch_ = nowEpoch;
}

void ActivationHistory::accumulate(uint32_t from, uint32_t to) {
  while (from < to) {
    const uint32_t day = timeutil::dayIndex(from);
    const uint32_t dayEnd = (day + 1) * timeutil::kSecondsPerDay;
    const uint32_t chunkEnd = (to < dayEnd) ? to : dayEnd;
    bucketFor(day).secondsOn += chunkEnd - from;
    from = chunkEnd;
  }
}

ActivationHistory::Bucket& ActivationHistory::bucketFor(uint32_t day) {
  Bucket* oldest = &buckets_[0];
  for (uint8_t i = 0; i < kBucketCount; i++) {
    if (buckets_[i].day == day) return buckets_[i];
    if (buckets_[i].day < oldest->day) oldest = &buckets_[i];
  }
  oldest->day = day;
  oldest->secondsOn = 0;
  return *oldest;
}

uint32_t ActivationHistory::secondsOnForDay(uint32_t dayIndex) const {
  for (uint8_t i = 0; i < kBucketCount; i++) {
    if (buckets_[i].day == dayIndex) return buckets_[i].secondsOn;
  }
  return 0;
}

float ActivationHistory::hoursOnLastDays(uint32_t todayIndex,
                                         uint8_t nDays) const {
  uint32_t totalSeconds = 0;
  for (uint8_t i = 0; i < nDays; i++) {
    if (todayIndex < i) break;
    totalSeconds += secondsOnForDay(todayIndex - i);
  }
  return totalSeconds / 3600.0f;
}

}  // namespace solar
