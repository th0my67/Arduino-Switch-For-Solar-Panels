#ifndef ACTIVATION_TRACKER_H
#define ACTIVATION_TRACKER_H

#include <Arduino.h>

// Accumulates how long the switch stayed ON, per local day, so the
// backup-charge feature can tell whether the sun actually heated the
// boiler over the last days.
//
// All inputs are LOCAL Unix epoch seconds (already shifted by the UTC
// offset), which keeps working across deep sleeps — unlike millis().
// History lives in RAM only: a power cycle forgets it, which can only
// cause one extra backup charge while the history rebuilds.
class ActivationTracker {
public:
    static const uint8_t kMaxDays = 8; // covers the largest history window

    ActivationTracker();

    // Samples the switch state. Call once per poll loop, AFTER the
    // switch decision. ON time between two samples is attributed to the
    // day the interval started in, capped at 24 h per interval.
    void record(unsigned long localEpochSeconds, bool switchOn);

    // Total ON time over the last `days` days (including today), in ms.
    uint32_t onMsInLastDays(unsigned long localEpochSeconds, uint8_t days) const;

    bool wasOnAtLastSample() const { return lastOn_; }

private:
    struct DayBucket {
        long day;
        uint32_t onMs;
    };

    DayBucket& bucketForDay(long day);
    const DayBucket* findBucket(long day) const;

    DayBucket days_[kMaxDays];
    unsigned long lastEpoch_ = 0;
    long lastDay_ = 0;
    bool lastOn_ = false;
};

#endif
