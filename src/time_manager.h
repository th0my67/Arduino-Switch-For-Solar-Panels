#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>

// Wall-clock source. On the target board this is the SAMD21 RTC, which
// keeps running through LowPower.deepSleep() (unlike millis(), which
// freezes while the CPU sleeps). Implementations for tests are trivial.
class IEpochClock {
public:
    virtual ~IEpochClock() = default;
    virtual unsigned long epoch() = 0;              // Unix seconds
    virtual void setEpoch(unsigned long epochSeconds) = 0;
    virtual bool isSet() const = 0;                  // valid since boot
};

// Tracks wall-clock time from NTP synchronizations. Hours and night
// detection come from the epoch clock (RTC), so they survive deep sleep.
// Sync-attempt rate limiting uses millis() when unsynchronized (no RTC
// time available yet) and the epoch clock once synchronized.
class TimeManager {
public:
    TimeManager(IEpochClock& clock, long utcOffsetSeconds)
        : clock_(clock), utcOffsetSeconds_(utcOffsetSeconds) {}

    // Called after each successful NTP synchronization.
    void onNtpSync(unsigned long epochSeconds);
    // Called after each NTP attempt (success or failure).
    void markSyncAttempt() { lastAttemptMs_ = millis(); attempted_ = true; }

    bool isSynced() const { return synced_; }
    int currentHour() const;               // 0..23 local, -1 when unsynced
    // Local time as Unix epoch seconds (already shifted by the UTC
    // offset); 0 when unsynchronized. The domain used by the backup
    // charge feature, robust across deep sleeps.
    unsigned long localEpoch() const;
    bool isNight(int sleepStartHour) const; // true from sleepStartHour to midnight

    // Rate-limits NTP attempts. Once synced: epoch-based (wall time,
    // deep-sleep-proof). While unsynced: millis-based.
    bool shouldAttemptSync(uint32_t regularIntervalS,
                           uint32_t retryIntervalMs) const;

private:
    IEpochClock& clock_;
    long utcOffsetSeconds_;
    bool synced_ = false;
    unsigned long lastSyncEpoch_ = 0;
    unsigned long lastAttemptMs_ = 0;
    bool attempted_ = false;
};

// Minimal SNTP client: builds a 48-byte request, reads one response.
// The response parsing is pure and unit-testable.
class NtpSync {
public:
    static const size_t kPacketSize = 48;
    static const uint16_t kDefaultPort = 123;
    static const unsigned long kNtpEpochOffset = 2208988800UL; // 1900 -> 1970

    // Validates a response packet and extracts the Unix epoch.
    // Rejects: short packets, non-server modes, leap-alarm (unsynchronized)
    // servers, and timestamps before 1970.
    static bool epochFromResponse(const uint8_t* packet, size_t len,
                                  unsigned long& epochOut);

    // Sends one request and waits up to timeoutMs for the answer.
    // Returns false on timeout or invalid response.
    static bool sync(UDP& udp, const char* server,
                     unsigned long& epochOut,
                     uint32_t timeoutMs = 5000,
                     uint16_t serverPort = kDefaultPort,
                     uint16_t localPort = 8888);
};

#endif
