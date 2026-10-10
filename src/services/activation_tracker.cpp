#include "services/activation_tracker.h"

namespace {
const long kNoDay = -1000000000L; // sentinel: never a valid Unix day number
}

ActivationTracker::ActivationTracker() {
    for (uint8_t i = 0; i < kMaxDays; ++i) {
        days_[i] = {kNoDay, 0};
    }
}

ActivationTracker::DayBucket& ActivationTracker::bucketForDay(long day) {
    // Exact entry, a free one, or the oldest one (recycled, reset).
    DayBucket* free_ = nullptr;
    DayBucket* oldest = &days_[0];
    for (uint8_t i = 0; i < kMaxDays; ++i) {
        if (days_[i].day == day) return days_[i];
        if (free_ == nullptr && days_[i].day == kNoDay) free_ = &days_[i];
        if (days_[i].day < oldest->day) oldest = &days_[i];
    }
    DayBucket& bucket = (free_ != nullptr) ? *free_ : *oldest;
    bucket.day = day;
    bucket.onMs = 0; // a recycled bucket starts fresh
    return bucket;
}

const ActivationTracker::DayBucket* ActivationTracker::findBucket(long day) const {
    for (uint8_t i = 0; i < kMaxDays; ++i) {
        if (days_[i].day == day) return &days_[i];
    }
    return nullptr;
}

void ActivationTracker::record(unsigned long localEpochSeconds, bool switchOn) {
    const long day = (long)(localEpochSeconds / 86400UL);

    if (lastEpoch_ != 0 && lastOn_) {
        // ON time since the previous sample, attributed to the day the
        // interval started in. Capped to 24 h to survive any anomaly
        // (reboot, stale samples, clock jump) without polluting history.
        unsigned long deltaS = localEpochSeconds - lastEpoch_;
        if (deltaS > 86400UL) deltaS = 86400UL;
        bucketForDay(lastDay_).onMs += (uint32_t)deltaS * 1000UL;
    }

    lastEpoch_ = localEpochSeconds;
    lastDay_ = day;
    lastOn_ = switchOn;
}

uint32_t ActivationTracker::onMsInLastDays(unsigned long localEpochSeconds,
                                           uint8_t days) const {
    const long today = (long)(localEpochSeconds / 86400UL);
    if (days == 0) return 0;
    const long firstDay = today - (long)days + 1; // inclusive window

    uint32_t totalMs = 0;
    for (uint8_t i = 0; i < kMaxDays; ++i) {
        if (days_[i].day != kNoDay && days_[i].day >= firstDay && days_[i].day <= today) {
            totalMs += days_[i].onMs;
        }
    }
    return totalMs;
}
