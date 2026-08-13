#pragma once
#include <stdint.h>

#include "../hal/IClock.h"

namespace solar {

// Wall clock built on a monotonic millisecond source (e.g. Arduino millis()).
// The millisecond source stops during SAMD sleep, so the sleeper must report
// slept time through advance(). Unsigned subtraction keeps now() correct
// across the 49.7-day millis() wraparound as long as it is called at least
// once per wrap period (the main loop runs far more often than that).
class SoftClock : public IClock {
 public:
  typedef uint32_t (*MillisFn)();

  explicit SoftClock(MillisFn millisFn)
      : millisFn_(millisFn), valid_(false), epoch_(0), lastMillis_(0) {}

  bool isValid() const override { return valid_; }

  void sync(uint32_t localEpoch) override {
    epoch_ = localEpoch;
    lastMillis_ = millisFn_();
    valid_ = true;
  }

  uint32_t now() override {
    const uint32_t m = millisFn_();
    const uint32_t elapsedSeconds = (m - lastMillis_) / 1000UL;
    epoch_ += elapsedSeconds;
    lastMillis_ += elapsedSeconds * 1000UL;  // keep the sub-second remainder
    return epoch_;
  }

  // Called by the sleeper after a sleep, during which millis() was frozen.
  void advance(uint32_t ms) { epoch_ += ms / 1000UL; }

 private:
  MillisFn millisFn_;
  bool valid_;
  uint32_t epoch_;
  uint32_t lastMillis_;
};

}  // namespace solar
