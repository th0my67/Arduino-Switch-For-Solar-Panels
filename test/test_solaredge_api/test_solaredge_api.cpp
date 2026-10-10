#include <unity.h>
#include <Arduino.h>
#include <vector>
#include "api/solaredge_api_v2.h"

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

static SolarEdgeApiV2 makeApi(FakeHttpClient& http) {
    return SolarEdgeApiV2("1234567", "TESTKEY123", http, "monitoringapi.solaredge.com");
}

// Realistic v2 power-flow payload (unit kW, like the live API).
static const char kValidKw[] =
    "{\"siteCurrentPowerFlow\":{\"updateRefreshRate\":3,\"unit\":\"kW\","
    "\"connections\":[{\"from\":\"PV\",\"to\":\"Load\"}],"
    "\"GRID\":{\"status\":\"Active\",\"currentPower\":0.5},"
    "\"LOAD\":{\"status\":\"Active\",\"currentPower\":2.25},"
    "\"PV\":{\"status\":\"Active\",\"currentPower\":1.75}}}";

static const char kValidW[] =
    "{\"siteCurrentPowerFlow\":{\"unit\":\"W\","
    "\"PV\":{\"status\":\"Active\",\"currentPower\":5230}}}";

void test_url_contains_site_id_and_api_key(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String(kValidKw)));
    auto api = makeApi(http);
    api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT(1, http.requestedUrls.size());
    const String url = http.requestedUrls[0];
    TEST_ASSERT_TRUE(url.startsWith("https://monitoringapi.solaredge.com/"));
    TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "/v2/sites/1234567/power-flow"));
    TEST_ASSERT_NOT_NULL(strstr(url.c_str(), "api_key=TESTKEY123"));
}

void test_power_converted_from_kw(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String(kValidKw)));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT(1750, r.unwrap()); // 1.75 kW -> 1750 W
}

void test_power_in_watts_untouched(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String(kValidW)));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT(5230, r.unwrap());
}

void test_power_in_megawatts(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":{\"unit\":\"MW\","
               "\"PV\":{\"currentPower\":0.005}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT(5000, r.unwrap());
}

void test_unknown_unit_is_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":{\"unit\":\"horsepower\","
               "\"PV\":{\"currentPower\":7.5}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_missing_unit_is_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":{\"PV\":{\"currentPower\":1.5}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_missing_pv_is_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":{\"unit\":\"kW\","
               "\"LOAD\":{\"currentPower\":0.7}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_null_site_flow_is_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":null}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_string_power_is_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":{\"unit\":\"kW\","
               "\"PV\":{\"currentPower\":\"1.75\"}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_overflowing_kw_is_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":{\"unit\":\"MW\","
               "\"PV\":{\"currentPower\":9.9e9}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_malformed_json_is_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_empty_body_is_rejected(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String("")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_invalid_credentials(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::err(HttpError::AuthError));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::AuthenticationFailure, (int)r.unwrapErr());
}

void test_rate_limited(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::err(HttpError::RateLimited));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::RateLimited, (int)r.unwrapErr());
}

void test_not_found_and_server_error_mapping(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::err(HttpError::NotFound));
    auto api = makeApi(http);
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse,
                          (int)api.getCurrentPower().unwrapErr());
    http.responses.push_back(Result<String, HttpError>::err(HttpError::ServerError));
    TEST_ASSERT_EQUAL_INT((int)InverterError::ServiceUnavailable,
                          (int)api.getCurrentPower().unwrapErr());
    http.responses.push_back(Result<String, HttpError>::err(HttpError::NetworkFailure));
    TEST_ASSERT_EQUAL_INT((int)InverterError::NetworkFailure,
                          (int)api.getCurrentPower().unwrapErr());
}

void test_empty_configuration_fails_fast(void) {
    FakeHttpClient http;
    // Empty site id or API key: no request must even be attempted.
    SolarEdgeApiV2 bad1("", "KEY", http);
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse,
                          (int)bad1.getCurrentPower().unwrapErr());
    SolarEdgeApiV2 bad2("123", "", http);
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse,
                          (int)bad2.getCurrentPower().unwrapErr());
    SolarEdgeApiV2 bad3(nullptr, "KEY", http);
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse,
                          (int)bad3.getCurrentPower().unwrapErr());
    TEST_ASSERT_EQUAL_INT(0, (int)http.requestedUrls.size());
}

void test_status_from_pv_status_string(void) {
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Producing,
                          (int)SolarEdgeApiV2::statusFromPvStatus("Active"));
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Idle,
                          (int)SolarEdgeApiV2::statusFromPvStatus("Idle"));
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Offline,
                          (int)SolarEdgeApiV2::statusFromPvStatus("SomethingElse"));
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Offline,
                          (int)SolarEdgeApiV2::statusFromPvStatus(nullptr));
}

void test_get_status_from_payload(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String(kValidKw)));
    auto api = makeApi(http);
    auto r = api.getStatus();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Producing, (int)r.unwrap());
}

void test_get_status_idle(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":{\"unit\":\"kW\","
               "\"PV\":{\"status\":\"Idle\",\"currentPower\":0.0}}}")));
    auto api = makeApi(http);
    auto r = api.getStatus();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Idle, (int)r.unwrap());
}

void test_get_status_falls_back_to_power(void) {
    FakeHttpClient http;
    // No PV status: status is derived from the power reading.
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":{\"unit\":\"kW\","
               "\"PV\":{\"currentPower\":0.25}}}")));
    auto api = makeApi(http);
    auto r = api.getStatus();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Producing, (int)r.unwrap());

    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"siteCurrentPowerFlow\":{\"unit\":\"kW\","
               "\"PV\":{\"currentPower\":0.0}}}")));
    auto r2 = api.getStatus();
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Idle, (int)r2.unwrap());
}

void test_brand_name(void) {
    FakeHttpClient http;
    auto api = makeApi(http);
    TEST_ASSERT_EQUAL_STRING("SolarEdge", api.getBrandName());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_url_contains_site_id_and_api_key);
    RUN_TEST(test_power_converted_from_kw);
    RUN_TEST(test_power_in_watts_untouched);
    RUN_TEST(test_power_in_megawatts);
    RUN_TEST(test_unknown_unit_is_rejected);
    RUN_TEST(test_missing_unit_is_rejected);
    RUN_TEST(test_missing_pv_is_rejected);
    RUN_TEST(test_null_site_flow_is_rejected);
    RUN_TEST(test_string_power_is_rejected);
    RUN_TEST(test_overflowing_kw_is_rejected);
    RUN_TEST(test_malformed_json_is_rejected);
    RUN_TEST(test_empty_body_is_rejected);
    RUN_TEST(test_invalid_credentials);
    RUN_TEST(test_rate_limited);
    RUN_TEST(test_not_found_and_server_error_mapping);
    RUN_TEST(test_empty_configuration_fails_fast);
    RUN_TEST(test_status_from_pv_status_string);
    RUN_TEST(test_get_status_from_payload);
    RUN_TEST(test_get_status_idle);
    RUN_TEST(test_get_status_falls_back_to_power);
    RUN_TEST(test_brand_name);
    return UNITY_END();
}
