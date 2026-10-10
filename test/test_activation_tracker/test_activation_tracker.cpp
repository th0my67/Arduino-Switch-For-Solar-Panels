#include <unity.h>
#include <Arduino.h>
#include "services/activation_tracker.h"

void setUp(void) {}
void tearDown(void) {}

static const unsigned long kDay = 86400UL;
// Day 20736 (2026-10-10), 10:00 local.
static const unsigned long kBase = 20736UL * 86400UL + 10UL * 3600UL;

void test_first_sample_accumulates_nothing(void) {
    ActivationTracker t;
    t.record(kBase, true); // first sample: baseline only
    TEST_ASSERT_EQUAL_UINT32(0, t.onMsInLastDays(kBase, 2));
    TEST_ASSERT_TRUE(t.wasOnAtLastSample());
}

void test_on_time_accumulates(void) {
    ActivationTracker t;
    t.record(kBase, true);
    t.record(kBase + 90, true);          // 90 s ON
    t.record(kBase + 180, true);         // another 90 s ON
    t.record(kBase + 270, false);        // another 90 s ON
    TEST_ASSERT_EQUAL_UINT32(3UL * 90UL * 1000UL, t.onMsInLastDays(kBase + 270, 2));
}

void test_off_time_does_not_accumulate(void) {
    ActivationTracker t;
    t.record(kBase, false);
    t.record(kBase + 3600, false);
    t.record(kBase + 7200, true); // switches on here
    t.record(kBase + 7300, true);
    TEST_ASSERT_EQUAL_UINT32(100UL * 1000UL, t.onMsInLastDays(kBase + 7300, 2));
}

void test_transition_to_off_credits_the_on_interval(void) {
    ActivationTracker t;
    t.record(kBase, true);
    t.record(kBase + 60, false); // 60 s ON before turning off
    t.record(kBase + 120, false);
    TEST_ASSERT_EQUAL_UINT32(60UL * 1000UL, t.onMsInLastDays(kBase + 120, 2));
}

void test_day_boundary_attributes_to_interval_start_day(void) {
    ActivationTracker t;
    // ON from 23:58 today until 00:02 tomorrow: 240 s to today's bucket.
    const unsigned long beforeMidnight = 20736UL * kDay + 23UL * 3600UL + 58UL * 60UL;
    t.record(beforeMidnight, true);
    t.record(beforeMidnight + 240, false);
    // Today bucket: 240 s.
    TEST_ASSERT_EQUAL_UINT32(240UL * 1000UL, t.onMsInLastDays(20736UL * kDay + 12UL * 3600UL, 1));
    // Two-day window from tomorrow still sees it.
    TEST_ASSERT_EQUAL_UINT32(240UL * 1000UL,
        t.onMsInLastDays(20737UL * kDay + 12UL * 3600UL, 2));
}

void test_history_window_excludes_old_days(void) {
    ActivationTracker t;
    // Two hours ON, two days ago (day 20734).
    t.record(20734UL * kDay + 8UL * 3600UL, true);
    t.record(20734UL * kDay + 10UL * 3600UL, false);
    // Today (day 20736): not ON.
    t.record(20736UL * kDay + 12UL * 3600UL, false);

    // Window of 2 days from day 20736 covers 20735..20736: the 20734
    // bucket is excluded.
    TEST_ASSERT_EQUAL_UINT32(0, t.onMsInLastDays(20736UL * kDay + 12UL * 3600UL, 2));
    // Window of 3 days covers 20734..20736: included.
    TEST_ASSERT_EQUAL_UINT32(2UL * 3600UL * 1000UL,
        t.onMsInLastDays(20736UL * kDay + 12UL * 3600UL, 3));
}

void test_long_gap_is_capped(void) {
    ActivationTracker t;
    // ON at day 0 hour 10, next sample two days later while still ON:
    // only 24 h credited, not 40 h.
    t.record(20736UL * kDay + 10UL * 3600UL, true);
    t.record(20738UL * kDay + 2UL * 3600UL, true);
    // The capped 24 h went to day 20736's bucket; the 2-day window from
    // 20738 covers 20737..20738 and does not see it.
    TEST_ASSERT_EQUAL_UINT32(0, t.onMsInLastDays(20738UL * kDay + 2UL * 3600UL, 2));
    TEST_ASSERT_EQUAL_UINT32(24UL * 3600UL * 1000UL,
        t.onMsInLastDays(20738UL * kDay + 2UL * 3600UL, 4));
}

void test_multiple_days_sum(void) {
    ActivationTracker t;
    t.record(20735UL * kDay + 13UL * 3600UL, true);
    t.record(20735UL * kDay + 14UL * 3600UL, false); // 1 h on day 20735
    t.record(20736UL * kDay + 13UL * 3600UL, true);
    t.record(20736UL * kDay + 14UL * 3600UL, false); // 1 h on day 20736
    TEST_ASSERT_EQUAL_UINT32(2UL * 3600UL * 1000UL,
        t.onMsInLastDays(20736UL * kDay + 20UL * 3600UL, 2));
}

void test_zero_days_window(void) {
    ActivationTracker t;
    t.record(kBase, true);
    t.record(kBase + 3600, false);
    TEST_ASSERT_EQUAL_UINT32(0, t.onMsInLastDays(kBase, 0));
}

void test_ring_reuse_keeps_recent_days(void) {
    ActivationTracker t;
    // Record ON time on 10 consecutive days: the tracker keeps only the
    // most recent ones but never loses today's data.
    for (long day = 20730; day <= 20739; ++day) {
        t.record((unsigned long)day * kDay + 9UL * 3600UL, true);
        t.record((unsigned long)day * kDay + 10UL * 3600UL, false); // 1 h/day
    }
    TEST_ASSERT_EQUAL_UINT32(1UL * 3600UL * 1000UL,
        t.onMsInLastDays(20739UL * kDay + 12UL * 3600UL, 1));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_accumulates_nothing);
    RUN_TEST(test_on_time_accumulates);
    RUN_TEST(test_off_time_does_not_accumulate);
    RUN_TEST(test_transition_to_off_credits_the_on_interval);
    RUN_TEST(test_day_boundary_attributes_to_interval_start_day);
    RUN_TEST(test_history_window_excludes_old_days);
    RUN_TEST(test_long_gap_is_capped);
    RUN_TEST(test_multiple_days_sum);
    RUN_TEST(test_zero_days_window);
    RUN_TEST(test_ring_reuse_keeps_recent_days);
    return UNITY_END();
}
