#include <unity.h>
#include <Arduino.h>
#include <vector>
#include "services/power_monitor.h"

class FakeInverterApi : public IInverterAPI {
public:
    std::vector<Result<int, InverterError>> scripted;
    int calls = 0;

    Result<int, InverterError> getCurrentPower() override {
        ++calls;
        if (scripted.empty()) return Result<int, InverterError>::err(InverterError::NetworkFailure);
        auto r = scripted.front();
        scripted.erase(scripted.begin());
        return r;
    }
    Result<InverterStatus, InverterError> getStatus() override {
        return Result<InverterStatus, InverterError>::err(InverterError::InvalidResponse);
    }
    const char* getBrandName() const override { return "Fake"; }
};

static uint32_t g_waited = 0;
static void recordWait(uint32_t ms) { g_waited += ms; stubs::advanceMillis(ms); }

void setUp(void) {
    stubs::resetStubs();
    g_waited = 0;
}
void tearDown(void) {}

void test_success_first_try_no_wait(void) {
    auto* api = new FakeInverterApi();
    api->scripted.push_back(Result<int, InverterError>::ok(5230));
    PowerMonitor monitor(std::unique_ptr<IInverterAPI>(api), 3, 500, recordWait);
    auto r = monitor.getOutputPower();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT(5230, r.unwrap());
    TEST_ASSERT_EQUAL_INT(1, api->calls);
    TEST_ASSERT_EQUAL_UINT32(0, g_waited);
    TEST_ASSERT_EQUAL_INT(5230, monitor.lastSuccessfulReading());
}

void test_retry_then_success(void) {
    auto* api = new FakeInverterApi();
    api->scripted.push_back(Result<int, InverterError>::err(InverterError::NetworkFailure));
    api->scripted.push_back(Result<int, InverterError>::ok(4200));
    PowerMonitor monitor(std::unique_ptr<IInverterAPI>(api), 3, 500, recordWait);
    auto r = monitor.getOutputPower();
    TEST_ASSERT_TRUE(r.isOk());
    TEST_ASSERT_EQUAL_INT(4200, r.unwrap());
    TEST_ASSERT_EQUAL_INT(2, api->calls);
    TEST_ASSERT_EQUAL_UINT32(500, g_waited); // one backoff of 500 ms
}

void test_permanent_failure_is_bounded(void) {
    auto* api = new FakeInverterApi(); // every call fails (empty script)
    PowerMonitor monitor(std::unique_ptr<IInverterAPI>(api), 3, 500, recordWait);
    auto r = monitor.getOutputPower();
    TEST_ASSERT_TRUE(r.isError());
    TEST_ASSERT_EQUAL_INT((int)PowerError::NetworkDown, (int)r.unwrapErr());
    // 1 initial attempt + 3 retries, then stop: never infinite.
    TEST_ASSERT_EQUAL_INT(4, api->calls);
    // Backoff: 500 + 1000 + 1500 ms.
    TEST_ASSERT_EQUAL_UINT32(3000, g_waited);
}

void test_zero_retries(void) {
    auto* api = new FakeInverterApi();
    PowerMonitor monitor(std::unique_ptr<IInverterAPI>(api), 0, 500, recordWait);
    auto r = monitor.getOutputPower();
    TEST_ASSERT_TRUE(r.isError());
    TEST_ASSERT_EQUAL_INT(1, api->calls); // single attempt, no retry
    TEST_ASSERT_EQUAL_UINT32(0, g_waited);
}

void test_retries_reset_after_success(void) {
    auto* api = new FakeInverterApi();
    api->scripted.push_back(Result<int, InverterError>::err(InverterError::NetworkFailure));
    api->scripted.push_back(Result<int, InverterError>::ok(1));
    PowerMonitor monitor(std::unique_ptr<IInverterAPI>(api), 2, 100, recordWait);
    TEST_ASSERT_TRUE(monitor.getOutputPower().isOk());
    TEST_ASSERT_EQUAL_INT(2, api->calls);

    // Second call gets a fresh retry budget: two configured retries
    // allow three failing attempts before giving up.
    api->scripted.push_back(Result<int, InverterError>::err(InverterError::NetworkFailure));
    api->scripted.push_back(Result<int, InverterError>::err(InverterError::InvalidResponse));
    api->scripted.push_back(Result<int, InverterError>::err(InverterError::ServiceUnavailable));
    auto r2 = monitor.getOutputPower();
    TEST_ASSERT_TRUE(r2.isError());
    TEST_ASSERT_EQUAL_INT((int)PowerError::NetworkDown, (int)r2.unwrapErr());
    TEST_ASSERT_EQUAL_INT(5, api->calls); // 2 + 3
    // Last successful reading is preserved across the failure.
    TEST_ASSERT_EQUAL_INT(1, monitor.lastSuccessfulReading());
}

void test_error_mapping(void) {
    TEST_ASSERT_EQUAL_INT((int)PowerError::NetworkDown,
        (int)PowerMonitor::mapError(InverterError::NetworkFailure));
    TEST_ASSERT_EQUAL_INT((int)PowerError::NetworkDown,
        (int)PowerMonitor::mapError(InverterError::AuthenticationFailure));
    TEST_ASSERT_EQUAL_INT((int)PowerError::NetworkDown,
        (int)PowerMonitor::mapError(InverterError::ServiceUnavailable));
    TEST_ASSERT_EQUAL_INT((int)PowerError::ParseError,
        (int)PowerMonitor::mapError(InverterError::InvalidResponse));
    TEST_ASSERT_EQUAL_INT((int)PowerError::Timeout,
        (int)PowerMonitor::mapError(InverterError::RateLimited));
}

void test_caches_last_successful_reading(void) {
    auto* api = new FakeInverterApi();
    api->scripted.push_back(Result<int, InverterError>::ok(7777));
    PowerMonitor monitor(std::unique_ptr<IInverterAPI>(api), 3, 500, recordWait);
    TEST_ASSERT_EQUAL_INT(-1, monitor.lastSuccessfulReading()); // before
    stubs::advanceMillis(100); // reading happens at t = 100 ms
    TEST_ASSERT_TRUE(monitor.getOutputPower().isOk());
    TEST_ASSERT_EQUAL_INT(7777, monitor.lastSuccessfulReading());
    TEST_ASSERT_EQUAL_UINT32(100, monitor.lastSuccessfulReadingMs());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_success_first_try_no_wait);
    RUN_TEST(test_retry_then_success);
    RUN_TEST(test_permanent_failure_is_bounded);
    RUN_TEST(test_zero_retries);
    RUN_TEST(test_retries_reset_after_success);
    RUN_TEST(test_error_mapping);
    RUN_TEST(test_caches_last_successful_reading);
    return UNITY_END();
}
