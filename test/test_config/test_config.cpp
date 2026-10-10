#include <unity.h>
#include "config.h"

// This suite includes src/config.h in the NATIVE build. Its purpose is
// twofold:
//  1. prove that config.h compiles standalone (secrets fallback, no
//     missing includes);
//  2. evaluate the compile-time invariants (static_assert) on the host,
//     since main.cpp (their other inclusion point) is excluded from the
//     native test environment. A violated invariant fails this build.
//
// The runtime assertions below mirror the static_asserts so a failure is
// reported as a normal test result too.

void setUp(void) {}
void tearDown(void) {}

void test_hours_are_in_valid_ranges(void) {
    TEST_ASSERT_TRUE(config::SLEEP_START_HOUR >= 1 &&
                     config::SLEEP_START_HOUR <= 23);
    TEST_ASSERT_TRUE(config::BACKUP_START_HOUR <= 23);
    TEST_ASSERT_TRUE(config::BACKUP_CHARGE_DURATION_H >= 1);
    TEST_ASSERT_TRUE(config::WEATHER_FETCH_HOUR <= 23);
}

void test_backup_window_ends_before_night_sleep(void) {
    TEST_ASSERT_TRUE((long)config::BACKUP_START_HOUR +
                         (long)config::BACKUP_CHARGE_DURATION_H <=
                     (long)config::SLEEP_START_HOUR);
}

void test_weather_fetch_happens_before_night_sleep(void) {
    TEST_ASSERT_TRUE(config::WEATHER_FETCH_HOUR < config::SLEEP_START_HOUR);
}

void test_hysteresis_thresholds_are_ordered(void) {
    TEST_ASSERT_TRUE(config::POWER_OFF_THRESHOLD_W < config::POWER_ON_THRESHOLD_W);
}

void test_history_window_fits_the_tracker_capacity(void) {
    TEST_ASSERT_TRUE(config::BACKUP_HISTORY_DAYS >= 1);
    TEST_ASSERT_TRUE(config::BACKUP_HISTORY_DAYS <= ActivationTracker::kMaxDays);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_hours_are_in_valid_ranges);
    RUN_TEST(test_backup_window_ends_before_night_sleep);
    RUN_TEST(test_weather_fetch_happens_before_night_sleep);
    RUN_TEST(test_hysteresis_thresholds_are_ordered);
    RUN_TEST(test_history_window_fits_the_tracker_capacity);
    return UNITY_END();
}
