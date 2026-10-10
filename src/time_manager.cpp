#include "time_manager.h"
#include <cstring>

void TimeManager::onNtpSync(unsigned long epochSeconds) {
    clock_.setEpoch(epochSeconds);
    lastSyncEpoch_ = epochSeconds;
    synced_ = true;
    markSyncAttempt();
}

int TimeManager::currentHour() const {
    if (!synced_) return -1;
    const unsigned long localSeconds =
        clock_.epoch() + (unsigned long)utcOffsetSeconds_;
    return (int)((localSeconds / 3600UL) % 24UL);
}

bool TimeManager::isNight(int sleepStartHour) const {
    // No reliable time: never enter night sleep (functional safety first).
    if (!synced_) return false;
    // Guard against a misconfigured hour (0 would sleep forever,
    // >= 24 never matches).
    if (sleepStartHour < 1 || sleepStartHour > 23) return false;
    return currentHour() >= sleepStartHour;
}

bool TimeManager::shouldAttemptSync(uint32_t regularIntervalS,
                                    uint32_t retryIntervalMs) const {
    if (!attempted_) return true; // first attempt at boot is unconditional
    if (synced_) {
        // Wall-clock based: correct across deep sleeps.
        const unsigned long elapsedS = clock_.epoch() - lastSyncEpoch_;
        return elapsedS >= regularIntervalS;
    }
    // millis()-based retry while no RTC time is available. Deep sleeps
    // freeze millis(), which only delays retries, never spams them.
    return (millis() - lastAttemptMs_) >= retryIntervalMs;
}

bool NtpSync::epochFromResponse(const uint8_t* packet, size_t len,
                                unsigned long& epochOut) {
    if (packet == nullptr || len < kPacketSize) return false;

    const uint8_t firstByte = packet[0];
    const uint8_t leapIndicator = firstByte >> 6;         // LI
    const uint8_t mode = firstByte & 0x07;                // mode
    const bool serverMode = (mode == 4) || (mode == 5);   // server/broadcast
    if (leapIndicator == 3 || !serverMode) return false;

    const unsigned long seconds =
        ((unsigned long)packet[40] << 24) |
        ((unsigned long)packet[41] << 16) |
        ((unsigned long)packet[42] << 8) |
        (unsigned long)packet[43];

    if (seconds < kNtpEpochOffset) return false; // before 1970
    epochOut = seconds - kNtpEpochOffset;
    return true;
}

bool NtpSync::sync(UDP& udp, const char* server,
                   unsigned long& epochOut,
                   uint32_t timeoutMs,
                   uint16_t serverPort,
                   uint16_t localPort) {
    epochOut = 0;
    if (server == nullptr || server[0] == '\0') return false;
    if (udp.begin(localPort) == 0) return false;

    uint8_t request[kPacketSize] = {0};
    request[0] = 0x23; // LI=0, Version=4, Mode=3 (client)

    bool sent = udp.beginPacket(server, serverPort) != 0;
    if (sent) {
        udp.write(request, kPacketSize);
        sent = udp.endPacket() != 0;
    }
    if (!sent) {
        udp.stop();
        return false;
    }

    int packetSize = 0;
    const unsigned long deadline = millis() + timeoutMs;
    while ((long)(deadline - millis()) > 0) {
        packetSize = udp.parsePacket();
        if (packetSize >= (int)kPacketSize) break;
        delay(2);
    }

    bool ok = false;
    if (packetSize >= (int)kPacketSize) {
        uint8_t response[kPacketSize];
        memset(response, 0, sizeof(response));
        if (udp.read(response, kPacketSize) > 0) {
            ok = epochFromResponse(response, kPacketSize, epochOut);
        }
    }

    udp.stop();
    return ok;
}
