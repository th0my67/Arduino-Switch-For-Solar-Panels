#include <unity.h>
#include <Arduino.h>
#include <vector>
#include "api/fronius_solar_api_v1.h"

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

static FroniusSolarApiV1 makeApi(FakeHttpClient& http) {
    return FroniusSolarApiV1(IPAddress(192, 168, 1, 161), http);
}

static const char kValidBody[] =
    "{\"Body\":{\"Data\":{\"PAC\":{\"Value\":5230,\"Unit\":\"W\",\"Timestamp\":\"x\"}}},"
    "\"Head\":{\"RequestId\":\"1\"}}";

void test_valid_power_reading(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String(kValidBody)));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT(5230, r.unwrap());
    // URL must contain host, port default and endpoint with query.
    TEST_ASSERT_NOT_NULL(strstr(http.requestedUrls[0].c_str(), "http://192.168.1.161/solar_api/v1/GetInverterRealtimeData.cgi"));
    TEST_ASSERT_NOT_NULL(strstr(http.requestedUrls[0].c_str(), "DataCollection=CumulationInverterData"));
}

void test_float_power_value(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"Body\":{\"Data\":{\"PAC\":{\"Value\":5230.7}}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT(5230, r.unwrap());
}

void test_negative_power_is_reported(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"Body\":{\"Data\":{\"PAC\":{\"Value\":-42}}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT(-42, r.unwrap());
}

void test_missing_pac_is_invalid(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"Body\":{\"Data\":{\"DAY_ENERGY\":{\"Value\":1}}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_null_data_device_unknown(void) {
    // Fronius answers {"Body":{"Data":null}} for an unknown device id.
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"Body\":{\"Data\":null}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_string_pac_is_invalid(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"Body\":{\"Data\":{\"PAC\":{\"Value\":\"5230\"}}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_null_pac_is_invalid(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"Body\":{\"Data\":{\"PAC\":{\"Value\":null}}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_huge_double_is_invalid(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"Body\":{\"Data\":{\"PAC\":{\"Value\":9.9e17}}}}")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_malformed_json_is_invalid(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String("{\"Body\":{\"Data\":")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_html_error_page_is_invalid(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("<html><body>Not the API</body></html>")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_empty_body_is_invalid(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(String("")));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse, (int)r.unwrapErr());
}

void test_http_error_mapping(void) {
    TEST_ASSERT_EQUAL_INT((int)InverterError::NetworkFailure,
        (int)FroniusSolarApiV1::mapHttpError(HttpError::NetworkFailure));
    TEST_ASSERT_EQUAL_INT((int)InverterError::NetworkFailure,
        (int)FroniusSolarApiV1::mapHttpError(HttpError::Timeout));
    TEST_ASSERT_EQUAL_INT((int)InverterError::ServiceUnavailable,
        (int)FroniusSolarApiV1::mapHttpError(HttpError::ServerError));
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse,
        (int)FroniusSolarApiV1::mapHttpError(HttpError::NotFound));
    TEST_ASSERT_EQUAL_INT((int)InverterError::InvalidResponse,
        (int)FroniusSolarApiV1::mapHttpError(HttpError::InvalidResponse));
}

void test_network_failure_propagates(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::err(HttpError::NetworkFailure));
    auto api = makeApi(http);
    auto r = api.getCurrentPower();
    TEST_ASSERT_EQUAL_INT((int)InverterError::NetworkFailure, (int)r.unwrapErr());
}

void test_status_from_power(void) {
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Producing,
        (int)FroniusSolarApiV1::statusFromPower(1));
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Idle,
        (int)FroniusSolarApiV1::statusFromPower(0));
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Error,
        (int)FroniusSolarApiV1::statusFromPower(-1));
}

void test_get_status_producing(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"Body\":{\"Data\":{\"PAC\":{\"Value\":100}}}}")));
    auto api = makeApi(http);
    auto r = api.getStatus();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Producing, (int)r.unwrap());
}

void test_get_status_idle(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::ok(
        String("{\"Body\":{\"Data\":{\"PAC\":{\"Value\":0}}}}")));
    auto api = makeApi(http);
    auto r = api.getStatus();
    TEST_ASSERT_EQUAL_INT((int)InverterStatus::Idle, (int)r.unwrap());
}

void test_get_status_propagates_error(void) {
    FakeHttpClient http;
    http.responses.push_back(Result<String, HttpError>::err(HttpError::ServerError));
    auto api = makeApi(http);
    auto r = api.getStatus();
    TEST_ASSERT_EQUAL_INT((int)InverterError::ServiceUnavailable, (int)r.unwrapErr());
}

void test_brand_name(void) {
    FakeHttpClient http;
    auto api = makeApi(http);
    TEST_ASSERT_EQUAL_STRING("Fronius", api.getBrandName());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_power_reading);
    RUN_TEST(test_float_power_value);
    RUN_TEST(test_negative_power_is_reported);
    RUN_TEST(test_missing_pac_is_invalid);
    RUN_TEST(test_null_data_device_unknown);
    RUN_TEST(test_string_pac_is_invalid);
    RUN_TEST(test_null_pac_is_invalid);
    RUN_TEST(test_huge_double_is_invalid);
    RUN_TEST(test_malformed_json_is_invalid);
    RUN_TEST(test_html_error_page_is_invalid);
    RUN_TEST(test_empty_body_is_invalid);
    RUN_TEST(test_http_error_mapping);
    RUN_TEST(test_network_failure_propagates);
    RUN_TEST(test_status_from_power);
    RUN_TEST(test_get_status_producing);
    RUN_TEST(test_get_status_idle);
    RUN_TEST(test_get_status_propagates_error);
    RUN_TEST(test_brand_name);
    return UNITY_END();
}
