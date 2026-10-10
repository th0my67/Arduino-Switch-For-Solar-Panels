#include <unity.h>
#include <Arduino.h>
#include "switch_controller.h"

void setUp(void) { stubs::resetStubs(); }
void tearDown(void) {}

constexpr uint8_t kPin = 6;
constexpr int kOn = 5000;
constexpr int kOff = 4500;
constexpr uint32_t kMinOn = 300000;   // 5 min
constexpr uint32_t kMinOff = 300000;  // 5 min
constexpr uint8_t kFailLimit = 3;

static SwitchController makeController(uint8_t failLimit = kFailLimit) {
    return SwitchController(kPin, kOn, kOff, kMinOn, kMinOff, failLimit);
}

// Time is injected: use the fake millis clock as the time domain.
static void applyPower(SwitchController& c, int watts) {
    c.applyReading(Result<int, PowerError>::ok(watts), millis());
}

static void applyError(SwitchController& c, PowerError e) {
    c.applyReading(Result<int, PowerError>::err(e), millis());
}

void test_begin_sets_pin_off(void) {
    auto c = makeController();
    c.begin(millis());
    TEST_ASSERT_EQUAL_INT(OUTPUT, stubs::pinModeSet(kPin));
    TEST_ASSERT_EQUAL_INT(LOW, stubs::pinState(kPin));
    TEST_ASSERT_FALSE(c.isOn());
}

void test_on_requires_min_off_time_elapsed(void) {
    auto c = makeController();
    c.begin(millis());
    // High power right after begin: min OFF time not elapsed -> stays OFF.
    applyPower(c, 6000);
    TEST_ASSERT_FALSE(c.isOn());
    // After the minimum OFF time: switches ON.
    stubs::advanceMillis(kMinOff);
    applyPower(c, 6000);
    TEST_ASSERT_TRUE(c.isOn());
    TEST_ASSERT_EQUAL_INT(HIGH, stubs::pinState(kPin));
}

void test_off_requires_min_on_time_elapsed(void) {
    auto c = makeController();
    c.begin(millis());
    stubs::advanceMillis(kMinOff);
    applyPower(c, 6000);
    TEST_ASSERT_TRUE(c.isOn());
    // Low power immediately: min ON time not elapsed -> stays ON.
    applyPower(c, 100);
    TEST_ASSERT_TRUE(c.isOn());
    stubs::advanceMillis(kMinOn);
    applyPower(c, 100);
    TEST_ASSERT_FALSE(c.isOn());
    TEST_ASSERT_EQUAL_INT(LOW, stubs::pinState(kPin));
}

void test_hysteresis_band_holds_state(void) {
    auto c = makeController();
    c.begin(millis());
    stubs::advanceMillis(kMinOff);
    applyPower(c, 6000);
    TEST_ASSERT_TRUE(c.isOn());
    // 4600 W is between 4500 and 5000: holds ON, whatever min ON time says.
    stubs::advanceMillis(kMinOn);
    applyPower(c, 4600);
    TEST_ASSERT_TRUE(c.isOn());
    // Below the OFF threshold, min ON time elapsed: switches OFF.
    applyPower(c, 100);
    TEST_ASSERT_FALSE(c.isOn());
    // From OFF, the same in-band reading holds OFF (no toggle flapping).
    stubs::advanceMillis(kMinOff);
    applyPower(c, 4600);
    TEST_ASSERT_FALSE(c.isOn());
    applyPower(c, 4900);
    TEST_ASSERT_FALSE(c.isOn());
}

void test_off_at_threshold_boundary(void) {
    auto c = makeController();
    c.begin(millis());
    stubs::advanceMillis(kMinOff);
    applyPower(c, 5000); // ON at exactly the ON threshold
    TEST_ASSERT_TRUE(c.isOn());
    stubs::advanceMillis(kMinOn);
    applyPower(c, 4500); // OFF at exactly the OFF threshold
    TEST_ASSERT_FALSE(c.isOn());
}

void test_single_error_holds_state(void) {
    auto c = makeController();
    c.begin(millis());
    stubs::advanceMillis(kMinOff);
    applyPower(c, 6000);
    TEST_ASSERT_TRUE(c.isOn());
    applyError(c, PowerError::NetworkDown);
    TEST_ASSERT_TRUE(c.isOn()); // transient failure does not switch off
    TEST_ASSERT_EQUAL_INT(1, c.consecutiveFailures());
    applyPower(c, 6000); // success resets the failure counter
    TEST_ASSERT_EQUAL_INT(0, c.consecutiveFailures());
}

