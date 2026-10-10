#include "services/backup_charge.h"

BackupChargeController::BackupChargeController(IForecastProvider& provider,
                                               const BackupChargeConfig& config)
    : provider_(&provider), config_(config) {}

void BackupChargeController::tick(unsigned long localEpochSeconds,
                                  const ActivationTracker& tracker) {
    if (localEpochSeconds == 0) return; // no valid clock

    // Housekeeping: a schedule that is past its window is dropped.
    if (hasSchedule_ && localEpochSeconds >= endEpoch_) {
        hasSchedule_ = false;
    }

    const long day = (long)(localEpochSeconds / 86400UL);
    const int hour = (int)((localEpochSeconds / 3600UL) % 24UL);

    // One successful evaluation per day, inside the fetch hour.
    if (hour != config_.fetchHour) return;
    if (lastEvaluateDay_ == day) return;
    if ((localEpochSeconds - lastAttemptEpoch_) < kRetryIntervalS &&
        lastAttemptEpoch_ != 0) {
        return; // throttle retries within the hour
    }
    lastAttemptEpoch_ = localEpochSeconds;

    const auto forecast = provider_->getTomorrowSunshineHours(localEpochSeconds);
    if (forecast.isError()) {
        return; // no data -> no forced charge; retried after kRetryIntervalS
    }
    lastEvaluateDay_ = day; // evaluated successfully

    const uint32_t onMs =
        tracker.onMsInLastDays(localEpochSeconds, config_.historyDays);
    const uint32_t thresholdMs =
        (uint32_t)(config_.minActivationHours * 3600.0f) * 1000UL;

    const bool notEnoughHeating = onMs < thresholdMs;
    const bool poorSunTomorrow = forecast.unwrap() < config_.sunshineThresholdHours;

    if (!notEnoughHeating || !poorSunTomorrow) {
        return;
    }

    // Tomorrow, from startHour local, for chargeDurationHours.
    startEpoch_ = ((unsigned long)day + 1UL) * 86400UL +
                  (unsigned long)config_.startHour * 3600UL;
    endEpoch_ = startEpoch_ + (unsigned long)config_.chargeDurationHours * 3600UL;
    hasSchedule_ = true;
}

bool BackupChargeController::shouldForceOn(unsigned long localEpochSeconds) const {
    if (!hasSchedule_ || localEpochSeconds == 0) return false;
    return localEpochSeconds >= startEpoch_ && localEpochSeconds < endEpoch_;
}
