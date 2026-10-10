#include <unity.h>
#include <Arduino.h>
#include <vector>
#include "services/backup_charge.h"

// Scripted forecast provider.
class FakeForecastProvider : public IForecastProvider {
public:
    std::vector<Result<float, ForecastError>> scripted;
    int calls = 0;

    Result<float, ForecastError> getTomorrowSunshineHours(
        unsigned long localEpochSeconds) override {
        (void)localEpochSeconds;
        ++calls;
        if (scripted.empty()) {
            return Result<float, ForecastError>::err(ForecastError::NetworkFailure);
        }
        auto r = scripted.front();
        scripted.erase(scripted.begin());
        return r;
    }
};

static BackupChargeConfig makeConfig() {
    BackupChargeConfig c;
    c.minActivationHours = 2.0f;       // boiler must have been ON >= 2 h
    c.historyDays = 2;
    c.sunshineThresholdHours = 4.0f;   // tomorrow below 4 h sunshine -> charge
    c.startHour = 13;
    c.chargeDurationHours = 3;
    c.fetchHour = 18;
    return c;
}

static const unsigned long kDay = 86400UL;
static unsigned long at(long day, int hour, int minute = 0) {
    return (unsigned long)day * kDay + (unsigned long)hour * 3600UL +
           (unsigned long)minute * 60UL;
}

void setUp(void) {}
void tearDown(void) {}

static void heatDay(ActivationTracker& tracker, long day, unsigned long secondsOn,
                    unsigned long startHour = 12) {
    tracker.record(at(day, startHour), true);
    tracker.record(at(day, startHour) + secondsOn, false);
}

void test_schedule_created_when_cold_and_gloomy(void) {
    FakeForecastProvider provider;
    provider.scripted.push_back(Result<float, ForecastError>::ok(1.5f));
    auto config = makeConfig();
    BackupChargeController backup(provider, config);
    ActivationTracker tracker;
    // Less than 2 h of ON time over the last 2 days.
    tracker.record(at(20736, 12), true);
    tracker.record(at(20736, 12) + 3600, false);

    backup.tick(at(20736, 18), tracker);
    TEST_ASSERT_TRUE(backup.isScheduled());
    // Tomorrow = day 20737, 13:00 local, for 3 h.
    TEST_ASSERT_EQUAL_UINT32(at(20737, 13), backup.scheduledStartEpoch());
    TEST_ASSERT_EQUAL_UINT32(at(20737, 16), backup.scheduledEndEpoch());
    TEST_ASSERT_EQUAL_INT(1, provider.calls);
}

void test_no_schedule_when_boiler_already_heated(void) {
    FakeForecastProvider provider;
    provider.scripted.push_back(Result<float, ForecastError>::ok(1.5f));
    BackupChargeController backup(provider, makeConfig());
    ActivationTracker tracker;
    // 3 h ON: above the 2 h requirement.
    tracker.record(at(20736, 12), true);
    tracker.record(at(20736, 12) + 3UL * 3600UL, false);

    backup.tick(at(20736, 18), tracker);
    TEST_ASSERT_FALSE(backup.isScheduled());
}

void test_no_schedule_when_forecast_is_sunny(void) {
    FakeForecastProvider provider;
    provider.scripted.push_back(Result<float, ForecastError>::ok(6.0f));
    BackupChargeController backup(provider, makeConfig());
    ActivationTracker tracker;
    tracker.record(at(20736, 12), true);
    tracker.record(at(20736, 12) + 3600, false); // cold boiler

    backup.tick(at(20736, 18), tracker);
    TEST_ASSERT_FALSE(backup.isScheduled()); // but tomorrow is sunny
}

void test_forced_window_active_only_between_start_and_end(void) {
    FakeForecastProvider provider;
    provider.scripted.push_back(Result<float, ForecastError>::ok(1.5f));
    BackupChargeController backup(provider, makeConfig());
    ActivationTracker tracker;
    tracker.record(at(20736, 12), true);
    tracker.record(at(20736, 12) + 3600, false);

    backup.tick(at(20736, 18), tracker);
    TEST_ASSERT_TRUE(backup.isScheduled());
    // Before the window.
    TEST_ASSERT_FALSE(backup.shouldForceOn(at(20737, 12, 59)));
    // Within the window.
    TEST_ASSERT_TRUE(backup.shouldForceOn(at(20737, 13, 0)));
    TEST_ASSERT_TRUE(backup.shouldForceOn(at(20737, 15, 59)));
    // At the end (exclusive).
    TEST_ASSERT_FALSE(backup.shouldForceOn(at(20737, 16, 0)));
}

void test_window_is_dropped_after_it_ends(void) {
    FakeForecastProvider provider;
    BackupChargeController backup(provider, makeConfig());
    ActivationTracker tracker;
    tracker.record(at(20736, 12), true);
    tracker.record(at(20736, 12) + 3600, false);

    provider.scripted.push_back(Result<float, ForecastError>::ok(1.5f));
    backup.tick(at(20736, 18), tracker);
    TEST_ASSERT_TRUE(backup.isScheduled());

    // Time passes beyond the window: tick clears the schedule.
    backup.tick(at(20737, 17), tracker);
    TEST_ASSERT_FALSE(backup.isScheduled());
    TEST_ASSERT_FALSE(backup.shouldForceOn(at(20737, 17)));
}

