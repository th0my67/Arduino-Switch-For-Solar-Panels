#include <unity.h>
#include <Arduino.h>
#include <cstring>
#include "time_manager.h"

// Fake UDP: answers one canned NTP response.
class FakeUdp : public UDP {
public:
    std::string sent;
    int beginCalls = 0;
    int stopCalls = 0;
    uint8_t beginResult = 1;
    int parsePacketResult = 0;
    std::string response;
    size_t responseLen = 0;
    std::string lastHost;

    uint8_t begin(uint16_t) override { ++beginCalls; return beginResult; }
    void stop() override { ++stopCalls; }
    int beginPacket(const char* host, uint16_t) override {
        lastHost = host ? host : "";
        return 1;
    }
    int beginPacket(IPAddress, uint16_t) override { return 1; }
    int endPacket() override { return 1; }
    size_t write(const uint8_t* buf, size_t n) override {
        sent.append((const char*)buf, n);
        return n;
    }
    size_t write(uint8_t b) override { sent += (char)b; return 1; }
    int parsePacket() override { return parsePacketResult; }
    int read(uint8_t* buf, size_t n) override {
        const size_t c = (n < responseLen) ? n : responseLen;
        memcpy(buf, response.data(), c);
        responseLen = 0; // one-shot
        return (int)c;
    }
    int read() override { return -1; }
    int available() override { return (int)responseLen; }
    int peek() override { return -1; }
    IPAddress remoteIP() override { return IPAddress(192, 168, 0, 1); }
    uint16_t remotePort() override { return 123; }
};

static void setNtpResponse(FakeUdp& udp, uint8_t firstByte, unsigned long epoch) {
    uint8_t packet[48] = {0};
    packet[0] = firstByte;
    const unsigned long seconds = epoch + NtpSync::kNtpEpochOffset;
    packet[40] = (uint8_t)(seconds >> 24);
    packet[41] = (uint8_t)(seconds >> 16);
    packet[42] = (uint8_t)(seconds >> 8);
    packet[43] = (uint8_t)(seconds);
    udp.response.assign((const char*)packet, sizeof(packet));
    udp.responseLen = sizeof(packet);
    udp.parsePacketResult = 48;
}

// Fake epoch clock: stands in for the SAMD21 RTC.
class FakeEpochClock : public IEpochClock {
public:
    unsigned long epoch() override { return epoch_; }
    void setEpoch(unsigned long epochSeconds) override {
        epoch_ = epochSeconds;
        set_ = true;
    }
    bool isSet() const override { return set_; }
    void advanceSeconds(unsigned long s) { epoch_ += s; }

    unsigned long epoch_ = 0;
    bool set_ = false;
};

void setUp(void) { stubs::resetStubs(); }
void tearDown(void) {}

// --- NtpSync::epochFromResponse ---

void test_valid_server_response(void) {
    uint8_t packet[48] = {0};
    packet[0] = 0x24; // LI=0, Version=4, Mode=4 (server)
    const unsigned long seconds = 1000000000UL + NtpSync::kNtpEpochOffset;
    packet[40] = (uint8_t)(seconds >> 24);
    packet[41] = (uint8_t)(seconds >> 16);
    packet[42] = (uint8_t)(seconds >> 8);
    packet[43] = (uint8_t)(seconds);
    unsigned long epoch = 0;
    TEST_ASSERT_TRUE(NtpSync::epochFromResponse(packet, sizeof(packet), epoch));
    TEST_ASSERT_EQUAL_UINT32(1000000000UL, epoch);
}

void test_broadcast_mode_is_accepted(void) {
    uint8_t packet[48] = {0};
    packet[0] = 0x25; // Mode=5 (broadcast)
    const unsigned long seconds = 1000000000UL + NtpSync::kNtpEpochOffset;
    packet[40] = (uint8_t)(seconds >> 24);
    packet[41] = (uint8_t)(seconds >> 16);
    packet[42] = (uint8_t)(seconds >> 8);
    packet[43] = (uint8_t)(seconds);
    unsigned long epoch = 0;
    TEST_ASSERT_TRUE(NtpSync::epochFromResponse(packet, sizeof(packet), epoch));
    TEST_ASSERT_EQUAL_UINT32(1000000000UL, epoch);
}

void test_leap_alarm_is_rejected(void) {
    uint8_t packet[48] = {0};
    packet[0] = 0xE4; // LI=3: clock unsynchronized
    unsigned long epoch = 0;
    TEST_ASSERT_FALSE(NtpSync::epochFromResponse(packet, sizeof(packet), epoch));
}

