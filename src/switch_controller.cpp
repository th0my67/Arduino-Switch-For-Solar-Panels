#include "switch_controller.h"

SwitchController::SwitchController(uint8_t pin,
                                   int onThresholdW,
                                   int offThresholdW,
                                   uint32_t minOnTimeMs,
                                   uint32_t minOffTimeMs,
                                   uint8_t consecutiveFailuresBeforeOff)
    : pin_(pin),
      minOnTimeMs_(minOnTimeMs),
      minOffTimeMs_(minOffTimeMs),
      failureLimit_(consecutiveFailuresBeforeOff) {
    // Normalize an inverted configuration: hysteresis requires
    // onThreshold > offThreshold.
    if (onThresholdW <= offThresholdW) {
        const int mid = (onThresholdW + offThresholdW) / 2;
        onThresholdW_ = mid + 1;
        offThresholdW_ = mid;
    } else {
        onThresholdW_ = onThresholdW;
        offThresholdW_ = offThresholdW;
    }
}

void SwitchController::begin(uint32_t nowMs) {
    pinMode(pin_, OUTPUT);
    setOutput(false, nowMs);
}

void SwitchController::setOutput(bool on, uint32_t nowMs) {
    on_ = on;
    lastChangeMs_ = nowMs;
    digitalWrite(pin_, on ? HIGH : LOW);
}

uint32_t SwitchController::timeInCurrentStateMs(uint32_t nowMs) const {
    return nowMs - lastChangeMs_; // modular subtraction is rollover-safe
}

void SwitchController::forceOff(uint32_t nowMs) {
    if (on_) {
        setOutput(false, nowMs);
    }
}

void SwitchController::applyReading(const Result<int, PowerError>& reading,
                                    uint32_t nowMs) {
    if (reading.isError()) {
        if (consecutiveFailures_ != 255) {
            ++consecutiveFailures_;
        }
        // Fail-safe: stop heating when the inverter state is unknown for
        // too long, regardless of minimum ON time.
        if (on_ && consecutiveFailures_ >= failureLimit_) {
            setOutput(false, nowMs);
        }
        return;
    }

    consecutiveFailures_ = 0;
    const int powerW = reading.unwrap();
    const uint32_t elapsed = nowMs - lastChangeMs_;

    if (!on_) {
        if (powerW >= onThresholdW_ && elapsed >= minOffTimeMs_) {
            setOutput(true, nowMs);
        }
    } else {
        if (powerW <= offThresholdW_ && elapsed >= minOnTimeMs_) {
            setOutput(false, nowMs);
        }
    }
}