void test_evaluation_runs_once_per_day(void) {
    FakeForecastProvider provider;
    provider.scripted.push_back(Result<float, ForecastError>::ok(1.5f));
    BackupChargeController backup(provider, makeConfig());
    ActivationTracker tracker;

    backup.tick(at(20736, 18), tracker);
    TEST_ASSERT_EQUAL_INT(1, provider.calls);
    // A second tick the same day: no new fetch, even outside the fetch
    // hour, and the provider would fail (empty script) if called.
    backup.tick(at(20736, 19), tracker);
    backup.tick(at(20736, 20), tracker);
    TEST_ASSERT_EQUAL_INT(1, provider.calls);
    TEST_ASSERT_EQUAL_INT(0, provider.scripted.size());
}

void test_failed_forecast_is_retried_and_never_forces(void) {
    FakeForecastProvider provider; // always fails (empty script)
    BackupChargeController backup(provider, makeConfig());
    ActivationTracker tracker;
    tracker.record(at(20736, 12), true);
    tracker.record(at(20736, 12) + 3600, false);

    backup.tick(at(20736, 18, 0), tracker);
    TEST_ASSERT_EQUAL_INT(1, provider.calls);
    TEST_ASSERT_FALSE(backup.isScheduled());
    // Retried after the 15 min throttle...
    backup.tick(at(20736, 18, 20), tracker);
    TEST_ASSERT_EQUAL_INT(2, provider.calls);
    // ...but not before it.
    backup.tick(at(20736, 18, 25), tracker);
    TEST_ASSERT_EQUAL_INT(2, provider.calls);
    // After the fetch hour: no more attempts until the next day.
    backup.tick(at(20736, 20), tracker);
    TEST_ASSERT_EQUAL_INT(2, provider.calls);
    TEST_ASSERT_FALSE(backup.isScheduled());
}

void test_fetch_only_happens_in_the_fetch_hour(void) {
    FakeForecastProvider provider;
    BackupChargeController backup(provider, makeConfig());
    ActivationTracker tracker;

    backup.tick(at(20736, 17), tracker);
    backup.tick(at(20736, 19), tracker);
    TEST_ASSERT_EQUAL_INT(0, provider.calls); // wrong hours: no fetch
}

void test_invalid_clock_does_nothing(void) {
    FakeForecastProvider provider;
    BackupChargeController backup(provider, makeConfig());
    ActivationTracker tracker;

    backup.tick(0, tracker); // clock invalid
    TEST_ASSERT_EQUAL_INT(0, provider.calls);
    TEST_ASSERT_FALSE(backup.isScheduled());
    TEST_ASSERT_FALSE(backup.shouldForceOn(0));
}

void test_no_forcing_before_first_evaluation(void) {
    FakeForecastProvider provider;
    BackupChargeController backup(provider, makeConfig());
    // Fresh boot, clock valid: nothing scheduled, nothing forced.
    TEST_ASSERT_FALSE(backup.shouldForceOn(at(20736, 13)));
    TEST_ASSERT_FALSE(backup.shouldForceOn(at(20737, 13)));
}

void test_next_day_can_schedule_again(void) {
    FakeForecastProvider provider;
    BackupChargeController backup(provider, makeConfig());
    ActivationTracker tracker;
    tracker.record(at(20736, 12), true);
    tracker.record(at(20736, 12) + 3600, false);

    provider.scripted.push_back(Result<float, ForecastError>::ok(1.5f));
    backup.tick(at(20736, 18), tracker);
    TEST_ASSERT_TRUE(backup.isScheduled());

    // Day 2: yesterday's window has expired (tick clears it), tomorrow
    // is sunny -> no new schedule.
    provider.scripted.push_back(Result<float, ForecastError>::ok(9.0f));
    backup.tick(at(20737, 18), tracker);
    TEST_ASSERT_FALSE(backup.isScheduled());

    // Day 3: schedule from day 1 has expired; cold + gloomy -> reschedule.
    tracker.record(at(20738, 12), true);
    tracker.record(at(20738, 12) + 3600, false);
    provider.scripted.push_back(Result<float, ForecastError>::ok(1.5f));
    backup.tick(at(20738, 18), tracker);
    TEST_ASSERT_EQUAL_UINT32(at(20739, 13), backup.scheduledStartEpoch());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_schedule_created_when_cold_and_gloomy);
    RUN_TEST(test_no_schedule_when_boiler_already_heated);
    RUN_TEST(test_no_schedule_when_forecast_is_sunny);
    RUN_TEST(test_forced_window_active_only_between_start_and_end);
    RUN_TEST(test_window_is_dropped_after_it_ends);
    RUN_TEST(test_evaluation_runs_once_per_day);
    RUN_TEST(test_failed_forecast_is_retried_and_never_forces);
    RUN_TEST(test_fetch_only_happens_in_the_fetch_hour);
    RUN_TEST(test_invalid_clock_does_nothing);
    RUN_TEST(test_no_forcing_before_first_evaluation);
    RUN_TEST(test_next_day_can_schedule_again);
    return UNITY_END();
}
