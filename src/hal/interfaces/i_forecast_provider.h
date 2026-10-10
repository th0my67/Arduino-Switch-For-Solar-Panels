#ifndef I_FORECAST_PROVIDER_H
#define I_FORECAST_PROVIDER_H

#include "utils/result.h"

enum class ForecastError {
    NetworkFailure,
    Timeout,
    InvalidResponse,
    ServiceUnavailable
};

// Sunshine forecast source. Implementations fetch the expected number of
// sunshine hours for the next day at the configured location.
// Note: MeteoSwiss publishes its sunshine forecast only as all-locations
// CSV files (several MB, unusable on this board), so the shipped
// implementation is Open-Meteo; any provider can be plugged in here.
class IForecastProvider {
public:
    virtual ~IForecastProvider() = default;

    // localEpochSeconds: current LOCAL time as Unix epoch seconds
    // (already shifted by the UTC offset). Returns the sunshine hours
    // forecast for tomorrow, or an error.
    virtual Result<float, ForecastError> getTomorrowSunshineHours(
        unsigned long localEpochSeconds) = 0;
};

#endif