void test_client_mode_is_rejected(void) {
    uint8_t packet[48] = {0};
    packet[0] = 0x23; // Mode=3 (client): not a server answer
    unsigned long epoch = 0;
    TEST_ASSERT_FALSE(NtpSync::epochFromResponse(packet, sizeof(packet), epoch));
}

void test_short_packet_is_rejected(void) {
    uint8_t packet[47] = {0};
    unsigned long epoch = 0;
    TEST_ASSERT_FALSE(NtpSync::epochFromResponse(packet, sizeof(packet), epoch));
    TEST_ASSERT_FALSE(NtpSync::epochFromResponse(nullptr, 48, epoch));
}

void test_pre_epoch_timestamp_is_rejected(void) {
    uint8_t packet[48] = {0};
    packet[0] = 0x24;
    packet[43] = 0x10; // seconds < 1900 offset -> before 1970
    unsigned long epoch = 0;
    TEST_ASSERT_FALSE(NtpSync::epochFromResponse(packet, sizeof(packet), epoch));
}

// --- NtpSync::sync ---

void test_sync_success(void) {
    FakeUdp udp;
    setNtpResponse(udp, 0x24, 1000000000UL);
    unsigned long epoch = 0;
    TEST_ASSERT_TRUE(NtpSync::sync(udp, "pool.ntp.org", epoch, 1000));
    TEST_ASSERT_EQUAL_UINT32(1000000000UL, epoch);
    // Request: 48 bytes, mode 3 client, sent to the right host.
    TEST_ASSERT_EQUAL_UINT32(48, udp.sent.size());
    TEST_ASSERT_EQUAL_UINT8(0x23, (uint8_t)udp.sent[0]);
    TEST_ASSERT_EQUAL_STRING("pool.ntp.org", udp.lastHost.c_str());
    TEST_ASSERT_EQUAL_INT(1, udp.stopCalls); // socket always closed
}

void test_sync_timeout(void) {
    FakeUdp udp;
    udp.parsePacketResult = 0; // no answer ever
    unsigned long epoch = 0;
    TEST_ASSERT_FALSE(NtpSync::sync(udp, "pool.ntp.org", epoch, 100));
    TEST_ASSERT_EQUAL_INT(1, udp.stopCalls);
}

void test_sync_invalid_response(void) {
    FakeUdp udp;
    setNtpResponse(udp, 0x23, 1000000000UL); // client mode: invalid
    unsigned long epoch = 0;
    TEST_ASSERT_FALSE(NtpSync::sync(udp, "pool.ntp.org", epoch, 100));
}

void test_sync_bad_input(void) {
    FakeUdp udp;
    unsigned long epoch = 0;
    TEST_ASSERT_FALSE(NtpSync::sync(udp, nullptr, epoch, 100));
    TEST_ASSERT_FALSE(NtpSync::sync(udp, "", epoch, 100));
    udp.beginResult = 0; // socket bind failure
    TEST_ASSERT_FALSE(NtpSync::sync(udp, "pool.ntp.org", epoch, 100));
    TEST_ASSERT_EQUAL_INT(0, udp.stopCalls); // never opened: no stop needed
}

// --- TimeManager (RTC-backed) ---

void test_unsynced_defaults(void) {
    FakeEpochClock clock;
    TimeManager tm(clock, 0);
    TEST_ASSERT_FALSE(tm.isSynced());
    TEST_ASSERT_EQUAL_INT(-1, tm.currentHour());
    TEST_ASSERT_FALSE(tm.isNight(21)); // no time: never sleep (safety)
    // First attempt is unconditional.
    TEST_ASSERT_TRUE(tm.shouldAttemptSync(86400, 1800000));
}

void test_hour_and_night_boundaries(void) {
    FakeEpochClock clock;
    TimeManager tm(clock, 0);
    tm.onNtpSync(22UL * 3600UL); // 22:00 local
    TEST_ASSERT_EQUAL_INT(22, tm.currentHour());
    TEST_ASSERT_TRUE(tm.isNight(21));
    // Exactly 21:00: night (inclusive bound).
    tm.onNtpSync(21UL * 3600UL);
    TEST_ASSERT_EQUAL_INT(21, tm.currentHour());
    TEST_ASSERT_TRUE(tm.isNight(21));
    // 20:59:59: not night yet.
    tm.onNtpSync(21UL * 3600UL - 1UL);
    TEST_ASSERT_EQUAL_INT(20, tm.currentHour());
    TEST_ASSERT_FALSE(tm.isNight(21));
    // 23:59:59: still night.
    tm.onNtpSync(24UL * 3600UL - 1UL);
    TEST_ASSERT_EQUAL_INT(23, tm.currentHour());
    TEST_ASSERT_TRUE(tm.isNight(21));
}

