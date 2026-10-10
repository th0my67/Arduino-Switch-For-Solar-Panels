#ifndef SWITCH_CONTROLLER_H
#define SWITCH_CONTROLLER_H

#include "services/power_monitor.h"
#include "utils/result.h"
#include <Arduino.h>

// Drives the boiler switch from power readings, with:
//  - hysteresis: ON at or above onThresholdW, OFF at or below offThresholdW,
//    hold state in between (prevents relay chatter around the threshold);
//  - minimum ON/OFF durations (protects the relay and the boiler);
//  - fail-safe: after N consecutive failed readings the switch is forced
//    OFF so the boiler never heats from grid power unknowingly.
//
// Time is injected at each call (nowMs), NOT read from millis(): on the
// target board millis() freezes during LowPower.deepSleep(), so wall
// time comes from the RTC (epoch * 1000). The caller must stay in a
// single time domain; arithmetic is modular and rollover-safe.
class SwitchController {
public:
    SwitchController(uint8_t pin,
                     int onThresholdW,
                     int offThresholdW,
                     uint32_t minOnTimeMs,
                     uint32_t minOffTimeMs,
                     uint8_t consecutiveFailuresBeforeOff);

    void begin(uint32_t nowMs); // configure pin as output, switch OFF
    void applyReading(const Result<int, PowerError>& reading, uint32_t nowMs);
    void forceOff(uint32_t nowMs);

    bool isOn() const { return on_; }
    uint32_t timeInCurrentStateMs(uint32_t nowMs) const;
    uint8_t consecutiveFailures() const { return consecutiveFailures_; }

private:
    void setOutput(bool on, uint32_t nowMs);

    uint8_t pin_;
    int onThresholdW_;
    int offThresholdW_;
    uint32_t minOnTimeMs_;
    uint32_t minOffTimeMs_;
    uint8_t failureLimit_;
    bool on_ = false;
    uint32_t lastChangeMs_ = 0;
    uint8_t consecutiveFailures_ = 0;
};

#endif