void test_consecutive_failures_force_off(void) {
    auto c = makeController();
    c.begin(millis());
    stubs::advanceMillis(kMinOff);
    applyPower(c, 6000);
    TEST_ASSERT_TRUE(c.isOn());
    applyError(c, PowerError::NetworkDown);
    applyError(c, PowerError::NetworkDown);
    TEST_ASSERT_TRUE(c.isOn()); // under the limit
    applyError(c, PowerError::NetworkDown);
    TEST_ASSERT_FALSE(c.isOn()); // at the limit: fail-safe OFF
    TEST_ASSERT_EQUAL_INT(LOW, stubs::pinState(kPin));
}

void test_force_off(void) {
    auto c = makeController();
    c.begin(millis());
    stubs::advanceMillis(kMinOff);
    applyPower(c, 6000);
    TEST_ASSERT_TRUE(c.isOn());
    c.forceOff(millis());
    TEST_ASSERT_FALSE(c.isOn());
    TEST_ASSERT_EQUAL_INT(LOW, stubs::pinState(kPin));
    // Already off: idempotent, timestamp untouched.
    stubs::advanceMillis(50);
    const uint32_t before = c.timeInCurrentStateMs(millis());
    c.forceOff(millis());
    TEST_ASSERT_TRUE(c.timeInCurrentStateMs(millis()) >= before);
}

void test_time_rollover_is_safe(void) {
    // Inject a time domain just before the 32-bit wrap, as the RTC-based
    // epoch*1000 value does every ~49.7 days.
    uint32_t nowMs = 0xFFFFFFFFUL - 1000UL;
    auto c = makeController();
    c.begin(nowMs);
    c.applyReading(Result<int, PowerError>::ok(6000), nowMs);
    TEST_ASSERT_FALSE(c.isOn()); // no min OFF time elapsed
    // Advance past min OFF time, crossing the wrap.
    nowMs += kMinOff + 100;
    c.applyReading(Result<int, PowerError>::ok(6000), nowMs);
    TEST_ASSERT_TRUE(c.isOn()); // elapsed computed across the wrap
    c.applyReading(Result<int, PowerError>::ok(100), nowMs); // min ON not elapsed
    TEST_ASSERT_TRUE(c.isOn());
    nowMs += kMinOn;
    c.applyReading(Result<int, PowerError>::ok(100), nowMs);
    TEST_ASSERT_FALSE(c.isOn());
}

void test_inverted_thresholds_are_normalized(void) {
    // on < off is a misconfiguration; the controller must still hysteres.
    // Normalized: on = mid+1 = 4751, off = mid = 4750.
    SwitchController c(kPin, 4500, 5000, 0, 0, 3);
    c.begin(millis());
    applyPower(c, 4500);
    TEST_ASSERT_FALSE(c.isOn()); // below normalized ON threshold
    applyPower(c, 4751);
    TEST_ASSERT_TRUE(c.isOn());
    applyPower(c, 4750);
    TEST_ASSERT_FALSE(c.isOn()); // at normalized OFF threshold
}

void test_time_in_current_state(void) {
    auto c = makeController();
    c.begin(1000);
    TEST_ASSERT_EQUAL_UINT32(0, c.timeInCurrentStateMs(1000));
    TEST_ASSERT_EQUAL_UINT32(9000, c.timeInCurrentStateMs(10000));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_sets_pin_off);
    RUN_TEST(test_on_requires_min_off_time_elapsed);
    RUN_TEST(test_off_requires_min_on_time_elapsed);
    RUN_TEST(test_hysteresis_band_holds_state);
    RUN_TEST(test_off_at_threshold_boundary);
    RUN_TEST(test_single_error_holds_state);
    RUN_TEST(test_consecutive_failures_force_off);
    RUN_TEST(test_force_off);
    RUN_TEST(test_time_rollover_is_safe);
    RUN_TEST(test_inverted_thresholds_are_normalized);
    RUN_TEST(test_time_in_current_state);
    return UNITY_END();
}