void test_night_guard_against_misconfiguration(void) {
    FakeEpochClock clock;
    TimeManager tm(clock, 0);
    tm.onNtpSync(23UL * 3600UL); // 23:00: any sane hour check would sleep
    TEST_ASSERT_FALSE(tm.isNight(0));   // would sleep forever
    TEST_ASSERT_FALSE(tm.isNight(24));  // never matches
    TEST_ASSERT_FALSE(tm.isNight(-1));  // invalid
    TEST_ASSERT_TRUE(tm.isNight(23));
}

void test_utc_offset_applies(void) {
    FakeEpochClock clock;
    TimeManager tm(clock, 3600); // UTC+1
    tm.onNtpSync(0); // midnight UTC -> 01:00 local
    TEST_ASSERT_EQUAL_INT(1, tm.currentHour());

    FakeEpochClock clock2;
    TimeManager tm2(clock2, -3600); // UTC-1
    tm2.onNtpSync(3600); // 01:00 UTC -> midnight local
    TEST_ASSERT_EQUAL_INT(0, tm2.currentHour());
}

void test_hour_advances_with_rtc_across_deep_sleep(void) {
    // The RTC keeps running while the CPU sleeps: an hour that starts at
    // 20:00 becomes night after one hour of sleep.
    FakeEpochClock clock;
    TimeManager tm(clock, 0);
    tm.onNtpSync(20UL * 3600UL); // 20:00 local
    TEST_ASSERT_FALSE(tm.isNight(21));
    clock.advanceSeconds(3600);
    TEST_ASSERT_EQUAL_INT(21, tm.currentHour());
    TEST_ASSERT_TRUE(tm.isNight(21));
}

void test_rtc_epoch_updated_on_sync(void) {
    FakeEpochClock clock;
    TimeManager tm(clock, 0);
    tm.onNtpSync(1000000000UL);
    TEST_ASSERT_TRUE(clock.isSet());
    TEST_ASSERT_EQUAL_UINT32(1000000000UL, clock.epoch());
}

void test_resync_rate_limits_when_synced(void) {
    FakeEpochClock clock;
    TimeManager tm(clock, 0);
    tm.onNtpSync(1000000000UL);
    // Regular interval is epoch-based (deep-sleep-proof).
    TEST_ASSERT_FALSE(tm.shouldAttemptSync(86400, 1800000));
    clock.advanceSeconds(86399);
    TEST_ASSERT_FALSE(tm.shouldAttemptSync(86400, 1800000));
    clock.advanceSeconds(1);
    TEST_ASSERT_TRUE(tm.shouldAttemptSync(86400, 1800000));
}

void test_resync_rate_limits_when_unsynced(void) {
    FakeEpochClock clock;
    TimeManager tm(clock, 0);
    tm.markSyncAttempt();
    // Retry interval is millis-based while no RTC time exists.
    TEST_ASSERT_FALSE(tm.shouldAttemptSync(86400, 1800000));
    stubs::advanceMillis(1800000);
    TEST_ASSERT_TRUE(tm.shouldAttemptSync(86400, 1800000));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_valid_server_response);
    RUN_TEST(test_broadcast_mode_is_accepted);
    RUN_TEST(test_leap_alarm_is_rejected);
    RUN_TEST(test_client_mode_is_rejected);
    RUN_TEST(test_short_packet_is_rejected);
    RUN_TEST(test_pre_epoch_timestamp_is_rejected);
    RUN_TEST(test_sync_success);
    RUN_TEST(test_sync_timeout);
    RUN_TEST(test_sync_invalid_response);
    RUN_TEST(test_sync_bad_input);
    RUN_TEST(test_unsynced_defaults);
    RUN_TEST(test_hour_and_night_boundaries);
    RUN_TEST(test_night_guard_against_misconfiguration);
    RUN_TEST(test_utc_offset_applies);
    RUN_TEST(test_hour_advances_with_rtc_across_deep_sleep);
    RUN_TEST(test_rtc_epoch_updated_on_sync);
    RUN_TEST(test_resync_rate_limits_when_synced);
    RUN_TEST(test_resync_rate_limits_when_unsynced);
    return UNITY_END();
}
