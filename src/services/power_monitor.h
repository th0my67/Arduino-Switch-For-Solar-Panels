#ifndef POWER_MONITOR_SERVICE_H
#define POWER_MONITOR_SERVICE_H

#include "../hal/interfaces/i_inverter_api.h"
#include "../utils/result.h"
#include <memory>

enum class PowerError {
    NetworkDown,
    InvalidResponse,
    Timeout,
    ParseError
};

class PowerMonitor {
    public:
        PowerMonitor(std::unique_ptr<IInverterAPI> inverterAPI);
        ~PowerMonitor() = default;
        Result<float, PowerError> getOutputPower();
    private:
        std::unique_ptr<IInverterAPI> inverterAPI_;
        float lastSuccessfulReading_;
        unsigned long lastSuccessfulReadingMs_;
        int numberOfRetries_;
        
        const int maxRetries_ = 3;
        const int defaultRetryDelayMs_ = 1000;
        const int readingTimeoutMs_ = 5000;
};

#endif