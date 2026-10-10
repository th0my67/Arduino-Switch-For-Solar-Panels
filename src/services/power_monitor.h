#ifndef POWER_MONITOR_SERVICE_H
#define POWER_MONITOR_SERVICE_H

#include "hal/interfaces/i_inverter_api.h"
#include "utils/result.h"
#include <memory>

enum class PowerError {
    NetworkDown,
    InvalidResponse,
    Timeout,
    ParseError
};

class PowerMonitor {
public:
    // waitMs is called between retry attempts; on hardware it should map to
    // a low-power sleep rather than a busy delay.
    typedef void (*WaitMsFn)(uint32_t ms);

    static void defaultWait(uint32_t ms) { delay(ms); }

    explicit PowerMonitor(std::unique_ptr<IInverterAPI> inverterAPI,
                          uint8_t maxRetries = 3,
                          uint16_t retryDelayMs = 500,
                          WaitMsFn waitMs = defaultWait);
    ~PowerMonitor() = default;

    // Reads the inverter output power, retrying a bounded number of times
    // with linear backoff. Never loops forever, never aborts the process.
    Result<int, PowerError> getOutputPower();

    static PowerError mapError(InverterError error);

    int lastSuccessfulReading() const { return lastSuccessfulReading_; }
    unsigned long lastSuccessfulReadingMs() const { return lastSuccessfulReadingMs_; }

private:
    std::unique_ptr<IInverterAPI> inverterAPI_;
    int lastSuccessfulReading_ = -1;
    unsigned long lastSuccessfulReadingMs_ = 0;
    uint8_t maxRetries_;
    uint16_t retryDelayMs_;
    WaitMsFn waitMs_;
};

#endif
