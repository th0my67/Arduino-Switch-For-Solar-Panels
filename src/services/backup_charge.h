#ifndef BACKUP_CHARGE_H
#define BACKUP_CHARGE_H

#include "hal/interfaces/i_forecast_provider.h"
#include "services/activation_tracker.h"
#include <Arduino.h>

// Backup grid-charging of the boiler.
//
// Once per day (at fetchHour), if the switch has been ON less than
// minActivationHours over the last historyDays AND the forecast promises
// less than sunshineThresholdHours of sunshine tomorrow, the boiler is
// charged from the grid the NEXT day: the switch is forced ON from
// startHour for chargeDurationHours.
//
// Fail-safe: without a valid clock or a usable forecast, nothing is ever
// forced ON from the grid.
struct BackupChargeConfig {
    float minActivationHours;      // e.g. 2.0
    uint8_t historyDays;          // e.g. 2 (look-back window)
    float sunshineThresholdHours; // e.g. 4.0
    uint8_t startHour;            // e.g. 13 (local)
    uint8_t chargeDurationHours;  // e.g. 3
    uint8_t fetchHour;            // e.g. 18 (local, daily evaluation)
};

class BackupChargeController {
public:
    BackupChargeController(IForecastProvider& provider,
                           const BackupChargeConfig& config);

    // Daily evaluation + window housekeeping. Call every poll with the
    // current LOCAL epoch seconds (0 = clock invalid: does nothing).
    void tick(unsigned long localEpochSeconds, const ActivationTracker& tracker);

    // True while the forced-charge window is active.
    bool shouldForceOn(unsigned long localEpochSeconds) const;

    bool isScheduled() const { return hasSchedule_; }
    unsigned long scheduledStartEpoch() const { return startEpoch_; }
    unsigned long scheduledEndEpoch() const { return endEpoch_; }
    bool evaluatedToday(long localDayNumber) const {
        return lastEvaluateDay_ == localDayNumber;
    }

    static const uint32_t kRetryIntervalS = 900; // retry a failed fetch every 15 min

private:
    IForecastProvider* provider_;
    BackupChargeConfig config_;
    bool hasSchedule_ = false;
    unsigned long startEpoch_ = 0;
    unsigned long endEpoch_ = 0;
    long lastEvaluateDay_ = -1000000000L;
    unsigned long lastAttemptEpoch_ = 0;
};

#endif
