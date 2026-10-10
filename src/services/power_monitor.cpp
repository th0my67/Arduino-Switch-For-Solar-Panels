#include "services/power_monitor.h"
#include <Arduino.h>

PowerMonitor::PowerMonitor(std::unique_ptr<IInverterAPI> inverterAPI,
                           uint8_t maxRetries,
                           uint16_t retryDelayMs,
                           WaitMsFn waitMs)
    : inverterAPI_(std::move(inverterAPI)),
      maxRetries_(maxRetries),
      retryDelayMs_(retryDelayMs),
      waitMs_(waitMs == nullptr ? defaultWait : waitMs) {}

PowerError PowerMonitor::mapError(InverterError error) {
    switch (error) {
        case InverterError::NetworkFailure:
        case InverterError::AuthenticationFailure:
        case InverterError::ServiceUnavailable:
            return PowerError::NetworkDown;
        case InverterError::InvalidResponse:
            return PowerError::ParseError;
        case InverterError::RateLimited:
            return PowerError::Timeout;
    }
    return PowerError::ParseError; // unreachable, keeps -Wreturn-type happy
}

Result<int, PowerError> PowerMonitor::getOutputPower() {
    PowerError lastPowerError = PowerError::NetworkDown;

    for (uint8_t attempt = 0; ; ++attempt) {
        auto result = inverterAPI_->getCurrentPower();
        if (result.isOk()) {
            lastSuccessfulReading_ = result.unwrap();
            lastSuccessfulReadingMs_ = millis();
            return Result<int, PowerError>::ok(lastSuccessfulReading_);
        }

        lastPowerError = mapError(result.unwrapErr());
        if (attempt >= maxRetries_) {
            break; // bounded: at most maxRetries_ + 1 requests
        }
        // Linear backoff: retryDelayMs, 2*retryDelayMs, 3*retryDelayMs...
        waitMs_((uint32_t)retryDelayMs_ * (attempt + 1));
    }

    return Result<int, PowerError>::err(lastPowerError);
}
