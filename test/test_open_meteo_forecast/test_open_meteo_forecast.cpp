#include <unity.h>
#include <Arduino.h>
#include <vector>
#include "api/open_meteo_forecast.h"

class FakeHttpClient : public IHttpClient {
public:
    std::vector<String> requestedUrls;
    std::vector<Result<String, HttpError>> responses;

    Result<String, HttpError> get(const char* url, unsigned long timeoutMs) override {
        (void)timeoutMs;
        requestedUrls.push_back(String(url));
        if (responses.empty()) {
            return Result<String, HttpError>::err(HttpError::NetworkFailure);
        }
        auto r = responses.front();
        responses.erase(responses.begin());
        return r;
    }
};

void setUp(void) {}
void tearDown(void) {}

static OpenMeteoForecast makeApi(FakeHttpClient& http) {
    return OpenMeteoForecast(46.5, 6.6, "Europe/Zurich", http);
}

// Typical Open-Meteo daily payload. With timezone=Europe/Zurich the
// daily.time entries are local dates.
static const char kPayload[] =
    "{\"latitude\":46.5,\"longitude\":6.6,\"timezone\":\"Europe/Zurich\","
    "\"daily\":{\"time\":[\"2026-10-10\",\"2026-10-11\"],"
    "\"sunshine_hours\":[3.7,2.9]}}";

// Local epoch helper: day number * 86400 + local hour.
static unsigned long epochAtLocalDay(long unixDay, int localHour) {
    return (unsigned long)unixDay * 86400UL + (unsigned long)localHour * 3600UL;
}

void test_url_contains_location_and_parameters(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String(kPayload)));
    auto api = makeApi(http);
    api.getTomorrowSunshineHours(epochAtLocalDay(20736, 10));
    TEST_ASSERT_EQUAL_INT(1, (int)http.requestedUrls.size());
    const String url = http.requestedUrls[0];
    TEST_ASSERT_TRUE(url.startsWith("https://api.open-meteo.com/v1/forecast?"));
    TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "latitude=46.5000"));
    TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "longitude=6.6000"));
    TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "daily=sunshine_hours"));
    TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "forecast_days=2"));
    TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "timezone=Europe/Zurich"));
}

void test_day_number_from_date_string(void) {
    long day = 0;
    TEST_ASSERT_TRUE(OpenMeteoForecast::dayNumberFromDateString("1970-01-01", day));
    TEST_ASSERT_EQUAL_INT(0, day);
    TEST_ASSERT_TRUE(OpenMeteoForecast::dayNumberFromDateString("2000-01-01", day));
    TEST_ASSERT_EQUAL_INT(10957, day);
    TEST_ASSERT_TRUE(OpenMeteoForecast::dayNumberFromDateString("2026-10-10", day));
    TEST_ASSERT_EQUAL_INT(20736, day);
    TEST_ASSERT_TRUE(OpenMeteoForecast::dayNumberFromDateString("2024-02-29", day));
    TEST_ASSERT_EQUAL_INT(19782, day); // leap day
    TEST_ASSERT_TRUE(OpenMeteoForecast::dayNumberFromDateString("2024-03-01", day));
    TEST_ASSERT_EQUAL_INT(19783, day);
}

void test_date_string_validation(void) {
    long day = 12345;
    TEST_ASSERT_FALSE(OpenMeteoForecast::dayNumberFromDateString("", day));
    TEST_ASSERT_FALSE(OpenMeteoForecast::dayNumberFromDateString("2026/10/10", day));
    TEST_ASSERT_FALSE(OpenMeteoForecast::dayNumberFromDateString("2026-13-01", day)); // month 13
    TEST_ASSERT_FALSE(OpenMeteoForecast::dayNumberFromDateString("2026-00-10", day)); // month 0
    TEST_ASSERT_FALSE(OpenMeteoForecast::dayNumberFromDateString("2026-10-00", day)); // day 0
    TEST_ASSERT_FALSE(OpenMeteoForecast::dayNumberFromDateString("2026-10-32", day)); // day 32
    TEST_ASSERT_FALSE(OpenMeteoForecast::dayNumberFromDateString("2026-10-1A", day));
    TEST_ASSERT_FALSE(OpenMeteoForecast::dayNumberFromDateString(nullptr, day));
    TEST_ASSERT_EQUAL_INT(12345, day); // untouched on failure
}

