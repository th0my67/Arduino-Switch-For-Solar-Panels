#ifndef POWER_MONITOR_SERVICE_H
#define POWER_MONITOR_SERVICE_H

#include "../hal/interfaces/i_network_interface.h"
#include <memory>
#include <optional>

enum class PowerError {
    NetworkDown,
    InvalidResponse,
    Timeout,
    ParseError
};

class PowerMonitor {
    public:
        PowerMonitor(std::unique_ptr<INetworkInterface> network);
        ~PowerMonitor() = default;
        Result<float,PowerError> getOutputPower();
    private:
        std::unique_ptr<INetworkInterface> network_;
        float lastSuccessfulReading_;
        unsigned long lastSuccessfulReadingMs_;
        int numberOfRetries_;
        
        const int maxRetries_ = 3;
        const int retryDelayMs_ = 1000;
        const int readingTimeoutMs_ = 5000;
};

#endif