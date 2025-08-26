#include "services/power_monitor.h"
#include "Arduino.h"

PowerMonitor::PowerMonitor(std::unique_ptr<IInverterAPI> inverterAPI):
    inverterAPI_(std::move(inverterAPI)),
    lastSuccessfulReading_(0.0f),
    lastSuccessfulReadingMs_(0),
    numberOfRetries_(0) {}

//TODO: Finish implementing retries and error handling
Result<float, PowerError> PowerMonitor::getOutputPower() {
    unsigned long startTime = millis();
    while (numberOfRetries_ < maxRetries_) {
        auto result = inverterAPI_ -> getCurrentPower();
        if (result.isOk()){
            lastSuccessfulReading_ = result.unwrap();
            lastSuccessfulReadingMs_ = millis();
            numberOfRetries_ = 0; // Reset retries on success
            return Result<float, PowerError>(lastSuccessfulReading_);
        } else {
            numberOfRetries_++;
            InverterError err = result.unwrapErr();
            PowerError powerErr;
            switch (err) {
                default:
                    exit(1);
                    powerErr = PowerError::ParseError;
                    break;
            }
            if (millis() - startTime > readingTimeoutMs_) {
                return Result<float, PowerError>(PowerError::Timeout);
            }
            delay(defaultRetryDelayMs_*numberOfRetries_);
        }
    }
}