void test_tomorrow_sunshine_extracted(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String(kPayload)));
    auto api = makeApi(http);
    // Today = 2026-10-10 (day 20677): tomorrow's sunshine is 2.9 h.
    auto r = api.getTomorrowSunshineHours(epochAtLocalDay(20736, 18));
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_FLOAT(2.9f, r.unwrap());
}

void test_today_sunshine_is_not_returned(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String(kPayload)));
    auto api = makeApi(http);
    // Today = 2026-10-11 (day 20678): "tomorrow" (20679) is not in the
    // payload (only two days) -> error.
    auto r = api.getTomorrowSunshineHours(epochAtLocalDay(20737, 9));
    TEST_ASSERT_TRUE(r.isError());
    TEST_ASSERT_EQUAL_INT((int)ForecastError::InvalidResponse, (int)r.unwrapErr());
}

void test_null_sunshine_is_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"daily\":{\"time\":[\"2026-10-10\",\"2026-10-11\"],"
               "\"sunshine_hours\":[3.7,null]}}")));
    auto api = makeApi(http);
    auto r = api.getTomorrowSunshineHours(epochAtLocalDay(20736, 18));
    TEST_ASSERT_TRUE(r.isError());
    TEST_ASSERT_EQUAL_INT((int)ForecastError::InvalidResponse, (int)r.unwrapErr());
}

void test_missing_arrays_are_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"daily\":{\"time\":[\"2026-10-10\"]}}")));
    auto api = makeApi(http);
    auto r = api.getTomorrowSunshineHours(epochAtLocalDay(20736, 18));
    TEST_ASSERT_EQUAL_INT((int)ForecastError::InvalidResponse, (int)r.unwrapErr());

    http.responses.push_back(Result<String, HttpError>::ok(String("{\"reason\":\"bad request\"}")));
    auto r2 = api.getTomorrowSunshineHours(epochAtLocalDay(20736, 18));
    TEST_ASSERT_EQUAL_INT((int)ForecastError::InvalidResponse, (int)r2.unwrapErr());
}

void test_malformed_json_is_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String("{\"daily\":")));
    auto api = makeApi(http);
    auto r = api.getTomorrowSunshineHours(epochAtLocalDay(20736, 18));
    TEST_ASSERT_EQUAL_INT((int)ForecastError::InvalidResponse, (int)r.unwrapErr());
}

void test_error_mapping(void) {
    TEST_ASSERT_EQUAL_INT((int)ForecastError::NetworkFailure,
        (int)OpenMeteoForecast::mapHttpError(HttpError::NetworkFailure));
    TEST_ASSERT_EQUAL_INT((int)ForecastError::NetworkFailure,
        (int)OpenMeteoForecast::mapHttpError(HttpError::Timeout));
    TEST_ASSERT_EQUAL_INT((int)ForecastError::ServiceUnavailable,
        (int)OpenMeteoForecast::mapHttpError(HttpError::ServerError));
    TEST_ASSERT_EQUAL_INT((int)ForecastError::InvalidResponse,
        (int)OpenMeteoForecast::mapHttpError(HttpError::RateLimited));
    TEST_ASSERT_EQUAL_INT((int)ForecastError::InvalidResponse,
        (int)OpenMeteoForecast::mapHttpError(HttpError::TlsUnsupported));
}

void test_network_failure_propagates(void) {
    FakeHttpClient http; // no scripted response -> network failure
    auto api = makeApi(http);
    auto r = api.getTomorrowSunshineHours(epochAtLocalDay(20736, 18));
    TEST_ASSERT_EQUAL_INT((int)ForecastError::NetworkFailure, (int)r.unwrapErr());
}

void test_integer_sunshine_value(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"daily\":{\"time\":[\"2026-10-10\",\"2026-10-11\"],"
               "\"sunshine_hours\":[3.7,8]}}")));
    auto api = makeApi(http);
    auto r = api.getTomorrowSunshineHours(epochAtLocalDay(20736, 18));
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_FLOAT(8.0f, r.unwrap());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_url_contains_location_and_parameters);
    RUN_TEST(test_day_number_from_date_string);
    RUN_TEST(test_date_string_validation);
    RUN_TEST(test_tomorrow_sunshine_extracted);
    RUN_TEST(test_today_sunshine_is_not_returned);
    RUN_TEST(test_null_sunshine_is_rejected);
    RUN_TEST(test_missing_arrays_are_rejected);
    RUN_TEST(test_malformed_json_is_rejected);
    RUN_TEST(test_error_mapping);
    RUN_TEST(test_network_failure_propagates);
    RUN_TEST(test_integer_sunshine_value);
    return UNITY_END();
}
