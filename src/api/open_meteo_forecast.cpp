#include "api/open_meteo_forecast.h"
#include "utils/result.h"
#include <cstdio>
#include <cstring>

namespace {
// Days since 1970-01-01 from a civil date (Howard Hinnant's algorithm).
long daysFromCivil(long y, unsigned m, unsigned d) {
    y -= (m <= 2) ? 1 : 0;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? (unsigned)-3 : 9u)) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097L + (long)doe - 719468L;
}

bool isDigits(const char* s, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
    }
    return true;
}
} // namespace

OpenMeteoForecast::OpenMeteoForecast(double latitude, double longitude,
                                     const char* timezone,
                                     IHttpClient& httpClient)
    : httpClient_(&httpClient) {
    // Built once: the location never changes at runtime.
    url_ = String("https://api.open-meteo.com/v1/forecast?latitude=") +
           String(latitude, 4) +
           String("&longitude=") + String(longitude, 4) +
           String("&daily=sunshine_hours&forecast_days=2&timezone=") +
           String(timezone ? timezone : "UTC");
}

bool OpenMeteoForecast::dayNumberFromDateString(const char* date,
                                                long& dayOut) {
    // Strict "YYYY-MM-DD": 10 characters, digits in fixed positions.
    if (date == nullptr) return false;
    const size_t len = strlen(date);
    if (len != 10 || date[4] != '-' || date[7] != '-') return false;
    if (!isDigits(date, 4) || !isDigits(date + 5, 2) || !isDigits(date + 8, 2)) {
        return false;
    }
    const long y = (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
                   (date[2] - '0') * 10 + (date[3] - '0');
    const unsigned m = (unsigned)((date[5] - '0') * 10 + (date[6] - '0'));
    const unsigned d = (unsigned)((date[8] - '0') * 10 + (date[9] - '0'));
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    dayOut = daysFromCivil(y, m, d);
    return true;
}

bool OpenMeteoForecast::extractSunshineForDay(const JsonDocument& doc,
                                               long localDayNumber,
                                               float& hoursOut) {
    JsonVariantConst time = doc["daily"]["time"];
    JsonVariantConst sunshine = doc["daily"]["sunshine_hours"];
    if (time.isNull() || sunshine.isNull()) return false;

    JsonArrayConst timeArray = time.as<JsonArrayConst>();
    JsonArrayConst sunshineArray = sunshine.as<JsonArrayConst>();
    if (timeArray.isNull() || sunshineArray.isNull()) return false;

    size_t index = 0;
    for (JsonVariantConst entry : timeArray) {
        if (!entry.is<const char*>()) return false;
        long day = 0;
        if (!dayNumberFromDateString(entry.as<const char*>(), day)) return false;
        if (day == localDayNumber) {
            JsonVariantConst value = sunshineArray[index];
            if (value.isNull()) return false; // not forecast yet
            if (value.is<float>()) {
                hoursOut = value.as<float>();
                return true;
            }
            if (value.is<int>()) {
                hoursOut = (float)value.as<int>();
                return true;
            }
            return false; // not a number
        }
        ++index;
    }
    return false; // day not in the forecast
}

ForecastError OpenMeteoForecast::mapHttpError(HttpError error) {
    switch (error) {
        case HttpError::Timeout:
        case HttpError::NetworkFailure:
            return ForecastError::NetworkFailure;
        case HttpError::ServerError:
            return ForecastError::ServiceUnavailable;
        case HttpError::NotFound:
        case HttpError::ClientError:
        case HttpError::AuthError:
        case HttpError::RateLimited:
        case HttpError::InvalidResponse:
        case HttpError::TlsUnsupported:
        default:
            return ForecastError::InvalidResponse;
    }
}

Result<float, ForecastError> OpenMeteoForecast::getTomorrowSunshineHours(
    unsigned long localEpochSeconds) {
    auto body = httpClient_->get(url_.c_str());
    if (body.isError()) {
        return Result<float, ForecastError>::err(mapHttpError(body.unwrapErr()));
    }

    // Filtered parse: only the daily arrays are retained.
    JsonDocument filter;
    filter["daily"]["time"] = true;
    filter["daily"]["sunshine_hours"] = true;

    JsonDocument doc;
    const DeserializationError jsonError =
        deserializeJson(doc, body.unwrap().c_str(),
                        DeserializationOption::Filter(filter));
    if (jsonError != DeserializationError::Ok) {
        return Result<float, ForecastError>::err(ForecastError::InvalidResponse);
    }

    // Tomorrow relative to the current LOCAL day.
    const long tomorrow = (long)(localEpochSeconds / 86400UL) + 1;
    float hours = 0.0f;
    if (!extractSunshineForDay(doc, tomorrow, hours)) {
        return Result<float, ForecastError>::err(ForecastError::InvalidResponse);
    }

    return Result<float, ForecastError>::ok(hours);
}
