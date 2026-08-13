#pragma once
#include <stdint.h>

namespace solar {

// Tracks how long the switch stayed ON per calendar day (local time) over a
// small rolling window. Lives in RAM only: it survives SAMD standby sleep
// (RAM is retained) but not a reset — acceptable for a heuristic input.
class ActivationHistory {
 public:
  ActivationHistory();

  void begin(uint32_t nowEpoch);

  // Must be called on every switch update (or at least once per poll) so
  // that ON time is integrated against the wall clock, split per day.
  void onSwitchState(bool on, uint32_t nowEpoch);

  uint32_t secondsOnForDay(uint32_t dayIndex) const;

  // Total ON hours over the last nDays calendar days *including* today.
  float hoursOnLastDays(uint32_t todayIndex, uint8_t nDays) const;

 private:
  static const uint8_t kBucketCount = 4;
  struct Bucket {
    uint32_t day;
    uint32_t secondsOn;
  };

  Bucket& bucketFor(uint32_t day);
  void accumulate(uint32_t fromEpoch, uint32_t toEpoch);

  Bucket buckets_[kBucketCount];
  bool lastOn_;
  uint32_t lastEpoch_;
};

}  // namespace solar
