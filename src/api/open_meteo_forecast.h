#ifndef OPEN_METEO_FORECAST_H
#define OPEN_METEO_FORECAST_H

#include "hal/interfaces/i_forecast_provider.h"
#include "hal/interfaces/i_http_client.h"
#include <Arduino.h>
#include <ArduinoJson.h>

// Sunshine forecast via the Open-Meteo daily forecast API (HTTPS, free,
// no API key). Uses the official `sunshine_hours` daily variable.
class OpenMeteoForecast : public IForecastProvider {
public:
    OpenMeteoForecast(double latitude, double longitude,
                      const char* timezone, IHttpClient& httpClient);
    virtual ~OpenMeteoForecast() = default;

    Result<float, ForecastError> getTomorrowSunshineHours(
        unsigned long localEpochSeconds) override;

    // Extracts the sunshine hours for the given local day number from an
    // Open-Meteo daily payload. Returns false when the day is absent, the
    // value is null (not forecast yet) or not a number.
    static bool extractSunshineForDay(const JsonDocument& doc,
                                      long localDayNumber,
                                      float& hoursOut);

    // Parses a strict "YYYY-MM-DD" date into a Unix day number
    // (days since 1970-01-01). Returns false on malformed input.
    static bool dayNumberFromDateString(const char* date, long& dayOut);

    static ForecastError mapHttpError(HttpError error);

    const char* getProviderName() const { return "Open-Meteo"; }

private:
    IHttpClient* httpClient_;
    String url_; // prebuilt once in the constructor
};

#endif
